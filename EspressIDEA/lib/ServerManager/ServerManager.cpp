#include "ServerManager.hpp"
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_spiffs.h>
#include <mdns.h>
#include <dirent.h>
#include <string.h>

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;
static const char* TAG = "ServerManager";

ServerManager::ServerManager(const char* credentialsPath) {
    initSpiffs();
    loadCredentials(credentialsPath);
}

void ServerManager::begin() {
    initWifi();
    initMdns();
    initHttp();
}

void ServerManager::initSpiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

void ServerManager::loadCredentials(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open credentials at %s", path);
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SSID=", 5) == 0) wifi_ssid = line + 5;
        else if (strncmp(line, "PASS=", 5) == 0) wifi_pass = line + 5;
        else if (strncmp(line, "HOST=", 5) == 0) mdns_hostname = line + 5;
    }
    fclose(f);

    auto trim = [](std::string& s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    };
    trim(wifi_ssid);
    trim(wifi_pass);
    trim(mdns_hostname);
}

void ServerManager::initWifi() {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, NULL, NULL));

    wifi_config_t wcfg = {};
    strncpy((char*)wcfg.sta.ssid, wifi_ssid.c_str(), sizeof(wcfg.sta.ssid));
    strncpy((char*)wcfg.sta.password, wifi_pass.c_str(), sizeof(wcfg.sta.password));
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi %s...", wifi_ssid.c_str());
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected");
}

void ServerManager::wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

void ServerManager::initMdns() {
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname.c_str()));
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 REPL"));
    ESP_ERROR_CHECK(mdns_service_add("esp32-web", "_http", "_tcp", 80, NULL, 0));
}

void ServerManager::initHttp() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    // Index fallback
    httpd_uri_t index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            const char* path = "/spiffs/www/index.html";
            FILE* f = fopen(path, "r");
            if (!f) return httpd_resp_send_404(req), ESP_FAIL;

            httpd_resp_set_type(req, "text/html");
            char buf[512]; size_t r;
            while ((r = fread(buf, 1, sizeof(buf), f)) > 0)
                httpd_resp_send_chunk(req, buf, r);
            fclose(f);
            return httpd_resp_send_chunk(req, NULL, 0);
        },
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &index);

    registerAllFiles("/spiffs/www");
    ESP_LOGI(TAG, "HTTP server started");
}

void ServerManager::registerAllFiles(const char* base) {
    DIR* d = opendir(base);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;

        std::string uri = "/" + std::string(e->d_name);
        std::string path = std::string(base) + "/" + e->d_name;
        char* path_dup = strdup(path.c_str());

        httpd_uri_t h = {
            .uri = strdup(uri.c_str()),
            .method = HTTP_GET,
            .handler = [](httpd_req_t* req) -> esp_err_t {
                const char* path = (const char*)req->user_ctx;
                FILE* f = fopen(path, "r");
                if (!f) return httpd_resp_send_404(req), ESP_FAIL;

                if (strstr(path, ".css")) httpd_resp_set_type(req, "text/css");
                else if (strstr(path, ".js")) httpd_resp_set_type(req, "application/javascript");
                else httpd_resp_set_type(req, "text/html");

                char buf[512]; size_t r;
                while ((r = fread(buf, 1, sizeof(buf), f)) > 0)
                    httpd_resp_send_chunk(req, buf, r);
                fclose(f);
                return httpd_resp_send_chunk(req, NULL, 0);
            },
            .user_ctx = path_dup
        };
        httpd_register_uri_handler(http_server, &h);
    }
    closedir(d);
}

void ServerManager::registerHttpHandler(const httpd_uri_t& handler) {
    httpd_register_uri_handler(http_server, &handler);
}

void ServerManager::registerWebSocketHandler(const httpd_uri_t& handler) {
    httpd_register_uri_handler(http_server, &handler);
}

std::string ServerManager::getSSID() const { return wifi_ssid; }
std::string ServerManager::getPass() const { return wifi_pass; }
std::string ServerManager::getHostname() const { return mdns_hostname; }
