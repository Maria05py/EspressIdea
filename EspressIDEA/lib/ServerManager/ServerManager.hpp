#pragma once

#include <string>
#include <functional>
#include <esp_err.h>
#include "esp_http_server.h"

class ServerManager {
public:
    ServerManager(const char* credentialsPath = "/spiffs/CREDENTIALS.txt");

    void begin();
    void registerHttpHandler(const httpd_uri_t& handler);
    void registerWebSocketHandler(const httpd_uri_t& handler);

    std::string getSSID() const;
    std::string getPass() const;
    std::string getHostname() const;

private:
    std::string wifi_ssid;
    std::string wifi_pass;
    std::string mdns_hostname;

    httpd_handle_t http_server = nullptr;

    void initSpiffs();
    void loadCredentials(const char* path);
    void initWifi();
    void initMdns();
    void initHttp();
    void registerAllFiles(const char* base);
    static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data);
};
