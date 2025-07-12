#pragma once

#include <string>
#include <vector>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

class Pyboard {
public:
    Pyboard(uart_port_t uart, int tx_pin, int rx_pin, int baudrate);
    void begin();
    void close();

    // REPL control
    void send_ctrl_c();
    void send_ctrl_d();
    void send_ctrl_a();
    void send_ctrl_b();
    void enter_raw_repl(bool soft_reset = false);
    void exit_raw_repl();
    void enter_paste();
    void soft_reset();
    bool check_repl_ready(TickType_t timeout = pdMS_TO_TICKS(3000));

    // Comunicación
    void write(const std::string& data);
    std::string read_line(TickType_t timeout = pdMS_TO_TICKS(1000));
    std::string read_until(const std::string& ending, TickType_t timeout);

    // Archivos (alta prioridad)
    std::string fs_cat(const std::string& path);
    void fs_rm(const std::string& path);
    void fs_write(const std::string& path, const std::string& data);
    void fs_mkdir(const std::string& path);
    void fs_touch(const std::string& path);
    void fs_cp(const std::string& src, const std::string& dest);
    void fs_put(const std::string& filename, const std::string& base64);
    std::string fs_get(const std::string& filename);

private:
    uart_port_t _uart;
    int _tx_pin;
    int _rx_pin;
    int _baudrate;
};