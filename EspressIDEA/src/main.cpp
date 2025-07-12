#include "ServerManager.hpp"
#include "WsHandlers.hpp"
#include "EspressIDEA.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

static const char* TAG = "main";

EspressIDEA* espIDEA = nullptr;

void registerAllWebSocketRoutes(ServerManager& server) {
    using namespace WsRoutes;

    const httpd_uri_t* routes[] = {
        &run, &stop, &fs_cat, &fs_upload, &fs_download, &fs_list, &firmware
    };

    for (auto r : routes) {
        server.registerWebSocketHandler(*r);
        ESP_LOGI(TAG, "WebSocket registrado: %s", r->uri);
    }
}

extern "C" void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());

    Pyboard py(UART_NUM_2, 21, 22, 115200);
    espIDEA = new EspressIDEA(std::move(py));
    espIDEA->begin();
    
    ServerManager server("/spiffs/CREDENTIALS.txt");
    server.begin();
    registerAllWebSocketRoutes(server);
}
