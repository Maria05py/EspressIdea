#pragma once

#include "Pyboard.hpp"
#include <string>
#include <vector>

class EspressIDEA {
public:
    EspressIDEA(Pyboard pyboard);

    // Inicializa el dispositivo (UART, REPL, etc.)
    void begin();

    // Ejecuta un bloque de código en el dispositivo
    bool run_script(const std::string& code);

    // Envía el script en modo paste
    bool paste_script(const std::string& code);

    // Reinicia el dispositivo (soft)
    void soft_reset();

    // Abre un archivo remoto y lo imprime
    std::string read_file(const std::string& path);

    // Escribe un archivo completo (sobrescribe)
    bool write_file(const std::string& path, const std::string& content);

    // Elimina un archivo
    bool delete_file(const std::string& path);

    bool make_dir(const std::string& path);

    bool touch_file(const std::string& path);

    bool copy_file(const std::string& src, const std::string& dest);

    bool upload_base64(const std::string& path, const std::string& b64);

    std:: string download_base64(const std::string& path);

    std::vector<std::string> list_files(const std::string& path);

    std::string encode_base64(const std::string& raw);

    std::string decode_base64(const std::string& b64);

    // Cierra la conexión
    void end();

private:
    Pyboard _py;
    bool _initialized = false;

    // Helpers internos
    void ensure_ready();
};
