#include "EspressIDEA.hpp"
#include "esp_log.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

static const char* TAG = "EspressIDEA";

EspressIDEA::EspressIDEA(Pyboard pyboard)
    : _py(std::move(pyboard)) {}

void EspressIDEA::begin() {
    ESP_LOGI(TAG, "Inicializando EspressIDEA...");
    _py.begin();
    _py.send_ctrl_c();
    vTaskDelay(pdMS_TO_TICKS(200));
    _py.send_ctrl_c();
    vTaskDelay(pdMS_TO_TICKS(200));
    _initialized = true;
}

void EspressIDEA::end() {
    ESP_LOGI(TAG, "Cerrando EspressIDEA...");
    _py.close();
    _initialized = false;
}

void EspressIDEA::ensure_ready() {
    if (!_initialized) {
        ESP_LOGW(TAG, "EspressIDEA aún no ha sido inicializado. Llamando a begin().");
        begin();
    }
}

bool EspressIDEA::run_script(const std::string& code) {
    ensure_ready();
    _py.enter_raw_repl(true);
    _py.write(code + "\r\n");
    _py.send_ctrl_d();
    std::string output = _py.read_until(">", pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "Resultado:\n%s", output.c_str());
    return output.find("Traceback") == std::string::npos;
}

bool EspressIDEA::paste_script(const std::string& code) {
    ensure_ready();
    _py.send_ctrl_c();
    vTaskDelay(pdMS_TO_TICKS(200));
    _py.send_ctrl_c();
    vTaskDelay(pdMS_TO_TICKS(200));

    _py.enter_paste();
    _py.write(code + "\r\n");
    _py.send_ctrl_d();
    ESP_LOGI(TAG, "Código enviado por modo paste.");
    ESP_LOGI(TAG, "Código recibido:\r\n %s", code.c_str());
    return true;
}

void EspressIDEA::soft_reset() {
    ensure_ready();
    _py.soft_reset();
}

std::string EspressIDEA::read_file(const std::string& path) {
    ensure_ready();
    return _py.fs_cat(path);
}

bool EspressIDEA::write_file(const std::string& path, const std::string& content) {
    ensure_ready();
    _py.fs_write(path, content);
    return true;
}

bool EspressIDEA::delete_file(const std::string& path) {
    ensure_ready();
    _py.fs_rm(path);
    return true;
}

bool EspressIDEA::make_dir(const std::string& path) {
    ensure_ready();
    _py.fs_mkdir(path);
    return true;
}

bool EspressIDEA::touch_file(const std::string& path) {
    ensure_ready();
    _py.fs_touch(path);
    return true;
}

bool EspressIDEA::copy_file(const std::string& src, const std::string& dest) {
    ensure_ready();
    _py.fs_cp(src, dest);
    return true;
}

bool EspressIDEA::upload_base64(const std::string& path, const std::string& b64) {
    ensure_ready();
    _py.fs_put(path, b64);
    return true;
}

std::string EspressIDEA::download_base64(const std::string& path) {
    ensure_ready();
    return _py.fs_get(path);
}

std::vector<std::string> EspressIDEA::list_files(const std::string& path) {
    ensure_ready();
    // TODO: leer listado real desde el REPL
    return { "boot.py", "main.py", "lib/", "code.py" };
}

std::string EspressIDEA::encode_base64(const std::string& raw) {
    size_t olen = 0;
    size_t output_len = raw.size() * 2;
    std::vector<uint8_t> encoded(output_len);
    int err = mbedtls_base64_encode(encoded.data(), encoded.size(), &olen,
                                    reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
    if (err != 0) return "";
    return std::string(reinterpret_cast<char*>(encoded.data()), olen);
}

std::string EspressIDEA::decode_base64(const std::string& b64) {
    size_t olen = 0;
    std::vector<uint8_t> decoded(b64.size());
    int err = mbedtls_base64_decode(decoded.data(), decoded.size(), &olen,
                                    reinterpret_cast<const uint8_t*>(b64.data()), b64.size());
    if (err != 0) return "";
    return std::string(reinterpret_cast<char*>(decoded.data()), olen);
}
