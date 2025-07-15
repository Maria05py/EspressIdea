#include "WsHandlers.hpp"
#include "EspressIDEA.hpp"

extern EspressIDEA* espIDEA;

// Helpers internos
bool WsHandlers::parse_json(httpd_req_t* req, JsonDocument& doc) {
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = nullptr,
        .len = 0
    };

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK || frame.len == 0) return false;

    std::vector<uint8_t> buffer(frame.len + 1);
    frame.payload = buffer.data();
    if (httpd_ws_recv_frame(req, &frame, frame.len) != ESP_OK) return false;

    DeserializationError err = deserializeJson(doc, buffer.data());
    return !err;
}

esp_err_t WsHandlers::send_json(httpd_req_t* req, const JsonDocument& doc) {
    std::string response;
    serializeJson(doc, response);

    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)response.data(),
        .len = response.length()
    };
    return httpd_ws_send_frame(req, &frame);
}

// Implementaciones de cada handler
esp_err_t WsHandlers::handle_run(httpd_req_t* req) {
    JsonDocument doc, res;
    if (!parse_json(req, doc)) return ESP_FAIL;

    std::string code = doc["code"] | "";
    espIDEA->paste_script(code);
    res["type"] = "stdout";
    res["text"] = "Código ejecutado";
    return send_json(req, res);
}

esp_err_t WsHandlers::handle_stop(httpd_req_t* req) {
    espIDEA->paste_script("\x03");  // Ctrl-C
    JsonDocument res;
    res["type"] = "stdout";
    res["text"] = "[REPL] detenido";
    return send_json(req, res);
}

esp_err_t WsHandlers::handle_fs_cat(httpd_req_t* req) {
    JsonDocument doc, res;
    if (!parse_json(req, doc)) return ESP_FAIL;

    std::string path = doc["path"] | "";
    res["type"] = "fs_cat";
    res["text"] = espIDEA->read_file(path);
    return send_json(req, res);
}

esp_err_t WsHandlers::handle_fs_upload(httpd_req_t* req) {
    JsonDocument doc, res;
    if (!parse_json(req, doc)) return ESP_FAIL;

    std::string path = doc["filename"] | "";
    std::string b64 = doc["b64"] | "";

    espIDEA->upload_base64(path, b64);
    res["type"] = "fs_upload";
    res["ok"] = true;
    return send_json(req, res);
}

esp_err_t WsHandlers::handle_fs_download(httpd_req_t* req) {
    JsonDocument doc, res;
    if (!parse_json(req, doc)) return ESP_FAIL;

    std::string path = doc["filename"] | "";

    res["type"] = "fs_download";
    res["filename"] = path;
    res["b64"] = espIDEA->download_base64(path);
    return send_json(req, res);
}

esp_err_t WsHandlers::handle_fs_list(httpd_req_t* req) {
    JsonDocument res;
    res["type"] = "fs_ls";
    JsonArray files = res["list"].to<JsonArray>();

    for (const auto& f : espIDEA->list_files(".")) files.add(f);
    return send_json(req, res);
}

esp_err_t WsHandlers::handle_firmware(httpd_req_t* req) {
    JsonDocument res;
    res["type"] = "firmware";
    res["name"] = "MicroPython";
    res["version"] = "1.21.0";
    return send_json(req, res);
}

// URI declarations
namespace WsRoutes {
    const httpd_uri_t run = {
        .uri = "/ws/run",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_run,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };

    const httpd_uri_t stop = {
        .uri = "/ws/stop",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_stop,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };

    const httpd_uri_t fs_cat = {
        .uri = "/ws/fs_cat",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_fs_cat,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };

    const httpd_uri_t fs_upload = {
        .uri = "/ws/fs_upload",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_fs_upload,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };

    const httpd_uri_t fs_download = {
        .uri = "/ws/fs_download",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_fs_download,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };

    const httpd_uri_t fs_list = {
        .uri = "/ws/fs_ls",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_fs_list,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };

    const httpd_uri_t firmware = {
        .uri = "/ws/firmware",
        .method = HTTP_GET,
        .handler = WsHandlers::handle_firmware,
        .user_ctx = nullptr,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };
}
