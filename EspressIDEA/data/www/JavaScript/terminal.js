// terminal.js
// Módulo ES que expone initTerminalWS(options)
// options: { terminalSelector, statusSelector, wsPath }
//
// - terminalSelector: CSS selector del contenedor contenteditable (por defecto "#terminal")
// - statusSelector:   CSS selector del badge de estado (por defecto "#wsStatus")
// - wsPath:           ruta del endpoint WS (por defecto "/ws/serial")

export function initTerminalWS({
  terminalSelector = "#terminal",
  statusSelector   = "#wsStatus",
  wsPath           = "/ws/serial",
} = {}) {

  const termEl   = document.querySelector(terminalSelector);
  const statusEl = document.querySelector(statusSelector);
  if (!termEl)   { console.error("[terminal-ws] No se encontró terminalSelector:", terminalSelector); }
  if (!statusEl) { console.error("[terminal-ws] No se encontró statusSelector:", statusSelector); }

  const WS_URL = (location.protocol === "https:" ? "wss://" : "ws://") + location.host + wsPath;

  // ===== Utiles =====
  const enc = new TextEncoder();
  const dec = new TextDecoder();

  function setStatus(text, cls) {
    if (!statusEl) return;
    statusEl.textContent = text;
    statusEl.className = 'status badge ' + (cls || '');
  }
  function escHTML(s){
    return s.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;');
  }
  function focusEnd(){
    if (!termEl) return;
    termEl.focus();
    const sel = window.getSelection();
    const r = document.createRange();
    r.selectNodeContents(termEl);
    r.collapse(false);
    sel.removeAllRanges();
    sel.addRange(r);
  }

  // ===== Mini terminal buffer (muy simple, "estable") =====
  class MiniTerm {
    constructor(cols=200){
      this.cols = cols;
      this.lines = [''];
      this.r = 0; // row
      this.c = 0; // col
      this.esc = null; // acumulador de escape
      this.maxLines = 1000;
    }
    clamp(){
      if (this.r < 0) this.r = 0;
      if (this.r >= this.lines.length) this.r = this.lines.length-1;
      if (this.c < 0) this.c = 0;
      const line = this.lines[this.r];
      if (this.c > line.length) this.c = line.length;
    }
    ensureRow(r){
      while (this.lines.length <= r) this.lines.push('');
    }
    newline(){
      this.r++;
      this.ensureRow(this.r);
      this.c = 0;
      if (this.lines.length > this.maxLines){
        this.lines.splice(0, this.lines.length - this.maxLines);
        this.r = this.lines.length - 1;
      }
    }
    cr(){ this.c = 0; }
    lf(){ this.newline(); }

    bs(){ // backspace: borra char anterior si existe
      if (this.c > 0){
        const s = this.lines[this.r];
        this.lines[this.r] = s.slice(0, this.c-1) + s.slice(this.c);
        this.c--;
      } else if (this.r > 0) {
        // opcional: unir con línea previa
        this.r--;
        this.c = this.lines[this.r].length;
      }
    }

    insertChars(txt){
      const s = this.lines[this.r];
      this.lines[this.r] = s.slice(0, this.c) + txt + s.slice(this.c);
      this.c += txt.length;
    }

    // ===== Interpretar ESC [ ... secuencias básicas =====
    handleCSI(params, final){
      const ps = params.length ? params.split(';').map(x=>x===''?0:parseInt(x,10)) : [0];
      const n = (i, def=1)=> (ps.length>i && !isNaN(ps[i])) ? ps[i] : def;

      switch(final){
        case 'C': { // CUF: cursor forward
          const k = n(0,1);
          this.c += k; this.clamp(); break;
        }
        case 'D': { // CUB: cursor back
          const k = n(0,1);
          this.c -= k; this.clamp(); break;
        }
        case 'G': { // CHA: move to column
          this.c = Math.max(0, n(0,1)-1); this.clamp(); break;
        }
        case 'H': // CUP: row;col
        case 'f': {
          const row = Math.max(1, n(0,1)) - 1;
          const col = Math.max(1, n(1,1)) - 1;
          this.ensureRow(row);
          this.r = row;
          this.c = col;
          this.clamp();
          break;
        }
        case 'K': { // EL: erase in line
          const mode = n(0,0);
          const s = this.lines[this.r];
          if (mode === 0){ // to end
            this.lines[this.r] = s.slice(0, this.c);
          } else if (mode === 1){ // to start
            this.lines[this.r] = ' '.repeat(this.c) + s.slice(this.c);
          } else if (mode === 2){ // entire line
            this.lines[this.r] = '';
            this.c = 0;
          }
          break;
        }
        case 'J': {
          // Opcional: limpiar pantalla; implementamos 2 (entera)
          const mode = n(0,0);
          if (mode === 2){
            this.lines = [''];
            this.r = 0; this.c = 0;
          }
          break;
        }
        case 'm': {
          // SGR (colores/estilo). Ignoramos para esta prueba.
          break;
        }
        default:
          // Ignorado
          break;
      }
    }

    write(data){
      for (let i=0;i<data.length;i++){
        const ch = data[i];

        if (this.esc !== null){
          // acumulando secuencia ESC
          this.esc += ch;
          // Detectamos CSI: ESC[
          if (this.esc.length === 2 && this.esc !== '\x1b['){
            // otra secuencia no soportada (p.ej. ESC( B); reseteamos)
            this.esc = null;
            continue;
          }
          if (this.esc.startsWith('\x1b[')){
            // seguimos hasta encontrar finalizador [@A-Za-z]
            const m = this.esc.match(/^\x1b\[([0-9;]*)([@A-Za-z])$/);
            if (m){
              this.handleCSI(m[1], m[2]);
              this.esc = null;
            } else {
              // si es demasiado larga o aparece char no válido, cortamos
              if (this.esc.length > 32) this.esc = null;
            }
          }
          continue;
        }

        switch(ch){
          case '\x1b': this.esc = '\x1b'; break;     // ESC
          case '\b':   this.bs(); break;             // BS
          case '\r':   this.cr(); break;             // CR
          case '\n':   this.lf(); break;             // LF
          case '\x07': /*BEL*/ break;                // ignoramos ding
          case '\t':   this.insertChars('    '); break; // tab simple
          default:
            // imprimible (control < 0x20 ya cubiertos)
            this.insertChars(ch);
            break;
        }
      }
    }

    renderHTML(){
      // render rápido: escapar HTML y unir con <br>
      return this.lines.map(escHTML).join('<br>');
    }
  }

  const term = new MiniTerm();

  // ===== WebSocket + pegado/teclado =====
  let ws = null, manualClose=false, pending=false;

  function connect(){
    if ((ws && (ws.readyState===WebSocket.OPEN || ws.readyState===WebSocket.CONNECTING)) || pending) return;
    pending = true; manualClose = false;
    setStatus('Conectando…', 'is-connecting');
    ws = new WebSocket(WS_URL);
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
      pending = false;
      setStatus('Conectado', 'is-ok');
      writeAndRender(`\r\n[WS abierto ${new Date().toLocaleTimeString()}]\r\n`);
      focusEnd();
    };
    ws.onmessage = (ev) => {
      const text = (ev.data instanceof ArrayBuffer)
        ? dec.decode(new Uint8Array(ev.data))
        : String(ev.data);
      writeAndRender(text);
    };
    ws.onclose = () => {
      setStatus('Desconectado', 'is-off');
      writeAndRender(`\r\n[WS cerrado ${new Date().toLocaleTimeString()}]\r\n`);
      if (!manualClose) setTimeout(connect, 1000);
    };
    ws.onerror = () => {
      setStatus('Error', 'is-error');
      writeAndRender(`\r\n[WS error]\r\n`);
    };
  }
  function disconnect(){
    if (!ws) return;
    manualClose = true;
    try { ws.close(); } catch {}
  }
  function writeAndRender(text){
    term.write(text);
    if (termEl){
      termEl.innerHTML = term.renderHTML();
      // truco: caret visual al final
      termEl.insertAdjacentHTML('beforeend','<span class="caret">&#8203;</span>');
      termEl.scrollTop = termEl.scrollHeight;
      focusEnd();
    }
  }
  function sendBytes(u8){ if(ws && ws.readyState===WebSocket.OPEN) ws.send(u8); }
  function sendText(s){ sendBytes(enc.encode(s)); }

  // Entrada desde teclado (terminal “real”: no escribimos local)
  function onKeydown(e) {
    // Permitir copiar si hay selección y Ctrl/Cmd+C
    if ((e.ctrlKey||e.metaKey) && e.key.toLowerCase()==='c' && !e.shiftKey){
      const sel = window.getSelection();
      if (sel && !sel.isCollapsed) return; // deja copiar
    }
    e.preventDefault();

    // Atajos Ctrl+Shift
    if (e.ctrlKey && e.shiftKey && e.code==='KeyC'){ sendText('\x03'); return; } // ^C
    if (e.ctrlKey && e.shiftKey && e.code==='KeyD'){ sendText('\x04'); return; } // ^D

    // Flechas → secuencias ANSI
    const ESC = '\x1b';
    if (e.key === 'ArrowLeft'){ sendText(ESC+'[D'); return; }
    if (e.key === 'ArrowRight'){ sendText(ESC+'[C'); return; }
    if (e.key === 'ArrowUp'){ sendText(ESC+'[A'); return; }
    if (e.key === 'ArrowDown'){ sendText(ESC+'[B'); return; }

    if (e.key === 'Backspace'){ sendText('\x08'); return; }   // BS
    if (e.key === 'Enter'){ sendText('\r\n'); return; }       // CRLF
    if (e.key === 'Tab'){ sendText('\t'); return; }           // TAB

    // Ctrl-C / Ctrl-D directos
    if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase()==='c'){ sendText('\x03'); return; }
    if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase()==='d'){ sendText('\x04'); return; }

    // Imprimibles
    if (e.key && e.key.length===1 && !e.metaKey){
      sendText(e.key);
    }
  }
  function onBeforeInput(e){ e.preventDefault(); }
  function onPaste(e){
    e.preventDefault();
    const t = (e.clipboardData||window.clipboardData).getData('text') || '';
    if (t) sendText(t.replace(/\r?\n/g, '\r\n'));
  }
  function onDrop(e){ e.preventDefault(); }

  // Listeners (y cleanup)
  if (termEl){
    termEl.addEventListener('keydown', onKeydown);
    termEl.addEventListener('beforeinput', onBeforeInput);
    termEl.addEventListener('paste', onPaste);
    termEl.addEventListener('drop', onDrop);
  }

  connect(); // autoconectar al iniciar

  // API pública por si quieres usarla luego
  const api = {
    connect,
    disconnect,
    sendText,
    isConnected: () => ws && ws.readyState === WebSocket.OPEN,
    destroy: () => {
      disconnect();
      if (!termEl) return;
      termEl.removeEventListener('keydown', onKeydown);
      termEl.removeEventListener('beforeinput', onBeforeInput);
      termEl.removeEventListener('paste', onPaste);
      termEl.removeEventListener('drop', onDrop);
    }
  };

  // Opcionalmente expón para debug:
  // window.__TerminalWS__ = api;

  return api;
}
