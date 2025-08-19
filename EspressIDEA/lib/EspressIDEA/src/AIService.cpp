#include "EspressIDEA/AIService.hpp"
#include "ServerManager.hpp"

#include <vector>
#include <cstring>
#include <cstdio>
#include <cctype>

#include "esp_log.h"
#include "esp_http_client.h"

using namespace EspressIDEA;
static const char* TAG = "AIService";

AIService* AIService::s_self_ = nullptr;

static std::string trim_copy(const std::string& in) {
  size_t i=0, j=in.size();
  while (i<j && (in[i]==' '||in[i]=='\t'||in[i]=='\r'||in[i]=='\n')) ++i;
  while (j>i && (in[j-1]==' '||in[j-1]=='\t'||in[j-1]=='\r'||in[j-1]=='\n')) --j;
  return (i<j) ? in.substr(i, j-i) : std::string();
}

bool AIService::loadLLMUrlFromCredentials(const char* credentialsPath) {
  llm_url_.clear();

  FILE* f = fopen(credentialsPath, "r");
  if (!f) {
    ESP_LOGW(TAG, "No pude abrir %s", credentialsPath);
    return false;
  }
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    // Aceptamos LLM_URL=... o AI_URL=...
    if (strncmp(line, "LLM_URL=", 8) == 0) {
      llm_url_ = trim_copy(std::string(line + 8));
    } else if (strncmp(line, "AI_URL=", 7) == 0) {
      llm_url_ = trim_copy(std::string(line + 7));
    }
  }
  fclose(f);

  if (!llm_url_.empty()) {
    // Si solo dan host:puerto, añadimos /generar por defecto (o lo dejamos a handler).
    // Mejor mantener limpio aquí: el handler forma la URL final.
    ESP_LOGI(TAG, "LLM_URL configurado: %s", llm_url_.c_str());
    return true;
  }
  ESP_LOGW(TAG, "No encontré LLM_URL= ni AI_URL= en %s", credentialsPath);
  return false;
}

void AIService::registerRoutes() {
  s_self_ = this;

  httpd_uri_t ping = {
    .uri="/api/ai/ping", .method=HTTP_GET, .handler=AIService::pingHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };
  httpd_uri_t gen = {
    .uri="/api/ai/generate", .method=HTTP_POST, .handler=AIService::generateHandler, .user_ctx=nullptr,
    .is_websocket=false, .handle_ws_control_frames=false, .supported_subprotocol=nullptr
  };

  server_.registerHttpHandler(ping);
  server_.registerHttpHandler(gen);
}

AIService* AIService::self() { return s_self_; }

void AIService::sendJSON(httpd_req_t* req, const std::string& json) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json.c_str(), json.size());
}

std::string AIService::esc(const std::string& s) {
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

static int hexval(char c){
  if (c>='0'&&c<='9') return c-'0';
  if (c>='A'&&c<='F') return 10+(c-'A');
  if (c>='a'&&c<='f') return 10+(c-'a');
  return -1;
}
std::string AIService::urlDecode(const std::string& s){
  std::string o; o.reserve(s.size());
  for (size_t i=0;i<s.size();++i){
    if (s[i]=='%'){
      if (i+2<s.size()){
        int hi = hexval(s[i+1]), lo = hexval(s[i+2]);
        if (hi>=0 && lo>=0){ o.push_back(char((hi<<4)|lo)); i+=2; continue; }
      }
      o.push_back('%');
    } else if (s[i]=='+'){ o.push_back(' '); }
    else { o.push_back(s[i]); }
  }
  return o;
}

bool AIService::queryParam(httpd_req_t* req, const char* key, std::string& out) {
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

// ---- HTTP client util para POST /generar ----

struct RespBuf {
  std::string data;
};

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data && evt->data_len > 0) {
    auto* rb = static_cast<RespBuf*>(evt->user_data);
    rb->data.append(reinterpret_cast<const char*>(evt->data), evt->data_len);
  }
  return ESP_OK;
}

bool AIService::postToLLM(const std::string& mensaje,
                          const std::string& placa,
                          const std::string& modelo,
                          std::string& responseJson)
{
  responseJson.clear();
  if (llm_url_.empty()) {
    ESP_LOGW(TAG, "LLM_URL no configurada");
    return false;
  }

  // Aceptamos que llm_url_ sea (por ejemplo):
  //   http://192.168.1.50:5000         -> añadimos /generar
  //   http://192.168.1.50:5000/generar -> usamos tal cual
  std::string url = llm_url_;
  if (url.find("/generar") == std::string::npos) {
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/generar";
  }

  // Construimos el JSON esperado por tu servidor Flask
  // { "mensaje": "...", "placa": "ideaboard", "modelo": "gemini" }
  std::string body = std::string("{\"mensaje\":\"") + esc(mensaje) + "\","
                     "\"placa\":\"" + esc(placa) + "\","
                     "\"modelo\":\"" + esc(modelo) + "\"}";

  RespBuf rb;
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.event_handler = _http_event_handler;
  cfg.user_data = &rb;
  cfg.timeout_ms = 8000;

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    ESP_LOGE(TAG, "esp_http_client_init failed");
    return false;
  }

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, body.c_str(), body.size());

  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP perform error: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (status != 200) {
    ESP_LOGW(TAG, "LLM respondió HTTP %d", status);
    return false;
  }

  responseJson.swap(rb.data);
  return true;
}

// ---- Handlers HTTP ----

esp_err_t AIService::pingHandler(httpd_req_t* req) {
  auto* inst = AIService::self();
  if (!inst) return ESP_FAIL;

  if (inst->llm_url_.empty()) {
    sendJSON(req, "{\"ok\":false,\"error\":\"LLM_URL no configurada\"}");
  } else {
    std::string j = std::string("{\"ok\":true,\"url\":\"") + esc(inst->llm_url_) + "\"}";
    sendJSON(req, j);
  }
  return ESP_OK;
}

esp_err_t AIService::generateHandler(httpd_req_t* req) {
  auto* inst = AIService::self(); if (!inst) return ESP_FAIL;

  // Query opcionales (con defaults)
  std::string placa = "ideaboard";
  std::string modelo = "gemini";
  (void) queryParam(req, "placa",  placa);
  (void) queryParam(req, "modelo", modelo);

  // Cuerpo = mensaje plano
  int len = req->content_len;
  if (len <= 0 || len > 8*1024) { // límite razonable
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body size");
    return ESP_OK;
  }
  std::string mensaje; mensaje.resize(len);
  int r = httpd_req_recv(req, mensaje.data(), len);
  if (r <= 0) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv error"); return ESP_OK; }

  // POST hacia el servidor LLM
  std::string llmResp;
  bool ok = inst->postToLLM(mensaje, placa, modelo, llmResp);
  if (!ok) {
    sendJSON(req, "{\"ok\":false,\"error\":\"LLM request failed or URL not set\"}");
    return ESP_OK;
  }

  // Transform opcional (si el usuario quiere remodelar)
  if (inst->transform_) {
    std::string t = inst->transform_(llmResp);
    sendJSON(req, t);
  } else {
    // Passthrough: devolvemos lo que respondió el servidor LLM (ya es JSON)
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, llmResp.c_str(), llmResp.size());
  }
  return ESP_OK;
}
