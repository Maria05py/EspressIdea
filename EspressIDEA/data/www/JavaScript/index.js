// /JavaScript/index.js
// Orquesta el flujo: STOP => ensure_idle -> listar '/' -> conectar WS.

import { initTerminalWS } from './terminal.js';
import { initFS, FS, ensureIdle as ensureIdleFS } from './fs.js';

let term = null;
let busy = false;

function setBusy(on) {
  busy = !!on;
  document.body.style.cursor = on ? 'progress' : 'default';
}

async function onStopClick() {
  if (busy) return;
  setBusy(true);
  try {
    // 1) Parar/reiniciar y esperar prompt >>>
    await ensureIdleFS();
    // 2) Listar raíz
    await FS.listDir('/');
    // 3) Conectar WS
    if (!term) {
      term = initTerminalWS();
    } else if (!term.isConnected()) {
      term.connect();
    }
  } catch (e) {
    alert('Error al preparar REPL: ' + e.message);
  } finally {
    setBusy(false);
  }
}

function wireUI() {
  const btnStop = document.getElementById('btnStop');
  if (btnStop) btnStop.addEventListener('click', onStopClick);
}

document.addEventListener('DOMContentLoaded', () => {
  initFS();
  wireUI();
});
