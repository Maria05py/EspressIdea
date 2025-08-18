#pragma once

#include <string>
#include "esp_http_server.h"

class ServerManager {
public:
    explicit ServerManager(const char* credentialsPath);
    void begin();

    void registerHttpHandler(const httpd_uri_t& handler);
    void registerWebSocketHandler(const httpd_uri_t& handler);
    void broadcastWS(const std::string& data);

    std::string getSSID() const;
    std::string getPass() const;
    std::string getHostname() const;

    static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data);

private:
    void initSpiffs();
    void loadCredentials(const char* path);
    void initNvs();     // <-- NUEVO
    void initWifi();
    void initMdns();
    void initHttp();
    void registerAllFiles(const char* base);

    httpd_handle_t http_server = nullptr;

    std::string wifi_ssid;
    std::string wifi_pass;
    std::string mdns_hostname = "espressidea";
};
