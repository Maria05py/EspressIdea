// /JavaScript/ai-chat.js
// Chat minimalista para AIService del ESP32.
// Endpoints usados:
//   GET  /api/ai/ping
//   POST /api/ai/generate?placa=<...>&modelo=<...>   (body = texto plano)
//
// Uso rápido:
//   import { initAIChat } from './ai-chat.js';
//   document.addEventListener('DOMContentLoaded', () => { initAIChat(); });
//
// Opciones:
//   containerEl:   HTMLElement contenedor de mensajes (default: #chatMessages)
//   inputEl:       <input> del prompt (default: #chatInput)
//   sendBtnEl:     <button> enviar (default: #sendBtn)
//   aiBadgeEl:     chip/etiqueta de estado IA (default: #tagAI si existe)
//   placa:         'ideaboard' | ... (default: 'ideaboard')
//   modelo:        'gemini' | 'ollama' (default: 'gemini')
//   onCode:        (code:string) => void  // para insertar código donde quieras
//
// Notas:
// - Si no pasas onCode, intentará insertar en #codeEditor y hará focus.

function $(sel, root = document) { return root.querySelector(sel); }

function escapeHTML(s) {
  return String(s)
    .replaceAll('&', '&amp;').replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;').replaceAll('"', '&quot;');
}

function makeBubble(role, html, extra = {}) {
  const div = document.createElement('div');
  div.className = role === 'user' ? 'chat-bubble-user' : 'chat-bubble';
  if (extra.small) div.classList.add('small');
  div.innerHTML = html;
  return div;
}

async function parseJSON(res) {
  const txt = await res.text();
  try { return JSON.parse(txt); }
  catch {
    throw new Error('Respuesta no JSON: ' + txt.slice(0, 160));
  }
}

function splitRespuesta(seccionesMaybe) {
  // El AIService hace passthrough del JSON del servidor Flask:
  // { explicacion: string, codigo: string }  (o similar)
  // También toleramos respuestas sin tags.
  let explicacion = '', codigo = '';
  if (!seccionesMaybe) return { explicacion, codigo };

  if (typeof seccionesMaybe === 'object') {
    explicacion = seccionesMaybe.explicacion || seccionesMaybe.explicación || '';
    codigo = seccionesMaybe.codigo || seccionesMaybe.código || '';
  } else if (typeof seccionesMaybe === 'string') {
    // fallback simple
    explicacion = seccionesMaybe;
  }
  return { explicacion, codigo };
}

