#include "PyBoardUART.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "PyBoardUART";

namespace PyBoard
{

    // Base64 encoding table
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Constructor
    PyBoardUART::PyBoardUART(uart_port_t uart, int tx, int rx, BaudRate baud,
                             Timeout timeout, ChunkSize chunk)
        : uartNum(uart), txPin(tx), rxPin(rx), baudRate(baud),
          defaultTimeout(timeout), chunkSize(chunk),
          inRawRepl(false), useRawPaste(true), monitorEnabled(false),
          eventQueue(nullptr), monitorTask(nullptr)
    {

        rxBuffer = std::make_unique<uint8_t[]>(BUFFER_SIZE);
        txBuffer = std::make_unique<uint8_t[]>(BUFFER_SIZE);
    }

    // Destructor
    PyBoardUART::~PyBoardUART()
    {
        deinit();
    }

    // Initialize UART
    ErrorCode PyBoardUART::init()
    {
        // Configure UART parameters
        uart_config_t uart_config = {
            .baud_rate = static_cast<int>(baudRate),
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_APB,
        };

        // Install UART driver
        esp_err_t err = uart_driver_install(uartNum, BUFFER_SIZE * 2,
                                            BUFFER_SIZE * 2, 20, &eventQueue, 0);
        if (err != ESP_OK)
        {
            setError("Failed to install UART driver");
            return ErrorCode::UART_ERROR;
        }

        // Configure UART parameters
        err = uart_param_config(uartNum, &uart_config);
        if (err != ESP_OK)
        {
            uart_driver_delete(uartNum);
            setError("Failed to configure UART parameters");
            return ErrorCode::UART_ERROR;
        }

        // Set UART pins
        err = uart_set_pin(uartNum, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK)
        {
            uart_driver_delete(uartNum);
            setError("Failed to set UART pins");
            return ErrorCode::UART_ERROR;
        }

        ESP_LOGI(TAG, "Initialized on UART%d (TX:%d, RX:%d, Baud:%d)",
                 uartNum, txPin, rxPin, static_cast<int>(baudRate));

        return ErrorCode::OK;
    }

    // Deinitialize UART
    ErrorCode PyBoardUART::deinit()
    {

        if (eventQueue != nullptr)
        {
            uart_driver_delete(uartNum);
            eventQueue = nullptr;
        }

        ESP_LOGI(TAG, "Deinitialized");
        return ErrorCode::OK;
    }

    // Write data to UART
    ErrorCode PyBoardUART::writeData(const uint8_t *data, size_t len)
    {
        int written = uart_write_bytes(uartNum, data, len);
        if (written != static_cast<int>(len))
        {
            setError("Failed to write all bytes to UART");
            return ErrorCode::UART_ERROR;
        }
        return ErrorCode::OK;
    }

    ErrorCode PyBoardUART::writeData(const std::string &data)
    {
        return writeData(reinterpret_cast<const uint8_t *>(data.c_str()), data.length());
    }

    // --- API pública de escritura cruda ---
    PyBoard::ErrorCode PyBoardUART::write(const void *data, size_t len)
    {
        if (!data || len == 0)
            return ErrorCode::OK;
        return writeData(reinterpret_cast<const uint8_t *>(data), len);
    }

