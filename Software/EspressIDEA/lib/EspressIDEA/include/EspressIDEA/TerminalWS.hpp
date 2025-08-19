#pragma once
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "driver/uart.h"

#include "ServerManager.hpp"
#include "PyBoardUART.hpp"
#include "EspressIDEA/ReplControl.hpp"

namespace EspressIDEA {

/**
 * TerminalWS
 *  - Lector UART -> StreamBuffer (backpressure, coalescing)
 *  - Emisor WS (pacing con async)
 *  - Handler WS con user_ctx=this (contexto correcto)
 *  - Respeta ReplControl (solo TERMINAL)
 */
class TerminalWS {
public:
  TerminalWS(PyBoard::PyBoardUART& board, ReplControl& repl, ServerManager& server);
  void registerRoutes(); // registra /ws/serial

private:
  // Config
  static constexpr uart_port_t kUartNum   = UART_NUM_2;
  static constexpr size_t      kUartBuf   = 4096;
  static constexpr int         kReadChunk = 512;
  static constexpr TickType_t  kReadWait  = pdMS_TO_TICKS(20);

  // Backpressure & pacing
  static constexpr size_t      kStreamBufSize = 16 * 1024; // 16 KB
  static constexpr size_t      kSendChunkMax  = 1024;      // 1 KB por frame
  static constexpr TickType_t  kMinFrameGap   = pdMS_TO_TICKS(16); // ~60 fps

  // Dependencias
  PyBoard::PyBoardUART& board_;
  ReplControl& repl_;
  ServerManager& server_;

  // Estado de sesión
  httpd_handle_t httpd_ = nullptr;
  int            sockfd_ = -1;
  volatile bool  active_ = false;

  // Buffer y tareas
  StreamBufferHandle_t sbuf_ = nullptr;
  TaskHandle_t         tReader_ = nullptr;
  TaskHandle_t         tSender_ = nullptr;

  // ---- Handlers/Tasks ----
  static esp_err_t wsHandler(httpd_req_t* req);
  static void uartReaderTask(void* arg);
  static void wsSenderTask(void* arg);

  // helpers
  void startTasksIfNeeded();
  bool queueSend(const uint8_t* data, size_t len);
  bool queueSendText(const char* s);
  bool isReady() const { return active_ && httpd_ && sockfd_ >= 0; }
};

} // namespace EspressIDEA
