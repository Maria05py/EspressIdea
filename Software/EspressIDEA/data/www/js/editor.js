// /JavaScript/editor.js
// Manejador de múltiples archivos abiertos (pestañas) y del textarea.
// + Ejecución del contenido activo vía /api/exec.
// Requisitos en fs.js: setExternalEditor(true), on('file:opened'), writeFile(), apiExists(), apiCreate().

import {
  FS,
  on as onFS,
  setExternalEditor,
  writeFile as fsWriteFile,
  apiExists,
  apiCreate
} from './fs.js';

const $  = (sel, root) => (root || document).querySelector(sel);

// ---------------------------- Estado ----------------------------
const state = {
  docs: new Map(), // path -> { name, text, dirty }
  activePath: null,
};

const els = {
  tabs: null,
  editor: null,
  btnSave: null,
  btnDownload: null,
  btnRun: null,
  btnStop: null,
  firstTab: null,   // #activeTab
  firstLabel: null, // span.label dentro de #activeTab
  wsStatus: null,
  terminal: null,
};

// ---------------------------- Utils ----------------------------
function baseName(p){ const s = String(p||''); const i = s.lastIndexOf('/'); return (i>=0)? s.slice(i+1): s; }
function joinPath(dir, name){
  if (!dir || dir === "/") return "/" + String(name).replace(/^\/+/, "");
  return dir.replace(/\/+$/, "") + "/" + String(name).replace(/^\/+/, "");
}

function ensureFirstTabCloseBtn(enabled, pathForClose){
  const old = $('#activeTab .close-tab');
  if (old) old.remove();
  if (!enabled) return;

  const close = document.createElement('span');
  close.className = 'close-tab';
  close.title = 'Cerrar';
  close.textContent = '×';
  close.addEventListener('click', (e) => {
    e.stopPropagation();
    closeTab(pathForClose);
  });
  els.firstTab.appendChild(close);
}

function renderTabs(){
  if (!els.tabs || !els.firstTab || !els.firstLabel) return;

  // Elimina pestañas dinámicas (todas menos la primera)
  Array.from(els.tabs.children).slice(1).forEach(n => n.remove());

  const paths = Array.from(state.docs.keys());

  if (paths.length === 0) {
    els.firstTab.classList.add('active');
    els.firstLabel.textContent = 'sin nombre';
    ensureFirstTabCloseBtn(false);
    return;
  }

  paths.forEach((path, idx) => {
    const doc = state.docs.get(path);
    const label = baseName(path) + (doc.dirty ? ' *' : '');

    if (idx === 0) {
      els.firstTab.classList.toggle('active', path === state.activePath);
      els.firstLabel.textContent = label;
      els.firstTab.onclick = () => switchTo(path);
      ensureFirstTabCloseBtn(true, path);
      return;
    }

    const tab = document.createElement('div');
    tab.className = 'tab' + (path === state.activePath ? ' active' : '');
    tab.innerHTML = `<span class="label">${label}</span> <span class="close-tab" title="Cerrar">×</span>`;
    tab.addEventListener('click', (e) => {
      if (e.target && e.target.classList.contains('close-tab')) {
        e.stopPropagation();
        closeTab(path);
      } else {
        switchTo(path);
      }
    });
    els.tabs.appendChild(tab);
  });
}

function markDirty(on){
  const p = state.activePath;
  if (!p) return;
  const doc = state.docs.get(p);
  if (!doc) return;
  doc.dirty = !!on;
  renderTabs();
}

function switchTo(path){
  if (!path || !state.docs.has(path)) return;
  state.activePath = path;
  const doc = state.docs.get(path);
  els.editor.value = doc.text;
  renderTabs();
}

function closeTab(path){
  if (!state.docs.has(path)) return;

  const wasActive = (state.activePath === path);
  const doc = state.docs.get(path);

  if (doc.dirty) {
    const r = confirm(`El archivo "${baseName(path)}" tiene cambios sin guardar. ¿Cerrar de todas formas?`);
    if (!r) return;
  }

  state.docs.delete(path);

  if (state.docs.size === 0) {
    state.activePath = null;
    els.editor.value = '';
    renderTabs();
    return;
  }

  if (wasActive) {
    const next = Array.from(state.docs.keys()).pop();
    state.activePath = next;
    els.editor.value = state.docs.get(next).text;
  }
  renderTabs();
}

function openOrFocus(path, text){
  if (!state.docs.has(path)) {
    state.docs.set(path, { name: baseName(path), text: text||'', dirty:false });
  } else {
    const doc = state.docs.get(path);
    doc.text = text || '';
    doc.dirty = false;
  }
  state.activePath = path;
  els.editor.value = text || '';
  renderTabs();
}

function parseJSON(res){
  return res.text().then(txt=>{
    try { return JSON.parse(txt); }
    catch { throw new Error('Respuesta no JSON: ' + txt.slice(0,120)); }
  });
}

function setWsStatus(mode /* 'idle' | 'running' | 'ok' | 'error' */){
  if (!els.wsStatus) return;
  els.wsStatus.classList.remove('success','danger','warn');
  if (mode === 'running'){ els.wsStatus.classList.add('warn');   els.wsStatus.textContent = 'Ejecutando...'; }
  else if (mode === 'ok'){ els.wsStatus.classList.add('success'); els.wsStatus.textContent = 'Ejecutado'; }
  else if (mode === 'idle'){ els.wsStatus.classList.add('success'); els.wsStatus.textContent = 'REPL listo'; }
  else if (mode === 'error'){ els.wsStatus.classList.add('danger'); els.wsStatus.textContent = 'Error'; }
}

// ---------------------------- Guardado (create o write) ----------------------------
async function savePathEnsuringCreate(path, text){
  const exists = await apiExists(path);
  if (!exists) await apiCreate(path, text);
  else         await fsWriteFile(path, text);
}

