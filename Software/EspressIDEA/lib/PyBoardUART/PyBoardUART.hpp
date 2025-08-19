#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"

namespace PyBoard
{

    // Configuration enums
    enum class BaudRate : int
    {
        BAUD_9600 = 9600,
        BAUD_19200 = 19200,
        BAUD_38400 = 38400,
        BAUD_57600 = 57600,
        BAUD_115200 = 115200,
        BAUD_230400 = 230400,
        BAUD_460800 = 460800,
        BAUD_921600 = 921600
    };

    enum class Timeout : uint32_t
    {
        SHORT = 1000,     // 1 second
        MEDIUM = 5000,    // 5 seconds
        LONG = 10000,     // 10 seconds
        VERY_LONG = 30000 // 30 seconds
    };

    enum class ChunkSize : size_t
    {
        SMALL = 64,
        MEDIUM = 256,
        LARGE = 512,
        VERY_LARGE = 1024
    };

    // Error codes
    enum class ErrorCode
    {
        OK = 0,
        TIMEOUT,
        UART_ERROR,
        REPL_ERROR,
        EXEC_ERROR,
        MEMORY_ERROR,
        FILE_ERROR,
        INVALID_PARAM,
        BASE64_ERROR,
        NOT_IN_RAW_REPL
    };

    // File information structure
    struct FileInfo
    {
        std::string name;
        size_t size;
        bool isDirectory;

        FileInfo(const std::string &n = "", size_t s = 0, bool dir = false)
            : name(n), size(s), isDirectory(dir) {}
    };

    // Callback types
    using DataCallback = std::function<void(const std::string &data)>;
    using ProgressCallback = std::function<void(size_t current, size_t total)>;
    using MonitorCallback = std::function<void(char c)>;

    class PyBoardUART
    {
    private:
        // UART configuration
        uart_port_t uartNum;
        int txPin;
        int rxPin;
        BaudRate baudRate;
        Timeout defaultTimeout;
        ChunkSize chunkSize;

        // State management
        bool inRawRepl;
        bool useRawPaste;
        bool monitorEnabled;

        // Buffers
        std::unique_ptr<uint8_t[]> rxBuffer;
        std::unique_ptr<uint8_t[]> txBuffer;
        static constexpr size_t BUFFER_SIZE = 2048;

        // FreeRTOS handles
        QueueHandle_t eventQueue;
        TaskHandle_t monitorTask;
        MonitorCallback monitorCallback;

        // REPL control characters
        static constexpr uint8_t CTRL_A = 0x01; // Enter raw REPL
        static constexpr uint8_t CTRL_B = 0x02; // Exit raw REPL
        static constexpr uint8_t CTRL_C = 0x03; // Interrupt
        static constexpr uint8_t CTRL_D = 0x04; // Soft reset / EOF
        static constexpr uint8_t CTRL_E = 0x05; // Paste mode

        // Private helper methods
        ErrorCode writeData(const uint8_t *data, size_t len);
        ErrorCode writeData(const std::string &data);
        ErrorCode readUntil(const std::string &ending, std::string &output, uint32_t timeoutMs);
        ErrorCode flushInput();
        ErrorCode execRawNoFollow(const std::string &command);
        ErrorCode follow(std::string &output, std::string &error, uint32_t timeoutMs);
        ErrorCode rawPasteWrite(const std::string &data);

    public:
        // Constructor and destructor
        PyBoardUART(uart_port_t uart = UART_NUM_2,
                    int tx = 21,
                    int rx = 22,
                    BaudRate baud = BaudRate::BAUD_115200,
                    Timeout timeout = Timeout::MEDIUM,
                    ChunkSize chunk = ChunkSize::MEDIUM);
        ~PyBoardUART();

        // Disable copy constructor and assignment
        PyBoardUART(const PyBoardUART &) = delete;
        PyBoardUART &operator=(const PyBoardUART &) = delete;

        // Initialization
        ErrorCode init();
        ErrorCode deinit();

        // Configuration setters
        void setBaudRate(BaudRate baud) { baudRate = baud; }
        void setTimeout(Timeout timeout) { defaultTimeout = timeout; }
        void setChunkSize(ChunkSize chunk) { chunkSize = chunk; }

        // REPL control
        ErrorCode enterRawRepl(bool softReset = true);
        ErrorCode exitRawRepl();
        ErrorCode softReset();
        ErrorCode interrupt();

        bool isInRawRepl() const { return inRawRepl; }
        ErrorCode syncReplCircuitPython(uint32_t timeoutMs);
        ErrorCode waitForReplPrompt(uint32_t timeoutMs);

        // --- Escritura cruda por UART (API pública) ---
        /** Escribe bytes crudos al puerto UART. No agrega terminadores. */
        ErrorCode write(const void *data, size_t len);
        /** Escribe una cadena tal cual (sin CR/LF automático). */
        ErrorCode write(const std::string &data);
        /** Conveniencia para buffers C. Equivalente a write(void*, len). */
        ErrorCode write(const char *data, size_t len);

        // Code execution
        ErrorCode execRaw(const std::string &command, std::string &output, std::string &error, uint32_t timeoutMs = 0);
        ErrorCode exec(const std::string &command, std::string &output, uint32_t timeoutMs = 0);
        ErrorCode exec(const std::string &command); // No output version
        ErrorCode eval(const std::string &expression, std::string &result, uint32_t timeoutMs = 0);
        ErrorCode execPaste(const std::string &code, std::string &output, uint32_t timeoutMs = 0);
        ErrorCode execFriendly(const std::string& command, std::string& output, uint32_t timeoutMs = 0);

        // File system operations with Base64 encoding
        ErrorCode listDir(const std::string &path, std::vector<FileInfo> &files);
        ErrorCode readFile(const std::string &path, std::string &content); // Returns Base64
        ErrorCode writeFile(const std::string &path, const std::string &base64Content);
        // Escribe un trozo binario en 'path'.
// append=false => crea/trunca ('wb'), append=true => agrega ('ab').
ErrorCode writeFileChunk(const std::string& path,
                         const uint8_t* data, size_t len,
                         bool append);

        ErrorCode readFileRaw(const std::string &path, std::vector<uint8_t> &content);
        ErrorCode writeFileRaw(const std::string &path, const std::vector<uint8_t> &content);
        ErrorCode deleteFile(const std::string &path);
        ErrorCode createDir(const std::string &path);
        ErrorCode deleteDir(const std::string &path);
        ErrorCode exists(const std::string &path, bool &result);
        ErrorCode getFileInfo(const std::string &path, FileInfo &info);

        // File transfer with progress
        ErrorCode downloadFile(const std::string &remotePath, const std::string &localPath, ProgressCallback progress = nullptr);
        ErrorCode uploadFile(const std::string &localPath, const std::string &remotePath, ProgressCallback progress = nullptr);

        // Utility
        static std::string errorToString(ErrorCode error);
        std::string getLastError() const { return lastError; }

        // Base64 utilities (public for external use if needed)
        static std::string base64Encode(const std::vector<uint8_t> &data);
        static std::vector<uint8_t> base64Decode(const std::string &encoded);

    private:
        std::string lastError;
        void setError(const std::string &error) { lastError = error; }
    };

} // namespace PyBoard