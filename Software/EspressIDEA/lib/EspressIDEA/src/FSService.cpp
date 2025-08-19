#include "EspressIDEA/FSService.hpp"
#include "EspressIDEA/ReplControl.hpp"
#include "PyBoardUART.hpp"
#include "ServerManager.hpp"

#include <vector>
#include <cstring>
#include <algorithm>  // std::min

using namespace EspressIDEA;

FSService* FSService::s_self_ = nullptr;

FSService::FSService(PyBoard::PyBoardUART& board, ReplControl& repl, ServerManager& server)
: board_(board), repl_(repl), server_(server) {
  s_self_ = this;
}

FSService* FSService::self() { return s_self_; }

void FSService::sendJSON(httpd_req_t* req, const std::string& json) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json.c_str(), json.size());
}

std::string FSService::esc(const std::string& s) {
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

// --- Helpers URL decode / Python quoting ---
static int hexval(char c){
  if (c>='0'&&c<='9') return c-'0';
  if (c>='A'&&c<='F') return 10+(c-'A');
  if (c>='a'&&c<='f') return 10+(c-'a');
  return -1;
}
static std::string urlDecode(const std::string& s){
  std::string o; o.reserve(s.size());
  for (size_t i=0;i<s.size();++i){
    if (s[i]=='%'){
      if (i+2<s.size()){
        int hi = hexval(s[i+1]), lo = hexval(s[i+2]);
        if (hi>=0 && lo>=0){ o.push_back(char((hi<<4)|lo)); i+=2; continue; }
      }
      o.push_back('%');
    } else if (s[i]=='+'){
      o.push_back(' ');
    } else {
      o.push_back(s[i]);
    }
  }
  return o;
}
// Literal de Python entre comillas simples, escapando \ y '
static std::string pyQuote(const std::string& s){
  std::string o; o.reserve(s.size()+2);
  o.push_back('\'');
  for (char c: s){
    switch(c){
      case '\\': o+="\\\\"; break;
      case '\'': o+="\\\'"; break;
      case '\n': o+="\\n"; break;
      case '\r': o+="\\n"; break;
      case '\t': o+="\\t"; break;
      default:   o.push_back(c); break;
    }
  }
  o.push_back('\'');
  return o;
}

bool FSService::queryParam(httpd_req_t* req, const char* key, std::string& out) {
  size_t qlen = httpd_req_get_url_query_len(req) + 1;
  if (qlen <= 1) return false;
  std::vector<char> qbuf(qlen);
  if (httpd_req_get_url_query_str(req, qbuf.data(), qbuf.size()) != ESP_OK) return false;
  char val[256];
  if (httpd_query_key_value(qbuf.data(), key, val, sizeof(val)) == ESP_OK) {
    out = urlDecode(val);
    return true;
  }
  return false;
}

bool FSService::queryParamInt(httpd_req_t* req, const char* key, int& out) {
  std::string s;
  if (!queryParam(req, key, s)) return false;
  char* end=nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (end == s.c_str()) return false;
  out = (int)v;
  return true;
}

// ---------------- list/read/write (base64) ----------------

esp_err_t FSService::listHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;

  std::string path;
  (void) inst->queryParam(req, "path", path);
  if (path.empty()) path = "/";

  // Importante: NO llamar ensureIdle aquí. El frontend ya lo invocó antes (botón STOP).
  ReplControl::ScopedReplLock lock(inst->repl_, "fs.list");

  std::vector<PyBoard::FileInfo> files;
  auto rc = inst->board_.listDir(path, files);
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"") + esc(inst->board_.getLastError()) + "\"}");
    return ESP_OK;
  }

  std::string j = "{\"ok\":true,\"path\":\"" + esc(path) + "\",\"files\":[";
  for (size_t i=0;i<files.size();++i) {
    const auto& f = files[i];
    j += "{\"name\":\"" + esc(f.name) + "\",\"size\":" + std::to_string(f.size) +
         ",\"dir\":" + (f.isDirectory ? "true":"false") + "}";
    if (i+1<files.size()) j += ",";
  }
  j += "]}";
  inst->sendJSON(req, j);
  return ESP_OK;
}

esp_err_t FSService::readHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;

  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.read");

  std::string content_b64;
  auto rc = inst->board_.readFile(path, content_b64); // Base64
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"") + esc(inst->board_.getLastError()) + "\"}");
    return ESP_OK;
  }
  inst->sendJSON(req, std::string("{\"ok\":true,\"path\":\"") + esc(path) + "\",\"base64\":\"" + esc(content_b64) + "\"}");
  return ESP_OK;
}

esp_err_t FSService::writeHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;

  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  int len = req->content_len;
  if (len <= 0 || (size_t)len > MAX_UPLOAD) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body size");
    return ESP_OK;
  }
  std::string body; body.resize(len);
  int r = httpd_req_recv(req, body.data(), len);
  if (r <= 0) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv error"); return ESP_OK; }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.write");

  auto rc = inst->board_.writeFile(path, body); // body (base64)
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"") + esc(inst->board_.getLastError()) + "\"}");
    return ESP_OK;
  }
  inst->sendJSON(req, std::string("{\"ok\":true,\"path\":\"") + esc(path) + "\"}");
  return ESP_OK;
}