    PyBoard::ErrorCode PyBoardUART::write(const std::string &data)
    {
        if (data.empty())
            return ErrorCode::OK;
        return writeData(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    }

    PyBoard::ErrorCode PyBoardUART::write(const char *data, size_t len)
    {
        if (!data || len == 0)
            return ErrorCode::OK;
        return writeData(reinterpret_cast<const uint8_t *>(data), len);
    }

    // Read until ending sequence
    ErrorCode PyBoardUART::readUntil(const std::string &ending, std::string &output, uint32_t timeoutMs)
    {
        output.clear();
        uint32_t startTime = xTaskGetTickCount();

        while (true)
        {
            uint8_t byte;
            int len = uart_read_bytes(uartNum, &byte, 1, 10 / portTICK_PERIOD_MS);

            if (len > 0)
            {
                output += static_cast<char>(byte);

                // Check if we have the ending sequence
                if (output.length() >= ending.length())
                {
                    if (output.substr(output.length() - ending.length()) == ending)
                    {
                        return ErrorCode::OK;
                    }
                }
            }

            // Check timeout
            if ((xTaskGetTickCount() - startTime) * portTICK_PERIOD_MS > timeoutMs)
            {
                setError("Timeout waiting for: " + ending);
                return ErrorCode::TIMEOUT;
            }

            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }

    // Flush input buffer
    ErrorCode PyBoardUART::flushInput()
    {
        uart_flush_input(uartNum);
        return ErrorCode::OK;
    }

    // Enter raw REPL mode
    ErrorCode PyBoardUART::enterRawRepl(bool softReset)
    {
        ErrorCode err;
        std::string response;

        // Send Ctrl-C to interrupt any running program
        uint8_t ctrlC[] = {'\r', CTRL_C};
        err = writeData(ctrlC, sizeof(ctrlC));
        if (err != ErrorCode::OK)
            return err;

        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Flush input
        err = flushInput();
        if (err != ErrorCode::OK)
            return err;

        // Send Ctrl-A to enter raw REPL
        uint8_t ctrlA[] = {'\r', CTRL_A};
        err = writeData(ctrlA, sizeof(ctrlA));
        if (err != ErrorCode::OK)
            return err;

        // Wait for raw REPL prompt
        err = readUntil("raw REPL; CTRL-B to exit\r\n>", response, static_cast<uint32_t>(defaultTimeout));
        if (err != ErrorCode::OK)
        {
            setError("Failed to enter raw REPL");
            return ErrorCode::REPL_ERROR;
        }

        if (softReset)
        {
            // Send Ctrl-D for soft reset
            err = writeData(&CTRL_D, 1);
            if (err != ErrorCode::OK)
                return err;

            // Wait for soft reboot message
            err = readUntil("soft reboot\r\n", response, static_cast<uint32_t>(defaultTimeout));
            if (err != ErrorCode::OK)
            {
                setError("Soft reset failed");
                return ErrorCode::REPL_ERROR;
            }

            // Wait for raw REPL prompt again
            err = readUntil("raw REPL; CTRL-B to exit\r\n", response, static_cast<uint32_t>(defaultTimeout));
            if (err != ErrorCode::OK)
            {
                return ErrorCode::REPL_ERROR;
            }
        }

        inRawRepl = true;
        ESP_LOGI(TAG, "Entered raw REPL mode");

        return ErrorCode::OK;
    }

    // Exit raw REPL mode
    ErrorCode PyBoardUART::exitRawRepl()
    {
        uint8_t ctrlB[] = {'\r', CTRL_B};
        ErrorCode err = writeData(ctrlB, sizeof(ctrlB));
        if (err != ErrorCode::OK)
            return err;

        inRawRepl = false;
        ESP_LOGI(TAG, "Exited raw REPL mode");

        return ErrorCode::OK;
    }

    // Execute raw command without following output
    ErrorCode PyBoardUART::execRawNoFollow(const std::string &command)
    {
        if (!inRawRepl)
        {
            setError("Not in raw REPL mode");
            return ErrorCode::NOT_IN_RAW_REPL;
        }

        std::string response;

        // Wait for prompt
        ErrorCode err = readUntil(">", response, static_cast<uint32_t>(defaultTimeout));
        if (err != ErrorCode::OK)
            return err;

        // Try raw paste mode first
        if (useRawPaste)
        {
            uint8_t pasteCmd[] = {CTRL_E, 'A', 0x01};
            err = writeData(pasteCmd, sizeof(pasteCmd));
            if (err != ErrorCode::OK)
                return err;

            uint8_t pasteResponse[2];
            int len = uart_read_bytes(uartNum, pasteResponse, 2, 100 / portTICK_PERIOD_MS);

            if (len == 2 && pasteResponse[0] == 'R')
            {
                if (pasteResponse[1] == 0x01)
                {
                    // Raw paste mode supported
                    return rawPasteWrite(command);
                }
                else
                {
                    // Raw paste not supported
                    useRawPaste = false;
                }
            }
        }

        // Fall back to normal mode
        size_t chunkSizeVal = static_cast<size_t>(chunkSize);
        for (size_t i = 0; i < command.length(); i += chunkSizeVal)
        {
            size_t len = std::min(chunkSizeVal, command.length() - i);
            err = writeData(reinterpret_cast<const uint8_t *>(command.c_str() + i), len);
            if (err != ErrorCode::OK)
                return err;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        // Send Ctrl-D to execute
        err = writeData(&CTRL_D, 1);
        if (err != ErrorCode::OK)
            return err;

        // Check if command was accepted
        uint8_t okResponse[2];
        int okLen = uart_read_bytes(uartNum, okResponse, 2, 100 / portTICK_PERIOD_MS);
        if (okLen != 2 || okResponse[0] != 'O' || okResponse[1] != 'K')
        {
            setError("Command not accepted by device");
            return ErrorCode::EXEC_ERROR;
        }

        return ErrorCode::OK;
    }

    // Follow output after command execution
    ErrorCode PyBoardUART::follow(std::string &output, std::string &error, uint32_t timeoutMs)
    {
        ErrorCode err;

        // Read normal output
        err = readUntil("\x04", output, timeoutMs);
        if (err != ErrorCode::OK)
            return err;

        // Remove the \x04 at the end
        if (!output.empty() && output.back() == '\x04')
        {
            output.pop_back();
        }

        // Read error output
        err = readUntil("\x04", error, timeoutMs);
        if (err != ErrorCode::OK)
            return err;

        // Remove the \x04 at the end
        if (!error.empty() && error.back() == '\x04')
        {
            error.pop_back();
        }

        return ErrorCode::OK;
    }

    // Execute raw command
    ErrorCode PyBoardUART::execRaw(const std::string &command, std::string &output,
                                   std::string &error, uint32_t timeoutMs)
    {
        if (timeoutMs == 0)
        {
            timeoutMs = static_cast<uint32_t>(defaultTimeout);
        }

        ErrorCode err = execRawNoFollow(command);
        if (err != ErrorCode::OK)
            return err;

        return follow(output, error, timeoutMs);
    }

    // Execute command with automatic error handling
    ErrorCode PyBoardUART::exec(const std::string &command, std::string &output, uint32_t timeoutMs)
    {
        std::string error;
        ErrorCode err = execRaw(command, output, error, timeoutMs);

        if (!error.empty())
        {
            ESP_LOGE(TAG, "Execution error: %s", error.c_str());
            setError(error);
            if (err == ErrorCode::OK)
            {
                err = ErrorCode::EXEC_ERROR;
            }
        }

        return err;
    }

    // Execute command without output
    ErrorCode PyBoardUART::exec(const std::string &command)
    {
        std::string output;
        return exec(command, output);
    }

    // Evaluate expression
    ErrorCode PyBoardUART::eval(const std::string &expression, std::string &result, uint32_t timeoutMs)
    {
        std::string command = "print(repr(" + expression + "))";
        ErrorCode err = exec(command, result, timeoutMs);

        // Strip newline if present
        if (err == ErrorCode::OK && !result.empty() && result.back() == '\n')
        {
            result.pop_back();
        }

        return err;
    }

    // Base64 encoding
    std::string PyBoardUART::base64Encode(const std::vector<uint8_t> &data)
    {
        std::string encoded;
        int val = 0, valb = -6;

        for (uint8_t c : data)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }

        if (valb > -6)
        {
            encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }

        while (encoded.size() % 4)
        {
            encoded.push_back('=');
        }

        return encoded;
    }

    // Base64 decoding
    std::vector<uint8_t> PyBoardUART::base64Decode(const std::string &encoded)
    {
        std::vector<uint8_t> decoded;
        std::vector<int> T(256, -1);

        for (int i = 0; i < 64; i++)
        {
            T[base64_chars[i]] = i;
        }

        int val = 0, valb = -8;
        for (char c : encoded)
        {
            if (T[c] == -1)
                break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0)
            {
                decoded.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }

        return decoded;
    }

    // List directory
    ErrorCode PyBoardUART::listDir(const std::string &path, std::vector<FileInfo> &files)
    {
        files.clear();

        std::stringstream cmd;
        cmd << "import os\n";
        cmd << "for f in os.ilistdir(" << (path.empty() ? "" : "'" + path + "'") << "):\n";
        cmd << " print(f[0]+'|'+str(f[1])+'|'+str(f[3] if len(f)>3 else 0))";

        std::string output;
        ErrorCode err = exec(cmd.str(), output);
        if (err != ErrorCode::OK)
            return err;

        // Parse output
        std::istringstream stream(output);
        std::string line;

        while (std::getline(stream, line))
        {
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);

            if (pos1 != std::string::npos && pos2 != std::string::npos)
            {
                std::string name = line.substr(0, pos1);
                int type = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
                size_t size = std::stoull(line.substr(pos2 + 1));

                files.emplace_back(name, size, (type & 0x4000) != 0);
            }
        }

        return ErrorCode::OK;
    }

    // Read file and return as Base64
    ErrorCode PyBoardUART::readFile(const std::string &path, std::string &content)
    {
        std::vector<uint8_t> rawContent;
        ErrorCode err = readFileRaw(path, rawContent);
        if (err != ErrorCode::OK)
            return err;

        content = base64Encode(rawContent);
        return ErrorCode::OK;
    }

    // Write file from Base64 content
    ErrorCode PyBoardUART::writeFile(const std::string &path, const std::string &base64Content)
    {
        std::vector<uint8_t> rawContent = base64Decode(base64Content);
        return writeFileRaw(path, rawContent);
    }

    // Read file raw
    ErrorCode PyBoardUART::readFileRaw(const std::string &path, std::vector<uint8_t> &content)
    {
        content.clear();

        // First, get file size
        FileInfo info;
        ErrorCode err = getFileInfo(path, info);
        if (err != ErrorCode::OK)
            return err;

        // Open file for reading
        std::string openCmd = "f=open('" + path + "','rb')\nr=f.read";
        err = exec(openCmd);
        if (err != ErrorCode::OK)
            return err;

        // Read file in chunks
        size_t chunkSizeVal = static_cast<size_t>(chunkSize);
        std::string output;

        while (true)
        {
            std::string readCmd = "import ubinascii\nprint(ubinascii.b2a_base64(r(" +
                                  std::to_string(chunkSizeVal) + ")).decode().strip())";

            err = exec(readCmd, output);
            if (err != ErrorCode::OK)
                break;

            if (output.empty() || output == "None")
                break;

            // Decode Base64 chunk
            std::vector<uint8_t> chunk = base64Decode(output);
            content.insert(content.end(), chunk.begin(), chunk.end());

            if (chunk.size() < chunkSizeVal)
                break;
        }

        // Close file
        exec("f.close()");

        return ErrorCode::OK;
    }

    // Write file raw
    ErrorCode PyBoardUART::writeFileRaw(const std::string &path, const std::vector<uint8_t> &content)
    {
        // Open file for writing
        std::string openCmd = "f=open('" + path + "','wb')\nw=f.write";
        ErrorCode err = exec(openCmd);
        if (err != ErrorCode::OK)
            return err;

        // Write file in chunks
        size_t chunkSizeVal = static_cast<size_t>(chunkSize);

        for (size_t i = 0; i < content.size(); i += chunkSizeVal)
        {
            size_t len = std::min(chunkSizeVal, content.size() - i);
            std::vector<uint8_t> chunk(content.begin() + i, content.begin() + i + len);

            std::string base64Chunk = base64Encode(chunk);
            std::string writeCmd = "import ubinascii\nw(ubinascii.a2b_base64('" + base64Chunk + "'))";

            err = exec(writeCmd);
            if (err != ErrorCode::OK)
            {
                exec("f.close()");
                return err;
            }
        }

        // Close file
        return exec("f.close()");
    }

    // Delete file
    ErrorCode PyBoardUART::deleteFile(const std::string &path)
    {
        std::string cmd = "import os\nos.remove('" + path + "')";
        return exec(cmd);
    }

    // Create directory
    ErrorCode PyBoardUART::createDir(const std::string &path)
    {
        std::string cmd = "import os\nos.mkdir('" + path + "')";
        return exec(cmd);
    }

    // Delete directory
    ErrorCode PyBoardUART::deleteDir(const std::string &path)
    {
        std::string cmd = "import os\nos.rmdir('" + path + "')";
        return exec(cmd);
    }

    // Check if file/directory exists
    ErrorCode PyBoardUART::exists(const std::string &path, bool &result)
    {
        std::string cmd = "import os\ntry:\n os.stat('" + path + "')\n print('1')\nexcept:\n print('0')";
        std::string output;

        ErrorCode err = exec(cmd, output);
        if (err != ErrorCode::OK)
            return err;

        result = (!output.empty() && output[0] == '1');
        return ErrorCode::OK;
    }

    // Get file information
    ErrorCode PyBoardUART::getFileInfo(const std::string &path, FileInfo &info)
    {
        std::string cmd = "import os\ns=os.stat('" + path + "')\nprint(s[0],s[6])";
        std::string output;

        ErrorCode err = exec(cmd, output);
        if (err != ErrorCode::OK)
            return err;

        int mode, size;
        if (sscanf(output.c_str(), "%d %d", &mode, &size) == 2)
        {
            info.name = path;
            info.isDirectory = (mode & 0x4000) != 0;
            info.size = size;
            return ErrorCode::OK;
        }

        setError("Failed to parse file info");
        return ErrorCode::EXEC_ERROR;
    }

    // Download file from device to local system
    ErrorCode PyBoardUART::downloadFile(const std::string &remotePath, const std::string &localPath,
                                        ProgressCallback progress)
    {
        std::vector<uint8_t> content;

        // Get file info for progress reporting
        FileInfo info;
        ErrorCode err = getFileInfo(remotePath, info);
        if (err != ErrorCode::OK)
            return err;

        // Read file content
        err = readFileRaw(remotePath, content);
        if (err != ErrorCode::OK)
            return err;

        // Write to local file
        FILE *fp = fopen(localPath.c_str(), "wb");
        if (!fp)
        {
            setError("Failed to open local file for writing");
            return ErrorCode::FILE_ERROR;
        }

        size_t written = fwrite(content.data(), 1, content.size(), fp);
        fclose(fp);

        if (written != content.size())
        {
            setError("Failed to write all data to local file");
            return ErrorCode::FILE_ERROR;
        }

        if (progress)
        {
            progress(content.size(), info.size);
        }

        return ErrorCode::OK;
    }

    // Upload file from local system to device
    ErrorCode PyBoardUART::uploadFile(const std::string &localPath, const std::string &remotePath,
                                      ProgressCallback progress)
    {
        // Read local file
        FILE *fp = fopen(localPath.c_str(), "rb");
        if (!fp)
        {
            setError("Failed to open local file for reading");
            return ErrorCode::FILE_ERROR;
        }

        // Get file size
        fseek(fp, 0, SEEK_END);
        size_t fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        // Read file content
        std::vector<uint8_t> content(fileSize);
        size_t read = fread(content.data(), 1, fileSize, fp);
        fclose(fp);

        if (read != fileSize)
        {
            setError("Failed to read all data from local file");
            return ErrorCode::FILE_ERROR;
        }

        // Write to device
        ErrorCode err = writeFileRaw(remotePath, content);

        if (err == ErrorCode::OK && progress)
        {
            progress(fileSize, fileSize);
        }

        return err;
    }

    // Raw paste write implementation
    ErrorCode PyBoardUART::rawPasteWrite(const std::string &data)
    {
        // Read window size
        uint8_t windowBuf[2];
        int readLen = uart_read_bytes(uartNum, windowBuf, 2, 100 / portTICK_PERIOD_MS);
        if (readLen != 2)
        {
            setError("Failed to read paste mode window size");
            return ErrorCode::UART_ERROR;
        }

        uint16_t windowSize = windowBuf[0] | (windowBuf[1] << 8);
        uint16_t windowRemain = windowSize;

        size_t i = 0;
        while (i < data.length())
        {
            // Check for flow control
            size_t available = 0;
            uart_get_buffered_data_len(uartNum, &available);

            while (windowRemain == 0 || available > 0)
            {
                uint8_t byte;
                int len = uart_read_bytes(uartNum, &byte, 1, 10 / portTICK_PERIOD_MS);

                if (len > 0)
                {
                    if (byte == 0x01)
                    {
                        windowRemain += windowSize;
                    }
                    else if (byte == 0x04)
                    {
                        // Abrupt end
                        const uint8_t ack = 0x04;
                        uart_write_bytes(uartNum, &ack, 1);
                        return ErrorCode::OK;
                    }
                    else
                    {
                        setError("Unexpected byte in paste mode");
                        return ErrorCode::UART_ERROR;
                    }
                }

                // Update available data length
                uart_get_buffered_data_len(uartNum, &available);
            }

            // Send data
            size_t toSend = std::min(static_cast<size_t>(windowRemain), data.length() - i);
            int written = uart_write_bytes(uartNum,
                                           reinterpret_cast<const uint8_t *>(data.c_str() + i),
                                           toSend);
            if (written < 0)
            {
                setError("Failed to write data in paste mode");
                return ErrorCode::UART_ERROR;
            }

            windowRemain -= written;
            i += written;
        }

        // Send end marker
        const uint8_t endMarker = 0x04;
        uart_write_bytes(uartNum, &endMarker, 1);

        // Wait for acknowledgment
        uint8_t ack;
        int ackLen = uart_read_bytes(uartNum, &ack, 1, 1000 / portTICK_PERIOD_MS);
        if (ackLen != 1 || ack != 0x04)
        {
            setError("Failed to receive paste mode acknowledgment");
            return ErrorCode::UART_ERROR;
        }

        return ErrorCode::OK;
    }

    // Soft reset
    ErrorCode PyBoardUART::softReset()
    {
        return writeData(&CTRL_D, 1);
    }

    // Interrupt
    ErrorCode PyBoardUART::interrupt()
    {
        return writeData(&CTRL_C, 1);
    }

    // Error to string conversion
    std::string PyBoardUART::errorToString(ErrorCode error)
    {
        switch (error)
        {
        case ErrorCode::OK:
            return "Success";
        case ErrorCode::TIMEOUT:
            return "Timeout";
        case ErrorCode::UART_ERROR:
            return "UART error";
        case ErrorCode::REPL_ERROR:
            return "REPL error";
        case ErrorCode::EXEC_ERROR:
            return "Execution error";
        case ErrorCode::MEMORY_ERROR:
            return "Memory allocation error";
        case ErrorCode::FILE_ERROR:
            return "File operation error";
        case ErrorCode::INVALID_PARAM:
            return "Invalid parameter";
        case ErrorCode::BASE64_ERROR:
            return "Base64 encoding/decoding error";
        case ErrorCode::NOT_IN_RAW_REPL:
            return "Not in raw REPL mode";
        default:
            return "Unknown error";
        }
    }

} // namespace PyBoard