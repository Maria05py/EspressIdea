#pragma once
#include <string>
#include <functional>
#include "esp_http_server.h"

class ServerManager; // fwd

namespace EspressIDEA {

/**
 * AIService: puente mínimo para conversar con un servidor LLM externo.
 * - No ejecuta código en la placa.
 * - Solo envía/recibe mensajes.
 * - Lee la URL del LLM desde /spiffs/CREDENTIALS.txt (claves: LLM_URL= o AI_URL=).
 *
 * Endpoints:
 *  - POST /api/ai/generate?placa=ideaboard&modelo=gemini
 *      Body (text/plain): el mensaje del usuario.
 *      Respuesta: JSON tal cual lo devolvió el servidor LLM (o {"ok":false,"error":...}).
 *
 *  - GET  /api/ai/ping
 *      Respuesta: {"ok":true,"url":"..."} si hay URL cargada; si no, ok=false.
 */
class AIService {
public:
  explicit AIService(ServerManager& server)
  : server_(server) {}

  // Intenta cargar/recargar la URL del LLM desde el credentialsPath (ej: "/spiffs/CREDENTIALS.txt").
  // Devuelve true si encontró una línea LLM_URL=... o AI_URL=...
  bool loadLLMUrlFromCredentials(const char* credentialsPath);

  // Permite setear/overridear en caliente (útil para tests o UI avanzada).
  void setLLMUrl(const std::string& url) { llm_url_ = url; }

  // URL actual (vacía si no configurada correctamente).
  const std::string& llmUrl() const { return llm_url_; }

  // Registro explícito de rutas HTTP (no se auto-registra).
  void registerRoutes();

  // (Opcional) Hook para transformar la respuesta si quisieras post-procesar
  // (por defecto es passtrough). Si no se establece, no se usa.
  using ResponseTransform = std::function<std::string(const std::string&)>;
  void setResponseTransform(ResponseTransform fn) { transform_ = std::move(fn); }

  // Handlers estáticos para httpd
  static esp_err_t pingHandler(httpd_req_t* req);
  static esp_err_t generateHandler(httpd_req_t* req);

private:
  // Helpers
  static void sendJSON(httpd_req_t* req, const std::string& json);
  static std::string esc(const std::string& s);
  static std::string urlDecode(const std::string& s);
  static bool queryParam(httpd_req_t* req, const char* key, std::string& out);

  // POST al servidor LLM: /generar con JSON {"mensaje","placa","modelo"}
  // Devuelve true y escribe responseJson si HTTP 200 (lee cuerpo completo).
  bool postToLLM(const std::string& mensaje,
                 const std::string& placa,
                 const std::string& modelo,
                 std::string& responseJson);

  // Acceso a instancia (para handlers estáticos)
  static AIService* self();

  ServerManager&   server_;
  std::string      llm_url_;
  ResponseTransform transform_{};

  // Singleton minimalista (igual patrón que otros servicios tuyos)
  static AIService* s_self_;
};

} // namespace EspressIDEA
