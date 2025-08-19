#pragma once
#include <functional>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace PyBoard { class PyBoardUART; }
class ServerManager;

namespace EspressIDEA {

// Modo del REPL visto por los servicios
enum class ReplMode {
  TERMINAL,    // flujo libre hacia/desde el WS (TerminalWS)
  CONTROLADO   // en uso por FS/Exec; TerminalWS debe abstenerse
};

class ReplControl {
public:
  ReplControl();
  ~ReplControl();

  // Debe llamarse una vez tras construir Device
  void init(PyBoard::PyBoardUART* board, ServerManager* server);

  // ---- Config / detección de entorno ----
  // Marca si el dispositivo objetivo es CircuitPython.
  // Si es true, se evita raw REPL y se usa la secuencia ^C,^D,(opcional enter) + waitPrompt.
  void setCircuitPython(bool on) { circuitpython_ = on; }
  bool isCircuitPython() const { return circuitpython_; }

  // Permite inyectar un "esperador de prompt" externo (opcional).
  // Debe devolver true si encontró ">>>" antes de timeout.
  // Ejemplo de uso: desde TerminalWS (que ya lee UART) o un helper que pueda leer.
  void setPromptWaiter(std::function<bool(uint32_t timeout_ms)> waiter) {
    waitPromptFn_ = std::move(waiter);
  }

  // ---- Modo / arbitraje ----
  void setModeTerminal();
  ReplMode mode() const { return mode_; }

  // Llamado por TerminalWS antes de leer del UART; si consigue el lock rápido, puede continuar.
  bool tryLockFromTerminal();
  void unlockFromTerminal();

  // ---- Utilidades de sincronización REPL ----
  // Asegura que el REPL esté "listo para órdenes":
  // - En CircuitPython: ^C,^D y, si es posible, esperar hasta ver ">>>"
  // - En otros (MicroPython): puedes continuar usando tu flujo habitual (p.ej., raw REPL)
  // Devuelve true si se consideró "listo" (aunque sea por fallback sin esperar prompt real).
  bool ensureIdle(uint32_t timeout_ms = 3000);

  // Versión explícita de CircuitPython (por si quieres llamarla directamente)
  bool ensureIdleCircuitPython(uint32_t timeout_ms = 3000);

  // ---- RAII: Bloqueo CONTROLADO (FS/Exec) ----
  class ScopedReplLock {
  public:
    ScopedReplLock(ReplControl& rc, const char* tag = nullptr);
    ~ScopedReplLock();
    ScopedReplLock(const ScopedReplLock&) = delete;
    ScopedReplLock& operator=(const ScopedReplLock&) = delete;
  private:
    ReplControl& rc_;
    bool locked_ = false;
    const char* tag_;
  };

private:
  // Internos
  void forceControlled();
  void releaseControlled();

  // Dependencias
  PyBoard::PyBoardUART* board_ = nullptr;
  ServerManager* server_ = nullptr;

  // Estado
  ReplMode mode_ = ReplMode::TERMINAL;
  SemaphoreHandle_t mutex_ = nullptr;       // Serializa CONTROLADO vs TERMINAL
  volatile bool heldByTerminal_ = false;    // Fast-path para lector UART

  // Config
  bool circuitpython_ = true;               // por defecto true (tu target actual)
  std::function<bool(uint32_t)> waitPromptFn_; // espera ">>>", opcional
};

} // namespace EspressIDEA
