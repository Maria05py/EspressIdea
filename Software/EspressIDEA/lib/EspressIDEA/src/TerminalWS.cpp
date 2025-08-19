#include "EspressIDEA/TerminalWS.hpp"
#include "esp_log.h"
#include <cstring>
#include <vector>

using namespace EspressIDEA;
static const char* TAG = "TerminalWS";

TerminalWS::TerminalWS(PyBoard::PyBoardUART& board, ReplControl& repl, ServerManager& server)
: board_(board), repl_(repl), server_(server) {
  // StreamBuffer: tamaño + trigger level (>=1)
  sbuf_ = xStreamBufferCreate(kStreamBufSize, 1);
}

void TerminalWS::registerRoutes() {
  // Asegurar driver para lectura cruda (como versión funcional original)
  if (!uart_is_driver_installed(kUartNum)) {
    ESP_ERROR_CHECK(uart_driver_install(kUartNum, kUartBuf, kUartBuf, 0, nullptr, 0));
  }

  httpd_uri_t ws_serial = {
    .uri = "/ws/serial",
    .method = HTTP_GET,
    .handler = TerminalWS::wsHandler,
    .user_ctx = this,
    .is_websocket = true,
    .handle_ws_control_frames = true,
    .supported_subprotocol = nullptr
  };
  server_.registerWebSocketHandler(ws_serial);
  ESP_LOGI(TAG, "Ruta WebSocket /ws/serial registrada");
}

void TerminalWS::startTasksIfNeeded() {
  if (!tReader_) xTaskCreate(uartReaderTask, "tty_uart_reader", 4096, this, 10, &tReader_);
  if (!tSender_) xTaskCreate(wsSenderTask,   "tty_ws_sender",  4096, this, 10, &tSender_);
}

bool TerminalWS::queueSend(const uint8_t* data, size_t len) {
  if (!sbuf_ || !data || len == 0) return false;
  // Política TTY: no bloquear; si se llena, dropea exceso
  size_t sent = xStreamBufferSend(sbuf_, data, len, 0);
  return sent > 0;
}
bool TerminalWS::queueSendText(const char* s) {
  return s ? queueSend(reinterpret_cast<const uint8_t*>(s), strlen(s)) : false;
}

// --------- Tarea: UART -> StreamBuffer -----------
void TerminalWS::uartReaderTask(void* arg) {
  auto* self = static_cast<TerminalWS*>(arg);
  std::vector<uint8_t> buf(kReadChunk);

  ESP_LOGI(TAG, "UART reader iniciado");
  for (;;) {
    if (!self->active_) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

    // Solo en modo TERMINAL y sin bloquear si hay CONTROLADO
    if (self->repl_.mode() != ReplMode::TERMINAL ||
        !self->repl_.tryLockFromTerminal()) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    int n = uart_read_bytes(kUartNum, buf.data(), buf.size(), self->kReadWait);
    self->repl_.unlockFromTerminal();

    if (n > 0) {
      (void) self->queueSend(buf.data(), (size_t)n);
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

// --------- Tarea: StreamBuffer -> WS (async + pacing) ----------
void TerminalWS::wsSenderTask(void* arg) {
  auto* self = static_cast<TerminalWS*>(arg);
  std::vector<uint8_t> chunk(kSendChunkMax);

  ESP_LOGI(TAG, "WS sender iniciado");
  for (;;) {
    if (!self->isReady()) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

    // Espera hasta que haya algo en buffer (timeout para poder salir si se desconecta)
    size_t n = xStreamBufferReceive(self->sbuf_, chunk.data(), chunk.size(), pdMS_TO_TICKS(50));
    if (n == 0) continue;

    // Coalescing rápido: intenta completar chunk si hay más datos ya listos
    size_t freeSpace = chunk.size() - n;
    if (freeSpace > 0) {
      size_t m = xStreamBufferReceive(self->sbuf_, chunk.data() + n, freeSpace, 0);
      n += m;
    }

    // Envío asíncrono (desde esta tarea)
    httpd_ws_frame_t f = {};
    f.type = HTTPD_WS_TYPE_BINARY;
    f.payload = chunk.data();
    f.len = n;

    // Nota: asumimos que httpd_ws_send_frame_async copia el payload internamente
    // (comportamiento estándar del server para envíos async). El pacing limita saturación.
    if (httpd_ws_send_frame_async(self->httpd_, self->sockfd_, &f) != ESP_OK) {
      // Si la cola está llena, espera un poco y reintenta en el próximo ciclo
      vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      vTaskDelay(self->kMinFrameGap); // pacing ~60fps
    }
  }
}

// ----------------- WS Handler ---------------------
esp_err_t TerminalWS::wsHandler(httpd_req_t* req) {
  auto* self = static_cast<TerminalWS*>(req->user_ctx);
  if (!self) return ESP_FAIL;

  if (req->method == HTTP_GET) {
    // Upgrade: NO enviar frames aquí
    int sockfd = httpd_req_to_sockfd(req);

    if (self->active_) {
      httpd_ws_frame_t closef = {};
      closef.type = HTTPD_WS_TYPE_CLOSE;
      httpd_ws_send_frame_async(req->handle, sockfd, &closef);
      return ESP_FAIL;
    }

    self->httpd_ = req->handle;
    self->sockfd_ = sockfd;
    self->active_ = true;

    self->repl_.setModeTerminal();
    self->startTasksIfNeeded();

    // Saludo por el pipeline (se enviará por la tarea de envío)
    self->queueSendText(">> REPL listo (WS conectado)\r\n");
    return ESP_OK;
  }

  // Tráfico WS (datos/control)
  httpd_ws_frame_t frame = {};
  frame.type = HTTPD_WS_TYPE_BINARY;
  frame.payload = nullptr;

  esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
  if (ret != ESP_OK) return ret;

  std::vector<uint8_t> payload(frame.len);
  if (frame.len) {
    frame.payload = payload.data();
    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret != ESP_OK) return ret;
  }

  if (frame.type == HTTPD_WS_TYPE_CLOSE) {
    self->active_ = false;
    self->sockfd_ = -1;
    return ESP_OK;
  }

  // Solo aceptamos entradas en TERMINAL
  if (self->repl_.mode() != ReplMode::TERMINAL) {
    // Aquí estamos en hilo del server: podemos responder síncrono
    const char* busy = "[UART ocupado por operación CONTROLADA]\r\n";
    httpd_ws_frame_t f = {};
    f.type = HTTPD_WS_TYPE_TEXT;
    f.payload = (uint8_t*)busy;
    f.len = strlen(busy);
    (void) httpd_ws_send_frame(req, &f);
    return ESP_OK;
  }

  // Eco hacia UART
  if (!payload.empty()) {
    auto err = self->board_.write(payload.data(), payload.size());
    if (err != PyBoard::ErrorCode::OK) {
      ESP_LOGW(TAG, "board.write falló: %s", self->board_.getLastError().c_str());
    }
  }
  return ESP_OK;
}
