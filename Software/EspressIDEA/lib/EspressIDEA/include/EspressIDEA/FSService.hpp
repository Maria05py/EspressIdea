#pragma once
#include "esp_http_server.h"
#include <string>
#include <vector>

namespace PyBoard { class PyBoardUART; }
class ServerManager;

namespace EspressIDEA {
class ReplControl;

class FSService {
public:
  FSService(PyBoard::PyBoardUART& board, ReplControl& repl, ServerManager& server);
  void registerRoutes(); // /api/fs/*

private:
  // ---- Handlers existentes ----
  static esp_err_t listHandler(httpd_req_t* req);
  static esp_err_t readHandler(httpd_req_t* req);
  static esp_err_t writeHandler(httpd_req_t* req);

  // ---- Handlers nuevos ----
  static esp_err_t existsHandler(httpd_req_t* req);
  static esp_err_t infoHandler  (httpd_req_t* req);
  static esp_err_t deleteHandler(httpd_req_t* req);
  static esp_err_t mkdirHandler (httpd_req_t* req);
  static esp_err_t rmdirHandler (httpd_req_t* req);
  static esp_err_t renameHandler(httpd_req_t* req);
  static esp_err_t downloadHandler(httpd_req_t* req);
  static esp_err_t uploadHandler  (httpd_req_t* req);

  // ---- Utilidades comunes ----
  static FSService* self();
  static void sendJSON(httpd_req_t* req, const std::string& json);
  static std::string esc(const std::string& s);
  static bool queryParam(httpd_req_t* req, const char* key, std::string& out);
  static bool queryParamInt(httpd_req_t* req, const char* key, int& out);

  // Helpers para lógicas compuestas
  static bool rmdirRecursive(FSService* inst, const std::string& path, std::string& err);
  static bool renameFile(FSService* inst, const std::string& from, const std::string& to, std::string& err);

  // Dependencias
  PyBoard::PyBoardUART& board_;
  ReplControl& repl_;
  ServerManager& server_;

  // Singleton simple para callbacks
  static FSService* s_self_;

  // Límites/constantes
  static constexpr size_t MAX_UPLOAD = 2 * 1024 * 1024; // 2 MB por defecto
};

} // namespace EspressIDEA
