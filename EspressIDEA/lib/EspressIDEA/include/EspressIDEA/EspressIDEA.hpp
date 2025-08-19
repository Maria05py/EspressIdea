#pragma once
#include "EspressIDEA/ReplControl.hpp"
#include "EspressIDEA/TerminalWS.hpp"
#include "EspressIDEA/FSService.hpp"
#include "EspressIDEA/ExecService.hpp"
#include "PyBoardUART.hpp"   // <-- Necesario para usar waitForReplPrompt en el waiter

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
    exec(board_, repl, server)
  {
    // Inicializa ReplControl con dependencias
    repl.init(&board_, &server);

    // Forzamos modo CircuitPython (opcional; por defecto ya viene true)
    repl.setCircuitPython(true);

    // Conectamos un "esperador de prompt" real: usa UART para detectar ">>>"
    // Esto permite que ensureIdleCircuitPython() no solo espere, sino que confirme el prompt.
    repl.setPromptWaiter([this](uint32_t timeout_ms) {
      return this->board_.waitForReplPrompt(timeout_ms) == PyBoard::ErrorCode::OK;
    });
  }

  void begin() {
    terminal.registerRoutes();
    fs.registerRoutes();
    exec.registerRoutes();
  }

  ReplControl& replControl() { return repl; }
  TerminalWS&  terminalWS()  { return terminal; }
  FSService&   fsService()   { return fs; }
  ExecService& execService() { return exec; }

private:
  PyBoard::PyBoardUART& board_;
  ReplControl repl;
  TerminalWS  terminal;
  FSService   fs;
  ExecService exec;
};

} // namespace EspressIDEA
