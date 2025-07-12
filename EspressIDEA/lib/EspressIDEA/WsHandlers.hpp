#pragma once

#include "EspressIDEA.hpp"
#include <esp_http_server.h>
#include <ArduinoJson.h>

class WsHandlers {
public:
    // Funciones para parsear y enviar mensajes WebSocket
    static bool parse_json(httpd_req_t* req, JsonDocument& doc);
    static esp_err_t send_json(httpd_req_t* req, const JsonDocument& doc);

    // Handlers individuales para cada WebSocket
    static esp_err_t handle_run(httpd_req_t* req);
    static esp_err_t handle_stop(httpd_req_t* req);
    static esp_err_t handle_fs_cat(httpd_req_t* req);
    static esp_err_t handle_fs_upload(httpd_req_t* req);
    static esp_err_t handle_fs_download(httpd_req_t* req);
    static esp_err_t handle_fs_list(httpd_req_t* req);
    static esp_err_t handle_firmware(httpd_req_t* req);

private:
    static EspressIDEA& get_ide();
};

// Declaraciones de rutas WebSocket
namespace WsRoutes {
    extern const httpd_uri_t run;
    extern const httpd_uri_t stop;
    extern const httpd_uri_t fs_cat;
    extern const httpd_uri_t fs_upload;
    extern const httpd_uri_t fs_download;
    extern const httpd_uri_t fs_list;
    extern const httpd_uri_t firmware;
}

// Referencia global al IDE
extern EspressIDEA* espIDEA;
