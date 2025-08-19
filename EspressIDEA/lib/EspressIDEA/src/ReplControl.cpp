#include "EspressIDEA/ReplControl.hpp"
#include "PyBoardUART.hpp"
#include "ServerManager.hpp"
#include "esp_log.h"

using namespace EspressIDEA;
static const char* TAG = "ReplControl";

ReplControl::ReplControl() {
  mutex_ = xSemaphoreCreateMutex();
}

ReplControl::~ReplControl() {
  if (mutex_) vSemaphoreDelete(mutex_);
}

void ReplControl::init(PyBoard::PyBoardUART* board, ServerManager* server) {
  board_ = board;
  server_ = server;
  mode_ = ReplMode::TERMINAL;
  heldByTerminal_ = false;
  // Por defecto asumimos CircuitPython; puedes desactivarlo con setCircuitPython(false)
  // si detectas MicroPython en tu banner inicial.
}

void ReplControl::setModeTerminal() {
  // Cambio explícito a TERMINAL (usado al abrir WS, etc.)
  mode_ = ReplMode::TERMINAL;
}

// ---------------- Arbitraje con TerminalWS -----------------

bool ReplControl::tryLockFromTerminal() {
  // TerminalWS llama esto antes de leer del UART; si el REPL está CONTROLADO,
  // no debe leer (devuelve false). Si está libre, marca que lo tiene.
  if (mode_ != ReplMode::TERMINAL) return false;
  if (heldByTerminal_) return true; // ya "lo tenía" en este hilo de lectura
  // No usamos un mutex "largo" aquí para no bloquear la ISR/lector;
  // bastan flags coherentes con el modo.
  heldByTerminal_ = true;
  return true;
}

void ReplControl::unlockFromTerminal() {
  heldByTerminal_ = false;
}

void ReplControl::forceControlled() {
  // Adquiere mutex y cambia a CONTROLADO (los lectores TerminalWS verán el flag)
  if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
  mode_ = ReplMode::CONTROLADO;
}

void ReplControl::releaseControlled() {
  mode_ = ReplMode::TERMINAL;
  if (mutex_) xSemaphoreGive(mutex_);
}

// ---------------- Sincronización de REPL -----------------

bool ReplControl::ensureIdle(uint32_t timeout_ms) {
  if (circuitpython_) {
    return ensureIdleCircuitPython(timeout_ms);
  }
  // Si en el futuro usas MicroPython (raw REPL), aquí podrías:
  //  - enviar ^C, entrar raw, etc.
  // Por ahora: comportamiento similar a CP sin raw.
  return ensureIdleCircuitPython(timeout_ms);
}

bool ReplControl::ensureIdleCircuitPython(uint32_t timeout_ms) {
  if (!board_) return false;

  // Entramos en CONTROLADO para no interferir con TerminalWS.
  forceControlled();

  // Secuencia "tipo Thonny":
  // 1) ^C para interrumpir code.py si está corriendo
  board_->interrupt();
  vTaskDelay(pdMS_TO_TICKS(200));

  // 2) ^D para soft reset (CircuitPython reinicia y mostrará banner)
  //    Nota: aquí NO queremos raw REPL. Esto solo reinicia CP.
  board_->softReset();
  vTaskDelay(pdMS_TO_TICKS(200));

  // 3) Si tenemos callback de espera del prompt, lo usamos:
  bool ok = true;
  if (waitPromptFn_) {
    ok = waitPromptFn_(timeout_ms);
  } else {
    // Fallback: esperamos un poco y mandamos ENTER para "activar" el prompt
    // (No podemos leer UART desde aquí porque PyBoardUART no expone read en público)
    const TickType_t total = pdMS_TO_TICKS(timeout_ms);
    const TickType_t step  = pdMS_TO_TICKS(50);
    TickType_t acc = 0;
    while (acc < total) { vTaskDelay(step); acc += step; }
    // "Tocar" el REPL para que pinte el prompt
    const char crlf[] = "\r\n";
    (void) board_->write(crlf, sizeof(crlf)-1);
  }

  // 4) Cortesía: otro ^C suave por si el usuario tenía auto-reload o mensajes residuales
  vTaskDelay(pdMS_TO_TICKS(120));
  board_->interrupt();

  // Salimos a TERMINAL
  releaseControlled();

  ESP_LOGI(TAG, "ensureIdleCircuitPython: %s", ok ? "OK" : "timeout (fallback usado)");
  return ok;
}

// ---------------- RAII: ScopedReplLock -----------------

ReplControl::ScopedReplLock::ScopedReplLock(ReplControl& rc, const char* tag)
: rc_(rc), tag_(tag) {
  rc_.forceControlled();
  if (tag_) ESP_LOGI("ReplLock", "CONTROLADO on (%s)", tag_);
  locked_ = true;
}

ReplControl::ScopedReplLock::~ScopedReplLock() {
  if (locked_) {
    rc_.releaseControlled();
    if (tag_) ESP_LOGI("ReplLock", "CONTROLADO off (%s)", tag_);
  }
}