// ---------------------------- Acciones de archivo ----------------------------
async function onSave(){
  const p = state.activePath;
  if (!p) { alert("No hay archivo activo."); return; }

  try {
    await savePathEnsuringCreate(p, els.editor.value);
    const doc = state.docs.get(p);
    if (doc) { doc.text = els.editor.value; doc.dirty = false; }
    renderTabs();
    alert("Archivo guardado ✅");
  } catch (e) {
    alert(e.message || "Error al guardar");
  }
}

function onDownload(){
  const p = state.activePath;
  if (!p) { alert("No hay archivo activo."); return; }
  const url = "/api/fs/download?path=" + encodeURIComponent(p);
  window.open(url, "_blank");
}

// ---------------------------- Ejecución / REPL ----------------------------
// Flujo solicitado: GUARDAR → ENSURE_IDLE → EXEC
async function execActiveEditor(){
  const code = els.editor ? els.editor.value : '';
  if (!code.trim()){
    alert('El editor está vacío.');
    return;
  }

  // Asegurar que haya un archivo asociado; si no, pedir nombre y crear en cwd
  let path = state.activePath;
  if (!path){
    const cwd = (FS && FS.state && FS.state.cwd) ? FS.state.cwd : '/';
    const fname = prompt('Nombre del archivo a ejecutar:', 'code.py');
    if (!fname) return;
    path = joinPath(cwd, fname);
    // abre/fija pestaña
    openOrFocus(path, code);
  }

  if (els.btnRun) els.btnRun.disabled = true;
  setWsStatus('running');

  try{
    // 1) Guardar (crea si no existe)
    await savePathEnsuringCreate(path, els.editor.value);
    // sin marcar dirty
    const doc = state.docs.get(path);
    if (doc){ doc.text = els.editor.value; doc.dirty = false; renderTabs(); }

    // 2) Ensure idle
    await FS.ensureIdle();
    setWsStatus('idle'); // feedback intermedio

    // 3) Ejecutar
    const resp = await fetch('/api/exec', {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: code
    });
    const j = await parseJSON(resp);
    if (!j.ok) {
      console.log("fallo silencioso");
      return;
    }

    if (els.terminal){
      const hdr = document.createElement('div');
      hdr.style.opacity = '0.8';
      hdr.style.margin = '6px 0';
      hdr.textContent = '⮕ Salida de ejecución';
      const pre = document.createElement('pre');
      pre.textContent = j.stdout || '';
      els.terminal.appendChild(hdr);
      els.terminal.appendChild(pre);
      els.terminal.scrollTop = els.terminal.scrollHeight;
    } else {
      alert(j.stdout || 'Ejecutado sin salida.');
    }

    setWsStatus('ok');
  } catch(e){
    console.error(e);
    setWsStatus('error');
    alert('Error al ejecutar: ' + e.message);
  } finally {
    if (els.btnRun) els.btnRun.disabled = false;
  }
}

async function ensureIdleRepl(){
  try{
    const resp = await fetch('/api/repl/ensure_idle', { method:'POST' });
    const j = await parseJSON(resp);
    if (!j.ok) throw new Error('No se pudo asegurar REPL inactivo');
    setWsStatus('idle');
  } catch(e){
    console.error(e);
    setWsStatus('error');
    alert('Error preparando REPL: ' + e.message);
  }
}

// ---------------------------- Wiring del editor ----------------------------
function wireEditor(){
  els.editor.addEventListener('input', () => {
    const p = state.activePath;
    if (!p) return;
    const doc = state.docs.get(p);
    if (!doc) return;
    doc.text = els.editor.value;
    markDirty(true);
  });

  // Ctrl+S / Cmd+S
  window.addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
      e.preventDefault();
      onSave();
    }
  });

  // ▶️ y ⏹
  if (els.btnRun)  els.btnRun.addEventListener('click', execActiveEditor);
  if (els.btnStop) els.btnStop.addEventListener('click', ensureIdleRepl);
}

// ---------------------------- API pública ----------------------------
export function initEditor(){
  els.tabs        = document.getElementById('tabsBar') || document.querySelector('.tabs');
  els.firstTab    = document.getElementById('activeTab');
  els.firstLabel  = $('#activeTab .label') || (()=>{ // retrocompatibilidad
    const span = document.createElement('span');
    span.className = 'label';
    span.textContent = 'sin nombre';
    els.firstTab.textContent = '';
    els.firstTab.appendChild(span);
    return span;
  })();
  els.editor      = document.getElementById('codeEditor');
  els.btnSave     = document.getElementById('btnSave');
  els.btnDownload = document.getElementById('btnDownload');
  els.btnRun      = document.querySelector('.btn.play');
  els.btnStop     = document.getElementById('btnStop');
  els.wsStatus    = document.getElementById('wsStatus');
  els.terminal    = document.getElementById('terminal');

  if (!els.tabs || !els.editor || !els.firstTab || !els.firstLabel) {
    console.warn("[editor] Elementos base no presentes; omitiendo init.");
    return;
  }

  // Decimos a FS que NO toque el textarea ni la pestaña
  setExternalEditor(true);

  // Eventos FS
  onFS('file:opened', ({path, text}) => openOrFocus(path, text));

  // Botones
  if (els.btnSave)     els.btnSave.addEventListener('click', onSave);
  if (els.btnDownload) els.btnDownload.addEventListener('click', onDownload);

  wireEditor();
  renderTabs();
}

// util opcional
export function focusPath(path){
  if (state.docs.has(path)) switchTo(path);
}

// Para acceso manual desde consola si se requiere
export const runActive = execActiveEditor;
export const replEnsureIdle = ensureIdleRepl;
