#pragma once
#include "esp_http_server.h"
#include <string>

namespace PyBoard { class PyBoardUART; }
class ServerManager;

namespace EspressIDEA {
class ReplControl;

class ExecService {
public:
  ExecService(PyBoard::PyBoardUART& board, ReplControl& repl, ServerManager& server);
  void registerRoutes(); // /api/exec  + /api/repl/ensure_idle

  // singleton de callbacks
  static ExecService* self();

private:
  // Handlers
  static esp_err_t execHandler(httpd_req_t* req);
  static esp_err_t ensureIdleHandler(httpd_req_t* req);

  // utilidades
  static void sendJSON(httpd_req_t* req, const std::string& json);
  static std::string esc(const std::string& s);

  // dependencias
  PyBoard::PyBoardUART& board_;
  ReplControl& repl_;
  ServerManager& server_;

  static ExecService* s_self_;
};

} // namespace EspressIDEA
