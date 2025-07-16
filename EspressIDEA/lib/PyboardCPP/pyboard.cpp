#include "Pyboard.hpp"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include "mbedtls/base64.h"

static const char* TAG = "Pyboard";
static const int TIME = 100;

Pyboard::Pyboard(uart_port_t uart, int tx_pin, int rx_pin, int baudrate)
    : _uart(uart), _tx_pin(tx_pin), _rx_pin(rx_pin), _baudrate(baudrate) {}

void Pyboard::begin() {
    uart_config_t config;
    config.baud_rate = _baudrate;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.rx_flow_ctrl_thresh = 0;
    config.source_clk = UART_SCLK_DEFAULT;

    uart_driver_install(_uart, 4096, 4096, 0, nullptr, 0);
    uart_param_config(_uart, &config);
    uart_set_pin(_uart, _tx_pin, _rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void Pyboard::close() {
    uart_driver_delete(_uart);
}

void Pyboard::send_ctrl_c() {
    uart_write_bytes(_uart, "\x03", 1);
    vTaskDelay(pdMS_TO_TICKS(TIME));
}

void Pyboard::send_ctrl_d() {
    uart_write_bytes(_uart, "\x04", 1);
    vTaskDelay(pdMS_TO_TICKS(TIME));
}

void Pyboard::send_ctrl_a() {
    uart_write_bytes(_uart, "\x01", 1);
    vTaskDelay(pdMS_TO_TICKS(TIME));
}

void Pyboard::send_ctrl_b() {
    uart_write_bytes(_uart, "\x02", 1);
    vTaskDelay(pdMS_TO_TICKS(TIME));
}

 void Pyboard::enter_paste(){
    uart_write_bytes(_uart, "\x05", 1);
    vTaskDelay(pdMS_TO_TICKS(TIME));
 }

void Pyboard::enter_raw_repl(bool soft_reset) {
    send_ctrl_c();
    uart_flush(_uart);
    send_ctrl_a();

    std::string output = read_until("raw REPL; CTRL-B to exit\r\n>", pdMS_TO_TICKS(500));
    if (output.find("raw REPL; CTRL-B to exit") == std::string::npos) {
        ESP_LOGW(TAG, "No se pudo entrar en raw REPL");
        return;
    }

    if (soft_reset) {
        send_ctrl_d();
        read_until("soft reboot\r\n", pdMS_TO_TICKS(1000));
        read_until("raw REPL; CTRL-B to exit\r\n>", pdMS_TO_TICKS(500));
    }
}

void Pyboard::exit_raw_repl() {
    send_ctrl_b();
}

void Pyboard::soft_reset() {
    send_ctrl_d();
}

std::string Pyboard::read_until(const std::string& ending, TickType_t timeout) {
    std::string result;
    const TickType_t start = xTaskGetTickCount();
    const size_t buf_size = 64;
    uint8_t buf[buf_size];

    while ((xTaskGetTickCount() - start) < timeout) {
        int len = uart_read_bytes(_uart, buf, buf_size, pdMS_TO_TICKS(50));  // espera hasta 50ms
        if (len > 0) {
            ESP_LOGI(TAG, ">> %.*s", len, buf);

            result.append(reinterpret_cast<char*>(buf), len);

            // Revisa si el final deseado aparece
            if (result.size() >= ending.size() &&
                result.find(ending, result.size() - ending.size() - 8) != std::string::npos) {
                break;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));  // espera pequeño entre polls
        }
    }

    return result;
}

void Pyboard::write(const std::string& data) {
    uart_write_bytes(_uart, data.c_str(), data.length());
    vTaskDelay(pdMS_TO_TICKS(10));
}

std::string Pyboard::read_line(TickType_t timeout) {
    std::string line;
    uint8_t buf[1];
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < timeout) {
        int len = uart_read_bytes(_uart, buf, 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            if (buf[0] == '\n') break;
            line.push_back(buf[0]);
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return line;
}

bool Pyboard::check_repl_ready(TickType_t timeout) {
    std::string result = read_until(">>> ", timeout);
    return result.find(">>> ") != std::string::npos;
}

std::string Pyboard::fs_cat(const std::string& path) {
    std::string command = "with open('" + path + "', 'r') as f:\n print(f.read())\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    return read_until(">", pdMS_TO_TICKS(3000));
}

void Pyboard::fs_rm(const std::string& path) {
    std::string command = "import os\nos.remove('" + path + "')\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    read_until(">", pdMS_TO_TICKS(3000));
}

void Pyboard::fs_write(const std::string& path, const std::string& data) {
    std::string command = "f = open('" + path + "', 'w')\nf.write('" + data + "')\nf.close()\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    read_until(">", pdMS_TO_TICKS(3000));
}

void Pyboard::fs_mkdir(const std::string& path) {
    std::string command = "import os\nos.mkdir('" + path + "')\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    read_until(">", pdMS_TO_TICKS(3000));
}

void Pyboard::fs_touch(const std::string& path) {
    std::string command = "open('" + path + "', 'a').close()\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    read_until(">", pdMS_TO_TICKS(3000));
}

void Pyboard::fs_cp(const std::string& src, const std::string& dest) {
    std::string command = "fr=open('" + src + "','rb')\nfw=open('" + dest + "','wb')\nfw.write(fr.read())\nfr.close()\nfw.close()\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    read_until(">", pdMS_TO_TICKS(3000));
}

void Pyboard::fs_put(const std::string& filename, const std::string& b64) {
    std::string command =
        "import ubinascii\n"
        "data = ubinascii.a2b_base64('" + b64 + "')\n"
        "f = open('" + filename + "', 'wb')\n"
        "f.write(data)\nf.close()\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    read_until(">", pdMS_TO_TICKS(3000));
}

std::string Pyboard::fs_get(const std::string& filename) {
    std::string command =
        "import ubinascii\n"
        "with open('" + filename + "', 'rb') as f:\n"
        " print(ubinascii.b2a_base64(f.read()).decode('utf-8'))\n";
    send_ctrl_c();
    enter_raw_repl(false);
    write(command);
    send_ctrl_d();
    return read_until(">", pdMS_TO_TICKS(5000));
}