export function initAIChat(opts = {}) {
  const container = opts.containerEl || $('#chatMessages');
  const input = opts.inputEl || $('#chatInput');
  const sendBtn = opts.sendBtnEl || $('#sendBtn');
  const aiBadge = opts.aiBadgeEl || $('#tagAI');  // opcional
  const placa = (opts.placa || 'ideaboard').toLowerCase();
  const modelo = (opts.modelo || 'gemini').toLowerCase();
  const onCode = typeof opts.onCode === 'function'
    ? opts.onCode
    : (code) => {
      const ta = $('#codeEditor');
      if (!ta) { alert('No encontré #codeEditor para insertar el código'); return; }
      ta.value = code;
      // Dispara evento input para que otros módulos detecten cambio
      ta.dispatchEvent(new Event('input', { bubbles: true }));
      ta.focus();
    };

  if (!container || !input || !sendBtn) {
    console.warn('[ai-chat] faltan elementos base (#chatMessages, #chatInput, #sendBtn)');
    return;
  }

  // ---- Estado visual del chip IA (si existe) ----
  function setAIBadge(ok, textOK = 'IA OK', textFail = 'IA OFF') {
    if (!aiBadge) return;
    aiBadge.classList.remove('red', 'green');
    aiBadge.classList.add(ok ? 'green' : 'red');
    aiBadge.textContent = ok ? textOK : textFail;
  }

  // ---- Ping inicial a /api/ai/ping ----
  (async () => {
    try {
      const r = await fetch('/api/ai/ping');
      const j = await parseJSON(r);
      if (j.ok) {
        setAIBadge(true, 'IA');
        container.appendChild(makeBubble(
          'assistant',
          `<strong>Conectado a IA</strong><br><small>URL: ${escapeHTML(j.url || '')}</small>`,
          { small: true }
        ));
      } else {
        setAIBadge(false);
        container.appendChild(makeBubble(
          'assistant',
          `<strong>IA no configurada</strong><br><small>Configura LLM_URL/AI_URL en CREDENTIALS.txt</small>`,
          { small: true }
        ));
      }
    } catch (e) {
      setAIBadge(false);
      container.appendChild(makeBubble(
        'assistant',
        `<strong>IA no disponible</strong><br><small>${escapeHTML(e.message)}</small>`,
        { small: true }
      ));
    }
    container.scrollTop = container.scrollHeight;
  })();

  // ---- Envío de mensaje ----
  let sending = false;

  // --- NUEVO: helper para pelar triple backticks ---
  function peelFencedCode(txt) {
    if (typeof txt !== 'string') return { code: '', lang: '', raw: '', fenced: false };
    // Match bloque completo con fences (tolera espacios al inicio/fin)
    // ```python\n ... \n```
    const m = txt.match(/^\s*```([a-z0-9_+-]*)?\s*\n([\s\S]*?)\n```[\s]*$/i);
    if (m) {
      const lang = (m[1] || '').trim();
      const inner = m[2] ?? '';
      return { code: inner, lang, raw: txt, fenced: true };
    }
    // También soporta una línea con ```lang y cierre al final sin \n previos
    const m2 = txt.match(/^\s*```([a-z0-9_+-]*)?\s*([\s\S]*?)```[\s]*$/i);
    if (m2) {
      const lang = (m2[1] || '').trim();
      const inner = m2[2] ?? '';
      // si el inner comienza con \n, quítalo; si termina con \n, quítalo
      const cleaned = inner.replace(/^\n/, '').replace(/\n$/, '');
      return { code: cleaned, lang, raw: txt, fenced: true };
    }
    return { code: txt, lang: '', raw: txt, fenced: false };
  }


  async function sendMessage() {
    if (sending) return;
    const msg = input.value.trim();
    if (!msg) return;

    // UI: echo del usuario
    container.appendChild(makeBubble('user', escapeHTML(msg)));
    container.scrollTop = container.scrollHeight;
    input.value = '';
    sending = true;
    sendBtn.disabled = true;

    // Llamada al endpoint del ESP32
    const url = `/api/ai/generate?placa=${encodeURIComponent(placa)}&modelo=${encodeURIComponent(modelo)}`;
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: msg
      });

      const data = await parseJSON(res);

      // AIService devuelve directamente lo que respondió tu servidor (JSON):
      // { explicacion, codigo } o { ok:false, error:... }
      if (data && data.ok === false) {
        throw new Error(data.error || 'LLM request failed');
      }

      const { explicacion, codigo } = splitRespuesta(data);

      // Render de explicación
      if (explicacion) {
        container.appendChild(makeBubble(
          'assistant',
          `<div style="white-space:pre-wrap; line-height:1.25">${escapeHTML(explicacion)}</div>`
        ));
      }

      // Render de código + botones (copiar / insertar)
      // Render de código + botones (copiar / insertar)
      if (codigo) {
  const parsed = peelFencedCode(codigo);

  const codeWrap = document.createElement('div');
  codeWrap.className = 'chat-bubble';

  // guarda el texto crudo con fences por si lo quieres recuperar luego
  codeWrap.dataset.raw = parsed.raw || codigo;
  // opcional: mantener un caché global de crudos
  window.AIChatRawCache = window.AIChatRawCache || [];
  window.AIChatRawCache.push({ lang: parsed.lang, raw: parsed.raw });

  codeWrap.innerHTML = `
    <div class="chat-code-tools" style="display:flex; gap:8px; justify-content:flex-end; margin-bottom:6px;">
      <button class="btn-mini" data-act="copy" title="Copiar código sin formato">Copiar</button>
      <button class="btn-mini primary" data-act="insert" title="Insertar en el editor">Insertar en editor</button>
    </div>
    <pre class="chat-code" ${parsed.lang ? `data-lang="${parsed.lang}"` : ''}><code>${escapeHTML(parsed.code)}</code></pre>
  `;

  codeWrap.querySelector('[data-act="copy"]').addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(parsed.code); } catch {}
  });
  codeWrap.querySelector('[data-act="insert"]').addEventListener('click', () => onCode(parsed.code));

  container.appendChild(codeWrap);
}

      if (!explicacion && !codigo) {
        // fallback si vino un JSON raro
        container.appendChild(makeBubble('assistant', '<em>(Sin contenido)</em>', { small: true }));
      }
    } catch (e) {
      container.appendChild(makeBubble(
        'assistant',
        `<strong>Error:</strong> ${escapeHTML(e.message)}`
      ));
    } finally {
      sending = false;
      sendBtn.disabled = false;
      container.scrollTop = container.scrollHeight;
    }
  }

  // Eventos UI
  sendBtn.addEventListener('click', sendMessage);
  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  });

  // Devuelve helpers por si los necesitas
  return {
    sendMessage,
    setModel({ placa: p, modelo: m }) {
      if (typeof p === 'string') (this.placa = p.toLowerCase());
      if (typeof m === 'string') (this.modelo = m.toLowerCase());
    }
  };
}