// ---------------- exists/info/delete/mkdir/rmdir/rename ----------------

esp_err_t FSService::existsHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.exists");

  bool ex = false;
  auto rc = inst->board_.exists(path, ex);
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
    return ESP_OK;
  }
  inst->sendJSON(req, std::string("{\"ok\":true,\"exists\":") + (ex?"true":"false") + "}");
  return ESP_OK;
}

esp_err_t FSService::infoHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.info");

  PyBoard::FileInfo info;
  auto rc = inst->board_.getFileInfo(path, info);
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
    return ESP_OK;
  }
  std::string j = std::string("{\"ok\":true,\"info\":{")
    + "\"name\":\""+esc(info.name)+"\","
    + "\"size\":"+std::to_string(info.size)+","
    + "\"dir\":"+(info.isDirectory?"true":"false")
    + "}}";
  inst->sendJSON(req, j);
  return ESP_OK;
}

esp_err_t FSService::deleteHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.delete");

  auto rc = inst->board_.deleteFile(path);
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
    return ESP_OK;
  }
  inst->sendJSON(req, "{\"ok\":true}");
  return ESP_OK;
}

esp_err_t FSService::mkdirHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.mkdir");

  auto rc = inst->board_.createDir(path);
  if (rc != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
    return ESP_OK;
  }
  inst->sendJSON(req, "{\"ok\":true}");
  return ESP_OK;
}

// eliminación recursiva (si recursive=1), si no, rmdir simple
bool FSService::rmdirRecursive(FSService* inst, const std::string& path, std::string& err) {
  std::vector<PyBoard::FileInfo> files;
  auto rc = inst->board_.listDir(path, files);
  if (rc != PyBoard::ErrorCode::OK) { err = inst->board_.getLastError(); return false; }

  for (const auto& f : files) {
    std::string p = path;
    if (!p.empty() && p.back() != '/') p += "/";
    p += f.name;
    if (f.isDirectory) {
      if (!rmdirRecursive(inst, p, err)) return false;
    } else {
      rc = inst->board_.deleteFile(p);
      if (rc != PyBoard::ErrorCode::OK) { err = inst->board_.getLastError(); return false; }
    }
  }
  rc = inst->board_.deleteDir(path);
  if (rc != PyBoard::ErrorCode::OK) { err = inst->board_.getLastError(); return false; }
  return true;
}

esp_err_t FSService::rmdirHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }
  // Proteger raíz
  if (path == "/" || path == "//") {
    inst->sendJSON(req, "{\"ok\":false,\"error\":\"cannot remove root\"}");
    return ESP_OK;
  }

  int recursive = 0; (void) inst->queryParamInt(req, "recursive", recursive);

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.rmdir");

  if (recursive) {
    std::string err;
    bool ok = rmdirRecursive(inst, path, err);
    if (!ok) {
      inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(err)+"\"}");
      return ESP_OK;
    }
  } else {
    auto rc = inst->board_.deleteDir(path);
    if (rc != PyBoard::ErrorCode::OK) {
      inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
      return ESP_OK;
    }
  }
  inst->sendJSON(req, "{\"ok\":true}");
  return ESP_OK;
}

// rename de archivo (no directorio): from -> to
bool FSService::renameFile(FSService* inst, const std::string& from, const std::string& to, std::string& err) {
  // leer raw
  std::vector<uint8_t> data;
  auto rc = inst->board_.readFileRaw(from, data);
  if (rc != PyBoard::ErrorCode::OK) { err = inst->board_.getLastError(); return false; }
  // escribir destino
  rc = inst->board_.writeFileRaw(to, data);
  if (rc != PyBoard::ErrorCode::OK) { err = inst->board_.getLastError(); return false; }
  // borrar origen
  rc = inst->board_.deleteFile(from);
  if (rc != PyBoard::ErrorCode::OK) { err = inst->board_.getLastError(); return false; }
  return true;
}

esp_err_t FSService::renameHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string fromPath, toPath;
  if (!inst->queryParam(req, "from", fromPath) || !inst->queryParam(req, "to", toPath) ||
      fromPath.empty() || toPath.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing from/to");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.rename");

  // asegurar que 'from' sea archivo (no dir)
  PyBoard::FileInfo info;
  auto rcFI = inst->board_.getFileInfo(fromPath, info);
  if (rcFI != PyBoard::ErrorCode::OK) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
    return ESP_OK;
  }
  if (info.isDirectory) {
    inst->sendJSON(req, "{\"ok\":false,\"error\":\"rename de directorios no soportado\"}");
    return ESP_OK;
  }

  // Fast path: intenta os.rename (si el FS lo soporta)
  {
    std::string py = "import os\n"
                     "try:\n"
                     "  os.rename(" + pyQuote(fromPath) + "," + pyQuote(toPath) + ")\n"
                     "  print('OK')\n"
                     "except Exception as e:\n"
                     "  print('FAIL')\n";
    std::string out;
    auto rc = inst->board_.exec(py, out);
    if (rc == PyBoard::ErrorCode::OK && out.find("OK") != std::string::npos) {
      inst->sendJSON(req, "{\"ok\":true}");
      return ESP_OK;
    }
    // Si falló, cae al método por copia
  }

  std::string err;
  bool ok = renameFile(inst, fromPath, toPath, err);
  if (!ok) {
    inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(err)+"\"}");
    return ESP_OK;
  }
  inst->sendJSON(req, "{\"ok\":true}");
  return ESP_OK;
}

