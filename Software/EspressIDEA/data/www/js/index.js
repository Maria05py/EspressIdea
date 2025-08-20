// /JavaScript/index.js
import { initTerminalWS } from './terminal.js';
import { initFS, FS, ensureIdle as ensureIdleFS } from './fs.js';
import { initEditor } from './editor.js';
import { initAIChat } from './ai-chat.js';

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
    // 1) Dejar REPL inactivo
    await ensureIdleFS();
    // 2) Refrescar FS en raíz
    await FS.listDir('/');
    // 3) Abrir / reconectar terminal WS
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
  const btnStop     = document.getElementById('btnStop');
  const btnNewFile  = document.getElementById('btnNewFile');   // ← NUEVO

  if (btnStop)    btnStop.addEventListener('click', onStopClick);
  if (btnNewFile) btnNewFile.addEventListener('click', () => {
    try {
      // Crea archivo vacío en el cwd actual (prompt de nombre dentro de FS.newFileInCwd)
      if (FS && typeof FS.newFileInCwd === 'function') {
        FS.newFileInCwd();
      } else {
        console.log("IDK IT WORKS THIS WAY")
      }
    } catch (e) {
      alert('No se pudo crear el archivo: ' + (e.message || e));
    }
  });
}

// Arranque base
document.addEventListener('DOMContentLoaded', () => {
  initFS();
  initEditor();     // Editor controla guardar/ejecutar con el flujo nuevo
  wireUI();
  initAIChat();

  // Opcional: autostart si viene en la query (?autostart=1)
  const params = new URLSearchParams(location.search);
  if (params.get('autostart') === '1') {
    onStopClick();
  }
});
