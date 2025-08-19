// /JavaScript/index.js
// Orquesta el flujo: STOP => ensure_idle -> listar '/' -> conectar WS.
// La terminal (terminal.js) no se toca.

import { initTerminalWS } from './terminal.js';

let term = null;
let busy = false;

function setBusy(on) {
  busy = !!on;
  document.body.style.cursor = on ? 'progress' : 'default';
}

async function ensureIdle() {
  const res = await fetch('/api/repl/ensure_idle', { method: 'POST' });
  const j = await res.json().catch(() => ({}));
  if (!res.ok || (j.ok === false)) {
    throw new Error(j.error || `HTTP ${res.status}`);
  }
  return j;
}

async function onStopClick() {
  if (busy) return;
  setBusy(true);
  try {
    // 1) Parar/reiniciar y esperar prompt >>>
    await ensureIdle();
    // 2) Listar raíz (el FS inline NO auto-lista al inicio)
    if (window.EspressIdeaFS) {
      await window.EspressIdeaFS.listDir('/');
    }
    // 3) Conectar WS (si no está conectado ya)
    if (!term) {
      term = initTerminalWS();        // se conectará ahora
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

  // Opcional: si quieres forzar ensure_idle antes de cualquier guardado/subida
  // ya lo hace el FS inline; no necesitamos interceptar aquí.
}

document.addEventListener('DOMContentLoaded', wireUI);