// ---------------- download/upload (raw) ----------------

esp_err_t FSService::downloadHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;
  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.download");

  std::vector<uint8_t> data;
  auto rc = inst->board_.readFileRaw(path, data);
  if (rc != PyBoard::ErrorCode::OK) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, inst->board_.getLastError().c_str());
    return ESP_OK;
  }

  // Tipo y Content-Disposition (filename)
  httpd_resp_set_type(req, "application/octet-stream");
  std::string fname = path;
  auto slash = fname.find_last_of('/');
  if (slash != std::string::npos) fname = fname.substr(slash+1);
  std::string cd = "attachment; filename=\"" + fname + "\"";
  httpd_resp_set_hdr(req, "Content-Disposition", cd.c_str());

  const size_t CH = 1400;
  size_t off = 0;
  while (off < data.size()) {
    size_t n = std::min(CH, data.size() - off);
    esp_err_t er = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(data.data() + off), n);
    if (er != ESP_OK) { httpd_resp_send_chunk(req, nullptr, 0); return ESP_OK; }
    off += n;
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t FSService::uploadHandler(httpd_req_t* req) {
  auto* inst = FSService::self(); if (!inst) return ESP_FAIL;

  std::string path;
  if (!inst->queryParam(req, "path", path) || path.empty()) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing path");
    return ESP_OK;
  }

  int appendInt = 0;
  (void) inst->queryParamInt(req, "append", appendInt);
  bool append = appendInt != 0;

  int remaining = req->content_len;
  if (remaining <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body size");
    return ESP_OK;
  }

  ReplControl::ScopedReplLock lock(inst->repl_, "fs.upload");

  const size_t CH = 8 * 1024; // trozo de lectura del HTTP
  std::vector<uint8_t> buf(CH);

  while (remaining > 0) {
    int toRead = std::min<int>(remaining, (int)CH);
    int r = httpd_req_recv(req, reinterpret_cast<char*>(buf.data()), toRead);
    if (r <= 0) {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv error");
      return ESP_OK;
    }

    auto rc = inst->board_.writeFileChunk(path, buf.data(), (size_t)r, append);
    if (rc != PyBoard::ErrorCode::OK) {
      inst->sendJSON(req, std::string("{\"ok\":false,\"error\":\"")+esc(inst->board_.getLastError())+"\"}");
      return ESP_OK;
    }
    remaining -= r;
    append = true; // el resto siempre en append
  }

  inst->sendJSON(req, std::string("{\"ok\":true,\"path\":\"")+esc(path)+"\"}");
  return ESP_OK;
}

// ---------------- Registro de rutas ----------------

void FSService::registerRoutes() {
  // existentes
  httpd_uri_t list = {
    .uri="/api/fs/list", .method=HTTP_GET, .handler=FSService::listHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t read = {
    .uri="/api/fs/read", .method=HTTP_GET, .handler=FSService::readHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t write= {
    .uri="/api/fs/write", .method=HTTP_POST, .handler=FSService::writeHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };

  // nuevos
  httpd_uri_t exists = {
    .uri="/api/fs/exists", .method=HTTP_GET, .handler=FSService::existsHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t info = {
    .uri="/api/fs/info", .method=HTTP_GET, .handler=FSService::infoHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t del = {
    .uri="/api/fs/delete", .method=HTTP_POST, .handler=FSService::deleteHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t mkdir = {
    .uri="/api/fs/mkdir", .method=HTTP_POST, .handler=FSService::mkdirHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t rmdir = {
    .uri="/api/fs/rmdir", .method=HTTP_POST, .handler=FSService::rmdirHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t rename = {
    .uri="/api/fs/rename", .method=HTTP_POST, .handler=FSService::renameHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t download = {
    .uri="/api/fs/download", .method=HTTP_GET, .handler=FSService::downloadHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t upload = {
    .uri="/api/fs/upload", .method=HTTP_POST, .handler=FSService::uploadHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };

  server_.registerHttpHandler(list);
  server_.registerHttpHandler(read);
  server_.registerHttpHandler(write);

  server_.registerHttpHandler(exists);
  server_.registerHttpHandler(info);
  server_.registerHttpHandler(del);
  server_.registerHttpHandler(mkdir);
  server_.registerHttpHandler(rmdir);
  server_.registerHttpHandler(rename);
  server_.registerHttpHandler(download);
  server_.registerHttpHandler(upload);
}
