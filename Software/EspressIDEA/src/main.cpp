// src/main.cpp
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "ServerManager.hpp"
#include "PyBoardUART.hpp"
#include <EspressIDEA/EspressIDEA.hpp>

static const char* TAG = "EspressIDEA-main";

static ServerManager server("/spiffs/CREDENTIALS.txt");
static PyBoard::PyBoardUART board(
  UART_NUM_2, 21, 22,
  PyBoard::BaudRate::BAUD_115200,
  PyBoard::Timeout::MEDIUM,
  PyBoard::ChunkSize::MEDIUM
);

extern "C" void app_main() {
  ESP_ERROR_CHECK(nvs_flash_init());

  if (board.init() != PyBoard::ErrorCode::OK) {
    ESP_LOGE(TAG, "PyBoardUART init failed: %s", board.getLastError().c_str());
    return;
  }

  server.begin();

  static EspressIDEA::Device device(board, server);
  device.begin();

  ESP_LOGI(TAG, "EspressIDEA listo.");
}
