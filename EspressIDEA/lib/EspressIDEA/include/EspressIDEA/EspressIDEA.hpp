#pragma once
#include "EspressIDEA/ReplControl.hpp"
#include "EspressIDEA/TerminalWS.hpp"
#include "EspressIDEA/FSService.hpp"
#include "EspressIDEA/ExecService.hpp"
#include "EspressIDEA/AIService.hpp"     // <-- NUEVO
#include "PyBoardUART.hpp"               // Necesario para waitForReplPrompt en el waiter

namespace PyBoard { class PyBoardUART; }
class ServerManager;

namespace EspressIDEA {

class Device {
public:
  Device(PyBoard::PyBoardUART& board, ServerManager& server)
  : board_(board),
    repl(),
    terminal(board_, repl, server),
    fs(board_, repl, server),
    exec(board_, repl, server),
    ai(server) // <-- NUEVO
  {
    // Inicializa ReplControl con dependencias
    repl.init(&board_, &server);

    // Forzamos modo CircuitPython (opcional; por defecto ya viene true)
    repl.setCircuitPython(true);

    // Esperador de prompt real: usa UART para detectar ">>>"
    repl.setPromptWaiter([this](uint32_t timeout_ms) {
      return this->board_.waitForReplPrompt(timeout_ms) == PyBoard::ErrorCode::OK;
    });
  }

  void begin() {
    terminal.registerRoutes();
    fs.registerRoutes();
    exec.registerRoutes();

    // AIService: rutas HTTP para puente LLM y carga de URL desde SPIFFS (no ejecuta nada por sí solo).
    ai.loadLLMUrlFromCredentials("/spiffs/CREDENTIALS.txt"); // el módulo lee y parsea LLM_URL/AI_URL
    ai.registerRoutes();
  }

  ReplControl& replControl() { return repl; }
  TerminalWS&  terminalWS()  { return terminal; }
  FSService&   fsService()   { return fs; }
  ExecService& execService() { return exec; }
  AIService&   aiService()   { return ai; } // <-- NUEVO

private:
  PyBoard::PyBoardUART& board_;
  ReplControl repl;
  TerminalWS  terminal;
  FSService   fs;
  ExecService exec;
  AIService   ai; // <-- NUEVO
};

} // namespace EspressIDEA
