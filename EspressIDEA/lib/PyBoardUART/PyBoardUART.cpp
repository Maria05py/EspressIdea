#include "PyBoardUART.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cctype>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"  // esp_rom_delay_us()

static const char *TAG = "PyBoardUART";

// ============================================================================
// Helpers de archivo (scope local)
// ============================================================================
namespace {

// Quota segura para literales Python: ' -> \'
static std::string pyQuote(const std::string& s) {
    std::string out; out.reserve(s.size()+2);
    out.push_back('\'');
    for (char c: s) {
        if (c=='\'') out += "\\'";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

static inline void stripCR(std::string &s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
}

// Borra secuencias ANSI (CSI/OSC) que podrían filtrarse a través del WS
static void stripANSIEscapes(std::string &s) {
    std::string out; out.reserve(s.size());
    enum { NORM, ESC_SEEN, CSI, OSC, OSC_ESC } st = NORM;
    for (size_t i=0;i<s.size();++i){
        unsigned char c = (unsigned char)s[i];
        switch (st){
            case NORM:
                if (c == 0x1B) st = ESC_SEEN;
                else out.push_back(c);
                break;
            case ESC_SEEN:
                if (c == '[') st = CSI;
                else if (c == ']') st = OSC;
                else st = NORM;
                break;
            case CSI:
                if (c >= 0x40 && c <= 0x7E) st = NORM;
                break;
            case OSC:
                if (c == 0x07) { st = NORM; }
                else if (c == 0x1B) { st = OSC_ESC; }
                break;
            case OSC_ESC:
                if (c == '\\') { st = NORM; }
                else { st = OSC; }
                break;
        }
    }
    s.swap(out);
}

// Limpia artefactos de paste (líneas "===" y banner), y recorta '>>>'
static void stripPasteArtifacts(std::string &raw) {
    stripCR(raw);
    std::istringstream is(raw);
    std::string out, line;
    while (std::getline(is, line)) {
        if (line.rfind("===", 0) == 0) continue;
        if (line.find("paste mode;") != std::string::npos) continue;
        out += line;
        out.push_back('\n');
    }
    size_t p = out.rfind(">>>");
    if (p != std::string::npos) out.erase(p);
    while (!out.empty() && (out.back()=='\n' || out.back()=='\r')) out.pop_back();
    raw.swap(out);
}

// Espera un substring en UART con timeout
static PyBoard::ErrorCode waitForSubstring(uart_port_t u, const char* needle, uint32_t timeoutMs) {
    using namespace PyBoard;
    const uint64_t deadline = (uint64_t)esp_timer_get_time() + (uint64_t)timeoutMs * 1000ULL;
    std::string acc; acc.reserve(512);
    uint8_t buf[128];
    while ((uint64_t)esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(u, buf, sizeof(buf), pdMS_TO_TICKS(30));
        if (n > 0) {
            acc.append(reinterpret_cast<const char*>(buf), n);
            if (acc.find(needle) != std::string::npos) return ErrorCode::OK;
            if (acc.size() > 4096) acc.erase(0, acc.size() - 1024);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return ErrorCode::TIMEOUT;
}

// Asegura estar en prompt >>> (maneja “Press any key...”)
static PyBoard::ErrorCode ensureAtPrompt(uart_port_t u, uint32_t timeoutMs = 1500) {
    using namespace PyBoard;
    const uint64_t deadline = (uint64_t)esp_timer_get_time() + (uint64_t)timeoutMs * 1000ULL;

    const char CR = '\r';
    uart_flush_input(u);
    (void)uart_write_bytes(u, &CR, 1);

    std::string acc; acc.reserve(512);
    uint8_t buf[128];
    bool pressedAnyKey = false;

    while ((uint64_t)esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(u, buf, sizeof(buf), pdMS_TO_TICKS(40));
        if (n > 0) {
            acc.append(reinterpret_cast<const char*>(buf), n);

            if (!pressedAnyKey &&
                (acc.find("Press any key to enter the REPL") != std::string::npos ||
                 acc.find("Press any key to enter the REPL.") != std::string::npos)) {
                (void)uart_write_bytes(u, &CR, 1);
                pressedAnyKey = true;
            }

            if (acc.find(">>>") != std::string::npos) return ErrorCode::OK;

            if (acc.size() > 4096) acc.erase(0, acc.size() - 1024);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ErrorCode::TIMEOUT;
}

// Entra a paste mode (^E) y verifica banner ("paste mode" o "=== ")
static PyBoard::ErrorCode enterPasteMode(uart_port_t u) {
    using namespace PyBoard;
    const uint8_t CTRL_E = 0x05;

    auto rc = ensureAtPrompt(u, 2000);
    if (rc != ErrorCode::OK) return rc;

    uart_flush_input(u);

    if (uart_write_bytes(u, &CTRL_E, 1) != 1) return ErrorCode::UART_ERROR;

    if (waitForSubstring(u, "paste mode", 800) == ErrorCode::OK) return ErrorCode::OK;
    if (waitForSubstring(u, "=== ", 800) == ErrorCode::OK)       return ErrorCode::OK;

    return ErrorCode::REPL_ERROR;
}

// Pega en paste mode carácter por carácter con pequeño pacing y ejecuta (^D)
static PyBoard::ErrorCode pasteLiteralBlock(uart_port_t u, const char* data, size_t len) {
    using namespace PyBoard;
    if (!data || len == 0) return ErrorCode::OK;

    std::string norm; norm.reserve(len * 2);
    for (size_t i=0;i<len;i++){
        char ch = data[i];
        if (ch == '\r') {
            if (i+1 < len && data[i+1] == '\n') { norm.push_back('\r'); }
            else { norm += "\r\n"; }
        } else if (ch == '\n') {
            if (i>0 && data[i-1]=='\r') norm.push_back('\n'); else norm += "\r\n";
        } else {
            norm.push_back(ch);
        }
    }
    if (norm.empty() || norm.back() != '\n') norm += "\r\n";

    const uint32_t per_char_us = 200;
    const uint32_t per_nl_extra_us = 300;

    for (size_t i=0;i<norm.size(); ++i) {
        const char ch = norm[i];
        int wr = uart_write_bytes(u, &ch, 1);
        if (wr != 1) return ErrorCode::UART_ERROR;

        esp_rom_delay_us(per_char_us);
        if (ch == '\n') esp_rom_delay_us(per_nl_extra_us);
    }

    const uint8_t CTRL_D = 0x04;
    if (uart_write_bytes(u, &CTRL_D, 1) != 1) return ErrorCode::UART_ERROR;

    return ErrorCode::OK;
}

// Lee hasta ver 'end' y devuelve en 'output'
static PyBoard::ErrorCode readTo(uart_port_t u, const char* end, std::string& output, uint32_t timeoutMs) {
    using namespace PyBoard;
    output.clear();
    const uint64_t deadline = (uint64_t)esp_timer_get_time() + (uint64_t)timeoutMs * 1000ULL;
    std::string acc; acc.reserve(1024);
    uint8_t buf[256];

    while ((uint64_t)esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(u, buf, sizeof(buf), pdMS_TO_TICKS(40));
        if (n > 0) {
            acc.append(reinterpret_cast<const char*>(buf), n);
            size_t p = acc.find(end);
            if (p != std::string::npos) {
                output.append(acc.data(), p + std::strlen(end));
                return ErrorCode::OK;
            }
            if (acc.size() > 4096) {
                output.append(acc);
                acc.clear();
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    return ErrorCode::TIMEOUT;
}

// Drena hasta ver el prompt '>>>'
static void drainToPrompt(uart_port_t u, uint32_t timeoutMs = 800) {
    const uint64_t deadline = (uint64_t)esp_timer_get_time() + (uint64_t)timeoutMs * 1000ULL;
    std::string acc; acc.reserve(512);
    uint8_t buf[128];
    while ((uint64_t)esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(u, buf, sizeof(buf), pdMS_TO_TICKS(30));
        if (n > 0) {
            acc.append(reinterpret_cast<const char*>(buf), n);
            if (acc.find(">>>") != std::string::npos) return;
            if (acc.size() > 4096) acc.erase(0, acc.size() - 1024);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

} // namespace

// ============================================================================
// Implementación PyBoardUART
// ============================================================================
namespace PyBoard {

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

PyBoardUART::PyBoardUART(uart_port_t uart, int tx, int rx, BaudRate baud,
                         Timeout timeout, ChunkSize chunk)
    : uartNum(uart), txPin(tx), rxPin(rx), baudRate(baud),
      defaultTimeout(timeout), chunkSize(chunk),
      inRawRepl(false), useRawPaste(true), monitorEnabled(false),
      eventQueue(nullptr), monitorTask(nullptr) {
    rxBuffer = std::make_unique<uint8_t[]>(BUFFER_SIZE);
    txBuffer = std::make_unique<uint8_t[]>(BUFFER_SIZE);
}

PyBoardUART::~PyBoardUART() {
    deinit();
}

ErrorCode PyBoardUART::init() {
    uart_config_t uart_config = {
        .baud_rate = static_cast<int>(baudRate),
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_driver_install(uartNum, BUFFER_SIZE*2, BUFFER_SIZE*2, 20, &eventQueue, 0);
    if (err != ESP_OK) {
        setError("Failed to install UART driver");
        return ErrorCode::UART_ERROR;
    }
    err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        uart_driver_delete(uartNum);
        setError("Failed to configure UART parameters");
        return ErrorCode::UART_ERROR;
    }
    err = uart_set_pin(uartNum, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        uart_driver_delete(uartNum);
        setError("Failed to set UART pins");
        return ErrorCode::UART_ERROR;
    }

    ESP_LOGI(TAG, "Initialized on UART%d (TX:%d, RX:%d, Baud:%d)",
             uartNum, txPin, rxPin, static_cast<int>(baudRate));
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::deinit() {
    if (eventQueue != nullptr) {
        uart_driver_delete(uartNum);
        eventQueue = nullptr;
    }
    ESP_LOGI(TAG, "Deinitialized");
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::writeData(const uint8_t *data, size_t len) {
    int written = uart_write_bytes(uartNum, data, len);
    if (written != static_cast<int>(len)) {
        setError("Failed to write all bytes to UART");
        return ErrorCode::UART_ERROR;
    }
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::writeData(const std::string &data) {
    return writeData(reinterpret_cast<const uint8_t *>(data.c_str()), data.length());
}

ErrorCode PyBoardUART::write(const void *data, size_t len) {
    if (!data || len == 0) return ErrorCode::OK;
    return writeData(reinterpret_cast<const uint8_t *>(data), len);
}
ErrorCode PyBoardUART::write(const std::string &data) {
    if (data.empty()) return ErrorCode::OK;
    return writeData(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}
ErrorCode PyBoardUART::write(const char *data, size_t len) {
    if (!data || len == 0) return ErrorCode::OK;
    return writeData(reinterpret_cast<const uint8_t *>(data), len);
}

ErrorCode PyBoardUART::readUntil(const std::string &ending, std::string &output, uint32_t timeoutMs) {
    output.clear();
    uint64_t deadline = (uint64_t)esp_timer_get_time() + (uint64_t)timeoutMs * 1000ULL;

    while ((uint64_t)esp_timer_get_time() < deadline) {
        uint8_t byte;
        int len = uart_read_bytes(uartNum, &byte, 1, 10 / portTICK_PERIOD_MS);
        if (len > 0) {
            output += static_cast<char>(byte);
            if (output.length() >= ending.length()) {
                if (output.substr(output.length() - ending.length()) == ending) {
                    return ErrorCode::OK;
                }
            }
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    setError("Timeout waiting for: " + ending);
    return ErrorCode::TIMEOUT;
}

ErrorCode PyBoardUART::flushInput() {
    uart_flush_input(uartNum);
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::waitForReplPrompt(uint32_t timeoutMs) {
    if (!uart_is_driver_installed(uartNum)) {
        setError("UART driver not installed");
        return ErrorCode::UART_ERROR;
    }
    const uint64_t deadline = (uint64_t)esp_timer_get_time() + (uint64_t)timeoutMs * 1000ULL;
    std::string acc; acc.reserve(512);
    uint8_t tmp[128];

    while ((uint64_t)esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(uartNum, tmp, sizeof(tmp), pdMS_TO_TICKS(50));
        if (n > 0) {
            acc.append(reinterpret_cast<const char *>(tmp), n);
            if (acc.find(">>>") != std::string::npos) return ErrorCode::OK;
            if (acc.size() > 4096) acc.erase(0, acc.size() - 1024);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    setError("Timeout esperando prompt '>>>'");
    return ErrorCode::TIMEOUT;
}

ErrorCode PyBoardUART::syncReplCircuitPython(uint32_t timeoutMs) {
    const uint8_t ctrlC = 0x03;
    const uint8_t ctrlD = 0x04;

    write(&ctrlC, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    write(&ctrlD, 1);

    auto rc = waitForReplPrompt(timeoutMs);
    if (rc != ErrorCode::OK) {
        const char crlf[] = "\r\n";
        write(crlf, sizeof(crlf) - 1);
        rc = waitForReplPrompt(800);
    }
    return rc;
}

ErrorCode PyBoardUART::enterRawRepl(bool softReset) {
    ErrorCode err;
    std::string response;

    uint8_t ctrlC[] = {'\r', CTRL_C};
    err = writeData(ctrlC, sizeof(ctrlC));
    if (err != ErrorCode::OK) return err;

    vTaskDelay(100 / portTICK_PERIOD_MS);

    err = flushInput();
    if (err != ErrorCode::OK) return err;

    uint8_t ctrlA[] = {'\r', CTRL_A};
    err = writeData(ctrlA, sizeof(ctrlA));
    if (err != ErrorCode::OK) return err;

    err = readUntil("raw REPL; CTRL-B to exit\r\n>", response, static_cast<uint32_t>(defaultTimeout));
    if (err != ErrorCode::OK) {
        setError("Failed to enter raw REPL");
        return ErrorCode::REPL_ERROR;
    }

    if (softReset) {
        err = writeData(&CTRL_D, 1);
        if (err != ErrorCode::OK) return err;

        err = readUntil("soft reboot\r\n", response, static_cast<uint32_t>(defaultTimeout));
        if (err != ErrorCode::OK) {
            setError("Soft reset failed");
            return ErrorCode::REPL_ERROR;
        }

        err = readUntil("raw REPL; CTRL-B to exit\r\n", response, static_cast<uint32_t>(defaultTimeout));
        if (err != ErrorCode::OK) return ErrorCode::REPL_ERROR;
    }

    inRawRepl = true;
    ESP_LOGI(TAG, "Entered raw REPL mode");
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::exitRawRepl() {
    uint8_t ctrlB[] = {'\r', CTRL_B};
    ErrorCode err = writeData(ctrlB, sizeof(ctrlB));
    if (err != ErrorCode::OK) return err;

    inRawRepl = false;
    ESP_LOGI(TAG, "Exited raw REPL mode");
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::execRawNoFollow(const std::string &command) {
    if (!inRawRepl) {
        setError("Not in raw REPL mode");
        return ErrorCode::NOT_IN_RAW_REPL;
    }

    std::string response;

    ErrorCode err = readUntil(">", response, static_cast<uint32_t>(defaultTimeout));
    if (err != ErrorCode::OK) return err;

    if (useRawPaste) {
        uint8_t pasteCmd[] = {CTRL_E, 'A', 0x01};
        err = writeData(pasteCmd, sizeof(pasteCmd));
        if (err != ErrorCode::OK) return err;

        uint8_t pasteResponse[2];
        int len = uart_read_bytes(uartNum, pasteResponse, 2, 100 / portTICK_PERIOD_MS);

        if (len == 2 && pasteResponse[0] == 'R') {
            if (pasteResponse[1] == 0x01) {
                return rawPasteWrite(command);
            } else {
                useRawPaste = false;
            }
        }
    }

    size_t chunkSizeVal = static_cast<size_t>(chunkSize);
    for (size_t i = 0; i < command.length(); i += chunkSizeVal) {
        size_t len = std::min(chunkSizeVal, command.length() - i);
        err = writeData(reinterpret_cast<const uint8_t *>(command.c_str() + i), len);
        if (err != ErrorCode::OK) return err;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    err = writeData(&CTRL_D, 1);
    if (err != ErrorCode::OK) return err;

    uint8_t okResponse[2];
    int okLen = uart_read_bytes(uartNum, okResponse, 2, 100 / portTICK_PERIOD_MS);
    if (okLen != 2 || okResponse[0] != 'O' || okResponse[1] != 'K') {
        setError("Command not accepted by device");
        return ErrorCode::EXEC_ERROR;
    }

    return ErrorCode::OK;
}

ErrorCode PyBoardUART::follow(std::string &output, std::string &error, uint32_t timeoutMs) {
    ErrorCode err;
    err = readUntil("\x04", output, timeoutMs);
    if (err != ErrorCode::OK) return err;
    if (!output.empty() && output.back() == '\x04') output.pop_back();

    err = readUntil("\x04", error, timeoutMs);
    if (err != ErrorCode::OK) return err;
    if (!error.empty() && error.back() == '\x04') error.pop_back();

    return ErrorCode::OK;
}

ErrorCode PyBoardUART::execRaw(const std::string &command, std::string &output,
                               std::string &error, uint32_t timeoutMs) {
    if (timeoutMs == 0) timeoutMs = static_cast<uint32_t>(defaultTimeout);

    ErrorCode err = execRawNoFollow(command);
    if (err != ErrorCode::OK) return err;

    return follow(output, error, timeoutMs);
}

// Ruta TEXT (CircuitPython): paste literal con pacing, lee hasta ">>>", limpia artefactos
ErrorCode PyBoardUART::exec(const std::string &command, std::string &output, uint32_t timeoutMs) {
    if (timeoutMs == 0) timeoutMs = static_cast<uint32_t>(defaultTimeout);

    if (inRawRepl) {
        std::string errTxt;
        auto rc = execRaw(command, output, errTxt, timeoutMs);
        if (!errTxt.empty()) {
            ESP_LOGE(TAG, "Execution error: %s", errTxt.c_str());
            setError(errTxt);
            if (rc == ErrorCode::OK) rc = ErrorCode::EXEC_ERROR;
        }
        return rc;
    }

    auto rc = ensureAtPrompt(uartNum, 1500);
    if (rc != ErrorCode::OK) return rc;

    rc = enterPasteMode(uartNum);
    if (rc != ErrorCode::OK) return rc;

    rc = pasteLiteralBlock(uartNum, command.c_str(), command.size());
    if (rc != ErrorCode::OK) return rc;

    std::string raw;
    rc = readTo(uartNum, ">>>", raw, timeoutMs);
    if (rc != ErrorCode::OK) return rc;

    stripPasteArtifacts(raw);
    output.swap(raw);
    return ErrorCode::OK;
}

// Versión que ignora salida
ErrorCode PyBoardUART::exec(const std::string &command) {
    std::string discard;
    return exec(command, discard, 0);
}

// “Friendly” (opcional, mantiene por compatibilidad)
ErrorCode PyBoardUART::execFriendly(const std::string &command,
                                    std::string &output,
                                    uint32_t timeoutMs) {
    output.clear();
    if (timeoutMs == 0) timeoutMs = static_cast<uint32_t>(defaultTimeout);

    auto rc = waitForReplPrompt(800);
    if (rc != ErrorCode::OK) {
        rc = syncReplCircuitPython(timeoutMs);
        if (rc != ErrorCode::OK) {
            setError("No se pudo sincronizar REPL friendly");
            return rc;
        }
    }

    uint8_t ctrlE = 0x05;
    write(&ctrlE, 1);
    vTaskDelay(pdMS_TO_TICKS(60));

    auto toCRLF = [](const std::string &s) {
        std::string o; o.reserve(s.size()+8);
        for (char c: s) {
            if (c=='\n') { o+="\r\n"; }
            else { o.push_back(c=='\r' ? '\r' : c); }
        }
        return o;
    };

    std::string prog;
    prog.reserve(command.size() + 256);
    prog += "print('<<<BEGIN>>>')\r\n";
    prog += "try:\r\n";
    {
        std::string crlf = toCRLF(command);
        size_t pos = 0;
        while (pos < crlf.size()) {
            size_t e = crlf.find("\r\n", pos);
            std::string line = (e == std::string::npos) ? crlf.substr(pos)
                                                        : crlf.substr(pos, e - pos);
            prog += "  " + line + "\r\n";
            if (e == std::string::npos) break;
            pos = e + 2;
        }
    }
    prog += "except Exception as e:\r\n";
    prog += "  import sys,traceback\r\n";
    prog += "  sys.print_exception(e)\r\n";
    prog += "print('<<<END>>>')\r\n";

    auto wrc = write(prog);
    if (wrc != ErrorCode::OK) return wrc;

    uint8_t ctrlD = 0x04;
    write(&ctrlD, 1);

    std::string cap;
    rc = readUntil(">>>", cap, timeoutMs);
    if (rc != ErrorCode::OK) { setError("Timeout leyendo salida hasta >>>"); return rc; }

    const char *B = "<<<BEGIN>>>", *E = "<<<END>>>";
    size_t b = cap.find(B);
    size_t e = (b==std::string::npos) ? std::string::npos : cap.find(E, b + std::strlen(B));
    if (b == std::string::npos || e == std::string::npos || e <= b) { output = cap; return ErrorCode::OK; }

    size_t lb = cap.find('\n', b);
    if (lb == std::string::npos) lb = cap.find('\r', b);
    size_t start = (lb == std::string::npos) ? (b + std::strlen(B)) : (lb + 1);

    output = cap.substr(start, e - start);
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::eval(const std::string &expression, std::string &result, uint32_t timeoutMs) {
    std::string command = "print(repr(" + std::string(expression) + "))";
    ErrorCode err = exec(command, result, timeoutMs);
    if (err == ErrorCode::OK) {
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
    }
    return err;
}

// ============================================================================
// Base64 utils (robustos ante whitespace)
// ============================================================================
std::string PyBoardUART::base64Encode(const std::vector<uint8_t> &data) {
    std::string encoded;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c; valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4) encoded.push_back('=');
    return encoded;
}

std::vector<uint8_t> PyBoardUART::base64Decode(const std::string &encoded) {
    auto idx = [](unsigned char c)->int {
        if (c>='A'&&c<='Z') return c-'A';
        if (c>='a'&&c<='z') return c-'a'+26;
        if (c>='0'&&c<='9') return c-'0'+52;
        if (c=='+') return 62;
        if (c=='/') return 63;
        return -1;
    };
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c: encoded) {
        if (c=='=' || std::isspace(c)) continue;
        int d = idx(c);
        if (d < 0) continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// ============================================================================
// Filesystem
// ============================================================================
ErrorCode PyBoardUART::listDir(const std::string &path, std::vector<FileInfo> &files) {
    files.clear();
    std::string normPath = path.empty() ? "/" : path;

    std::stringstream cmd;
    cmd
        << "import os\r\n"
        << "BASE=" << pyQuote(normPath) << "\r\n"
        << "try:\r\n"
        << "  il = os.ilistdir\r\n"
        << "  use_il = True\r\n"
        << "except AttributeError:\r\n"
        << "  use_il = False\r\n"
        << "if use_il:\r\n"
        << "  for f in os.ilistdir(BASE):\r\n"
        << "    name=f[0]\r\n"
        << "    mode=f[1]\r\n"
        << "    size=(f[3] if len(f)>3 else 0)\r\n"
        << "    print(name+'|'+str(mode)+'|'+str(size))\r\n"
        << "else:\r\n"
        << "  L = os.listdir(BASE if BASE else '/')\r\n"
        << "  for name in L:\r\n"
        << "    p=(BASE.rstrip('/')+'/'+name) if BASE and BASE!='/' else '/' + name\r\n"
        << "    try:\r\n"
        << "      st=os.stat(p)\r\n"
        << "      print(name+'|'+str(st[0])+'|'+str(st[6]))\r\n"
        << "    except Exception:\r\n"
        << "      print(name+'|0|0')\r\n";

    std::string output;
    ErrorCode err = exec(cmd.str(), output);
    if (err != ErrorCode::OK) return err;

    stripANSIEscapes(output);
    stripCR(output);

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t pos1 = line.find('|');
        size_t pos2 = (pos1 == std::string::npos) ? std::string::npos : line.find('|', pos1 + 1);
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string name = line.substr(0, pos1);
            int mode = std::atoi(line.substr(pos1 + 1, pos2 - pos1 - 1).c_str());
            size_t size = static_cast<size_t>(std::strtoull(line.substr(pos2 + 1).c_str(), nullptr, 10));
            files.emplace_back(name, size, (mode & 0x4000) != 0);
        }
    }
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::readFile(const std::string &path, std::string &content) {
    std::vector<uint8_t> rawContent;
    ErrorCode err = readFileRaw(path, rawContent);
    if (err != ErrorCode::OK) return err;
    content = base64Encode(rawContent);
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::writeFile(const std::string &path, const std::string &base64Content) {
    std::vector<uint8_t> rawContent = base64Decode(base64Content);
    return writeFileRaw(path, rawContent);
}

ErrorCode PyBoardUART::readFileRaw(const std::string &path, std::vector<uint8_t> &content) {
    content.clear();

    FileInfo info;
    ErrorCode err = getFileInfo(path, info);
    if (err != ErrorCode::OK) return err;

    std::string openCmd = "f=open(" + pyQuote(path) + ",'rb')\nr=f.read";
    err = exec(openCmd);
    if (err != ErrorCode::OK) return err;

    size_t chunkSizeVal = static_cast<size_t>(chunkSize);
    std::string output;

    while (true) {
        std::string readCmd =
            "try:\n import ubinascii as binascii\n"
            "except ImportError:\n import binascii as binascii\n"
            "print(binascii.b2a_base64(r(" + std::to_string(chunkSizeVal) + ")).decode().strip())";

        err = exec(readCmd, output);
        if (err != ErrorCode::OK) break;

        if (output.empty() || output == "None") break;

        std::vector<uint8_t> chunk = base64Decode(output);
        if (!chunk.empty())
            content.insert(content.end(), chunk.begin(), chunk.end());

        if (chunk.size() < chunkSizeVal) break;
    }

    exec("f.close()");
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::writeFileRaw(const std::string &path, const std::vector<uint8_t> &content) {
    std::string openCmd = "f=open(" + pyQuote(path) + ",'wb')\nw=f.write";
    ErrorCode err = exec(openCmd);
    if (err != ErrorCode::OK) return err;

    size_t chunkSizeVal = static_cast<size_t>(chunkSize);

    for (size_t i = 0; i < content.size(); i += chunkSizeVal) {
        size_t len = std::min(chunkSizeVal, content.size() - i);
        std::vector<uint8_t> chunk(content.begin() + i, content.begin() + i + len);

        std::string base64Chunk = base64Encode(chunk);
        std::string writeCmd =
            "try:\n import ubinascii as binascii\n"
            "except ImportError:\n import binascii as binascii\n"
            "w(binascii.a2b_base64('" + base64Chunk + "'))";

        err = exec(writeCmd);
        if (err != ErrorCode::OK) {
            exec("f.close()");
            return err;
        }
    }

    return exec("f.close()");
}

ErrorCode PyBoardUART::deleteFile(const std::string &path) {
    std::string cmd = "import os\nos.remove(" + pyQuote(path) + ")";
    return exec(cmd);
}

ErrorCode PyBoardUART::createDir(const std::string &path) {
    std::string cmd = "import os\nos.mkdir(" + pyQuote(path) + ")";
    return exec(cmd);
}

ErrorCode PyBoardUART::deleteDir(const std::string &path) {
    std::string cmd = "import os\nos.rmdir(" + pyQuote(path) + ")";
    return exec(cmd);
}

ErrorCode PyBoardUART::exists(const std::string &path, bool &result) {
    std::string cmd =
        "import os\ntry:\n os.stat(" + pyQuote(path) + ")\n print('1')\nexcept:\n print('0')";
    std::string output;

    ErrorCode err = exec(cmd, output);
    if (err != ErrorCode::OK) return err;

    result = (!output.empty() && output[0] == '1');
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::getFileInfo(const std::string &path, FileInfo &info) {
    std::string cmd = "import os\ns=os.stat(" + pyQuote(path) + ")\nprint(s[0],s[6])";
    std::string output;

    ErrorCode err = exec(cmd, output);
    if (err != ErrorCode::OK) return err;

    int mode=0, size=0;
    if (std::sscanf(output.c_str(), "%d %d", &mode, &size) == 2) {
        info.name = path;
        info.isDirectory = (mode & 0x4000) != 0;
        info.size = static_cast<size_t>(size);
        return ErrorCode::OK;
    }

    setError("Failed to parse file info");
    return ErrorCode::EXEC_ERROR;
}

// ============================================================================
// Transferencias “host<->dispositivo” (utilidades de conveniencia)
// ============================================================================
ErrorCode PyBoardUART::downloadFile(const std::string &remotePath, const std::string &localPath,
                                    ProgressCallback progress) {
    std::vector<uint8_t> content;

    FileInfo info;
    ErrorCode err = getFileInfo(remotePath, info);
    if (err != ErrorCode::OK) return err;

    err = readFileRaw(remotePath, content);
    if (err != ErrorCode::OK) return err;

    FILE *fp = std::fopen(localPath.c_str(), "wb");
    if (!fp) {
        setError("Failed to open local file for writing");
        return ErrorCode::FILE_ERROR;
    }

    size_t written = std::fwrite(content.data(), 1, content.size(), fp);
    std::fclose(fp);

    if (written != content.size()) {
        setError("Failed to write all data to local file");
        return ErrorCode::FILE_ERROR;
    }

    if (progress) progress(content.size(), info.size);
    return ErrorCode::OK;
}

ErrorCode PyBoardUART::uploadFile(const std::string &localPath, const std::string &remotePath,
                                  ProgressCallback progress) {
    FILE *fp = std::fopen(localPath.c_str(), "rb");
    if (!fp) {
        setError("Failed to open local file for reading");
        return ErrorCode::FILE_ERROR;
    }

    std::fseek(fp, 0, SEEK_END);
    size_t fileSize = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);

    std::vector<uint8_t> content(fileSize);
    size_t read = std::fread(content.data(), 1, fileSize, fp);
    std::fclose(fp);

    if (read != fileSize) {
        setError("Failed to read all data from local file");
        return ErrorCode::FILE_ERROR;
    }

    ErrorCode err = writeFileRaw(remotePath, content);
    if (err == ErrorCode::OK && progress) progress(fileSize, fileSize);
    return err;
}

// ============================================================================
// Raw paste (modo RAW) – handshake
// ============================================================================
ErrorCode PyBoardUART::rawPasteWrite(const std::string &data) {
    uint8_t windowBuf[2];
    int readLen = uart_read_bytes(uartNum, windowBuf, 2, 100 / portTICK_PERIOD_MS);
    if (readLen != 2) {
        setError("Failed to read paste mode window size");
        return ErrorCode::UART_ERROR;
    }

    uint16_t windowSize = windowBuf[0] | (windowBuf[1] << 8);
    uint16_t windowRemain = windowSize;

    size_t i = 0;
    while (i < data.length()) {
        size_t available = 0;
        uart_get_buffered_data_len(uartNum, &available);

        while (windowRemain == 0 || available > 0) {
            uint8_t byte;
            int len = uart_read_bytes(uartNum, &byte, 1, 10 / portTICK_PERIOD_MS);

            if (len > 0) {
                if (byte == 0x01) {
                    windowRemain += windowSize;
                } else if (byte == 0x04) {
                    const uint8_t ack = 0x04;
                    uart_write_bytes(uartNum, &ack, 1);
                    return ErrorCode::OK;
                } else {
                    setError("Unexpected byte in paste mode");
                    return ErrorCode::UART_ERROR;
                }
            }
            uart_get_buffered_data_len(uartNum, &available);
        }

        size_t toSend = std::min(static_cast<size_t>(windowRemain), data.length() - i);
        int written = uart_write_bytes(uartNum,
                                       reinterpret_cast<const uint8_t *>(data.c_str() + i),
                                       toSend);
        if (written < 0) {
            setError("Failed to write data in paste mode");
            return ErrorCode::UART_ERROR;
        }

        windowRemain -= written;
        i += written;
    }

    const uint8_t endMarker = 0x04;
    uart_write_bytes(uartNum, &endMarker, 1);

    uint8_t ack;
    int ackLen = uart_read_bytes(uartNum, &ack, 1, 1000 / portTICK_PERIOD_MS);
    if (ackLen != 1 || ack != 0x04) {
        setError("Failed to receive paste mode acknowledgment");
        return ErrorCode::UART_ERROR;
    }
    return ErrorCode::OK;
}

// ============================================================================
// Control
// ============================================================================
ErrorCode PyBoardUART::softReset()   { return writeData(&CTRL_D, 1); }
ErrorCode PyBoardUART::interrupt()   { return writeData(&CTRL_C, 1); }

// ============================================================================
std::string PyBoardUART::errorToString(ErrorCode error) {
    switch (error) {
    case ErrorCode::OK:             return "Success";
    case ErrorCode::TIMEOUT:        return "Timeout";
    case ErrorCode::UART_ERROR:     return "UART error";
    case ErrorCode::REPL_ERROR:     return "REPL error";
    case ErrorCode::EXEC_ERROR:     return "Execution error";
    case ErrorCode::MEMORY_ERROR:   return "Memory allocation error";
    case ErrorCode::FILE_ERROR:     return "File operation error";
    case ErrorCode::INVALID_PARAM:  return "Invalid parameter";
    case ErrorCode::BASE64_ERROR:   return "Base64 encoding/decoding error";
    case ErrorCode::NOT_IN_RAW_REPL:return "Not in raw REPL mode";
    default:                        return "Unknown error";
    }
}

} // namespace PyBoard
