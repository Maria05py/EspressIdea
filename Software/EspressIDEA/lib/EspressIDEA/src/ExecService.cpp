#include "EspressIDEA/ExecService.hpp"
#include "EspressIDEA/ReplControl.hpp"
#include "PyBoardUART.hpp"
#include "ServerManager.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace EspressIDEA;

ExecService* ExecService::s_self_ = nullptr;

ExecService::ExecService(PyBoard::PyBoardUART& board, ReplControl& repl, ServerManager& server)
: board_(board), repl_(repl), server_(server) {
  s_self_ = this;
}

ExecService* ExecService::self() { return s_self_; }

void ExecService::sendJSON(httpd_req_t* req, const std::string& json) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json.c_str(), json.size());
}

std::string ExecService::esc(const std::string& s) {
  std::string o; o.reserve(s.size()+8);
  for (char c: s) {
    switch(c){
      case '\\': o+="\\\\"; break; case '"': o+="\\\""; break;
      case '\n': o+="\\n"; break; case '\r': o+="\\r"; break; case '\t': o+="\\t"; break;
      default: o+=c;
    }
  }
  return o;
}

// ==================== /api/exec ====================
esp_err_t ExecService::execHandler(httpd_req_t* req) {
  auto* inst = ExecService::self(); if (!inst) return ESP_FAIL;

  int len = req->content_len;
  if (len <= 0 || len > 64*1024) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body"); return ESP_OK; }
  std::string body; body.resize(len);
  int r = httpd_req_recv(req, body.data(), len);
  if (r <= 0) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv error"); return ESP_OK; }

  ReplControl::ScopedReplLock lock(inst->repl_, "exec");

  // Nota: no forzamos ensureIdle aquí para no resetear el contexto del usuario.
  // Si el usuario necesita parar code.py primero, el frontend llama a /api/repl/ensure_idle.

  std::string out;
  auto rc = inst->board_.exec(body, out, /*timeoutMs*/ 8000);
  bool ok = (rc == PyBoard::ErrorCode::OK);
  std::string j = std::string("{\"ok\":") + (ok?"true":"false") +
                  ",\"stdout\":\"" + esc(out) + "\",\"stderr\":\"" +
                  (ok ? "" : esc(inst->board_.getLastError())) + "\"}";
  sendJSON(req, j);
  return ESP_OK;
}

// ==================== /api/repl/ensure_idle ====================
// Delega en ReplControl para hacer ^C,^D y ESPERAR ">>>"
esp_err_t ExecService::ensureIdleHandler(httpd_req_t* req) {
  auto* inst = ExecService::self(); if (!inst) return ESP_FAIL;

  // ¡OJO! No uses ScopedReplLock aquí: ensureIdleCircuitPython ya toma/soltará el mutex internamente.
  bool ok = inst->repl_.ensureIdleCircuitPython(/*timeout_ms*/ 3000);
  if (!ok) {
    sendJSON(req, "{\"ok\":false,\"error\":\"timeout esperando prompt >>>\"}");
    return ESP_OK;
  }

  sendJSON(req, "{\"ok\":true,\"mode\":\"friendly\"}");
  return ESP_OK;
}

void ExecService::registerRoutes() {
  httpd_uri_t exec = {
    .uri="/api/exec", .method=HTTP_POST, .handler=ExecService::execHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t ensure = {
    .uri="/api/repl/ensure_idle", .method=HTTP_POST, .handler=ExecService::ensureIdleHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };

  server_.registerHttpHandler(exec);
  server_.registerHttpHandler(ensure);
}
