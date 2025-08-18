#include <cstring>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "esp_http_server.h"
#include "driver/uart.h"

#include "ServerManager.hpp"
#include "PyBoardUART.hpp"

static const char* TAG = "ws_serial";

// ===== Config UART/REPL =====
static constexpr uart_port_t kUartNum = UART_NUM_2;
static constexpr int kPinTX = 21;
static constexpr int kPinRX = 22;
static constexpr size_t kUartRxTxBuf = 4096;
static constexpr int kPumpChunk   = 512;
static constexpr TickType_t kReadWait = pdMS_TO_TICKS(20);

// ===== Instancias =====
static ServerManager server("/spiffs/CREDENTIALS.txt");
static PyBoard::PyBoardUART board(
    kUartNum, kPinTX, kPinRX,
    PyBoard::BaudRate::BAUD_115200,
    PyBoard::Timeout::MEDIUM,
    PyBoard::ChunkSize::MEDIUM
);

// ===== Estado de sesión WebSocket =====
struct WsSerialSession {
    httpd_handle_t server = nullptr;
    int sockfd = -1;
    TaskHandle_t pump_task = nullptr;
    bool active = false;
} g_session;

// ===== Utilidades envío WS =====
static inline bool ws_send_async_binary(httpd_handle_t server, int sockfd, const uint8_t* data, size_t len) {
    if (!server || sockfd < 0) return false;
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_BINARY;   // usa TEXT si prefieres texto
    frame.payload = const_cast<uint8_t*>(data);
    frame.len = len;
    return httpd_ws_send_frame_async(server, sockfd, &frame) == ESP_OK;
}

static inline bool ws_send_async_text(httpd_handle_t server, int sockfd, const char* s) {
    return ws_send_async_binary(server, sockfd, (const uint8_t*)s, strlen(s));
}

// ===== Tarea UART -> WS =====
static void uart_to_ws_pump(void* arg) {
    ESP_LOGI(TAG, "Pump UART->WS iniciado");
    std::vector<uint8_t> buf(kPumpChunk);

    while (true) {
        if (!g_session.active || !g_session.server || g_session.sockfd < 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Lectura cruda desde el UART (mientras PyBoardUART no exponga lectura pública)
        int n = uart_read_bytes(kUartNum, buf.data(), buf.size(), kReadWait);
        if (n > 0) {
            if (!ws_send_async_binary(g_session.server, g_session.sockfd, buf.data(), (size_t)n)) {
                ESP_LOGW(TAG, "WS envío falló; desactivando sesión");
                g_session.active = false;
                g_session.sockfd = -1;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

// ===== Handler WS único (/ws/serial) =====
static esp_err_t ws_serial_handler(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        // Handshake WS (upgrade)
        int sockfd = httpd_req_to_sockfd(req);

        if (g_session.active) {
            // Rechaza segunda sesión (terminal "seamless")
            httpd_ws_frame_t closef = {};
            closef.type = HTTPD_WS_TYPE_CLOSE;
            httpd_ws_send_frame_async(req->handle, sockfd, &closef);
            return ESP_FAIL;
        }

        g_session.server = req->handle;
        g_session.sockfd = sockfd;
        g_session.active = true;

        if (!g_session.pump_task) {
            xTaskCreate(uart_to_ws_pump, "uart_ws_pump", 4096, nullptr, 10, &g_session.pump_task);
        }

        ws_send_async_text(req->handle, sockfd, ">> REPL listo (WS conectado)\r\n");

        // Preparar el REPL: Ctrl-C, opcional soft reset
        board.interrupt();
        vTaskDelay(pdMS_TO_TICKS(100));
        // board.softReset();
        // vTaskDelay(pdMS_TO_TICKS(100));
        // board.interrupt();

        return ESP_OK;
    }

    // Frame de datos/control
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_BINARY; // aceptamos binario por defecto
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
        ESP_LOGI(TAG, "WS CLOSE recibido");
        g_session.active = false;
        g_session.sockfd = -1;
        return ESP_OK;
    }

    // Escritura hacia el REPL usando PyBoardUART
    if (!payload.empty()) {
        auto err = board.write(payload.data(), payload.size());
        if (err != PyBoard::ErrorCode::OK) {
            ESP_LOGW(TAG, "board.write falló: %s", board.getLastError().c_str());
        }
    }

    return ESP_OK;
}

// ===== Descriptor URI WS =====
static httpd_uri_t ws_serial = {
    .uri = "/ws/serial",
    .method = HTTP_GET,
    .handler = ws_serial_handler,
    .user_ctx = nullptr,
    .is_websocket = true,
    .handle_ws_control_frames = true,
    .supported_subprotocol = nullptr
};

// ===== app_main =====
extern "C" void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());

    if (board.init() != PyBoard::ErrorCode::OK) {
        ESP_LOGE(TAG, "PyBoardUART init failed: %s", board.getLastError().c_str());
        return;
    }

    // Asegurarnos de tener driver UART instalado (por si init() no lo hizo)
    if (!uart_is_driver_installed(kUartNum)) {
        // Instalar sólo buffers; no tocamos pins/baud para no interferir con PyBoardUART
        ESP_ERROR_CHECK(uart_driver_install(kUartNum, kUartRxTxBuf, kUartRxTxBuf, 0, nullptr, 0));
    }

    server.begin();
    server.registerWebSocketHandler(ws_serial);

    ESP_LOGI(TAG, "WS listo en /ws/serial");

    // Limpieza inicial del REPL (opcional)
    board.interrupt();
    vTaskDelay(pdMS_TO_TICKS(100));
    board.softReset();
    vTaskDelay(pdMS_TO_TICKS(100));
    board.interrupt();
    vTaskDelay(pdMS_TO_TICKS(100));
}
