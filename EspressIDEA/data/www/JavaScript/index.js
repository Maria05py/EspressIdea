// /JavaScript/index.js
import { initTerminalWS } from './terminal.js';
import { initFS, FS, ensureIdle as ensureIdleFS } from './fs.js';
import { initEditor } from './editor.js'; // ← NUEVO

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
    await ensureIdleFS();
    await FS.listDir('/');
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
  initEditor(); // ← NUEVO: activa manejo de pestañas y textarea
  wireUI();
});
