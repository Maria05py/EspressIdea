// /JavaScript/fs.js
// Módulo ES para el explorador/FS del dispositivo Python.
// Exporta: initFS(), ensureIdle(), FS (API pública)
// --- NUEVO: EventEmitter muy simple basado en EventTarget ---
const emitter = new EventTarget();
export function on(eventName, handler){
  emitter.addEventListener(eventName, (ev) => handler(ev.detail));
}
export function off(eventName, handler){
  emitter.removeEventListener(eventName, handler);
}

// --- NUEVO: bandera para delegar el pintado al editor externo ---
let externalEditor = false;
export function setExternalEditor(on){ externalEditor = !!on; }

// --- NUEVO: writeFile arbitrario (sin depender del textarea interno) ---
export function writeFile(path, text){
  const b64 = base64Encode(text || "");
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/write?path=" + encodeURIComponent(path), {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: b64
    }))
    .then(res => {
      if (!res.ok) throw new Error(res.error || "write failed");
      return listDir(state.cwd).catch(()=>{});
    });
}



const $  = (sel, root) => (root || document).querySelector(sel);
const $$ = (sel, root) => Array.prototype.slice.call((root || document).querySelectorAll(sel));

function base64Encode(str){ return btoa(unescape(encodeURIComponent(str))); }
function base64Decode(b64){ return decodeURIComponent(escape(atob(b64))); }

function fetchJSON(url, init){
  return fetch(url, init || {}).then(res =>
    res.text().then(txt => {
      let data = null;
      try { data = JSON.parse(txt); } catch(e) {}
      if (!res.ok) {
        const msg = (data && data.error) ? data.error : ("HTTP " + res.status);
        throw new Error(msg);
      }
      return data !== null ? data : txt;
    })
  );
}

function joinPath(dir, name){
  if (!dir || dir === "/") return "/" + String(name).replace(/^\/+/, "");
  return dir.replace(/\/+$/, "") + "/" + String(name).replace(/^\/+/, "");
}
function escapeHTML(s){
  return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;");
}

// ===== Estado =====
const state = {
  cwd: "/",
  openFile: null,
  selected: null // { path, name, dir(bool) }
};

// ===== Elementos (se resuelven en initFS) =====
const els = {
  tree:        null,
  editor:      null,
  tab:         null,
  btnStop:     null,
  btnSave:     null,
  btnDownload: null,
  btnUpload:   null,
  btnRefresh:  null,
  btnNewFolder:null,
  btnRename:   null,
  btnDelete:   null,
  btnInfo:     null
};

// ===== ensure_idle =====
let lastEnsure = 0;
export function ensureIdle(){
  const now = Date.now();
  if (now - lastEnsure < 250) return Promise.resolve();
  lastEnsure = now;
  return fetchJSON("/api/repl/ensure_idle", { method:"POST" }).then(j => {
    if (j && j.ok === false) throw new Error(j.error || "ensure_idle failed");
  });
}

// ===== Árbol y selección =====
function highlightSelected(){
  $$(".file-tree li").forEach(li => li.classList.remove("selected"));
  if (!state.selected) return;
  const match = $$(".file-tree li").find(li => li.dataset && li.dataset.name === state.selected.name);
  if (match) match.classList.add("selected");
}
function selectItem(name, isDir){
  const path = joinPath(state.cwd, name);
  state.selected = { name, dir: !!isDir, path };
  highlightSelected();
}
function clearSelection(){
  state.selected = null;
  highlightSelected();
}
function markSelectedFileOpen(path){
  const name = path.split("/").pop();
  state.selected = { name, dir:false, path };
  highlightSelected();
}

function renderTree(files){
  els.tree.innerHTML = "";

  // Ignorar archivos con nombre sospechoso (los de basura tipo "      print(name+'")
  files = files.filter(f => {
    if (!f || !f.name) return false;
    // descartar los que empiecen con espacios + "print(" o cosas así
    const trimmed = f.name.trim();
    if (trimmed.startsWith("print(") || trimmed.startsWith("print") || trimmed.includes("print(")) {
      return false;
    }
    return true;
  });


  if (state.cwd !== "/") {
    const liUp = document.createElement("li");
    liUp.className = "up-item";
    liUp.innerHTML = '<i class="bi bi-arrow-90deg-up"></i> ..';
    liUp.addEventListener("click", () => {
      ensureIdle()
        .then(() => {
          const parent = state.cwd.replace(/\/+$/, "").split("/").slice(0, -1).join("/") || "/";
          return listDir(parent);
        })
        .catch(e => alert(e.message));
    });
    els.tree.appendChild(liUp);
  }

  files.sort((a,b) => (b.dir - a.dir) || a.name.localeCompare(b.name, "es"));

  files.forEach(f => {
    const li = document.createElement("li");
    li.className = "file-item";
    li.dataset.name = f.name;
    li.dataset.dir  = f.dir ? "1" : "0";
    li.title = f.dir ? "Carpeta" : "Archivo";
    li.innerHTML = (f.dir ? '<i class="bi bi-folder"></i> ' : '<i class="bi bi-file-earmark-code"></i> ') + escapeHTML(f.name);

    // click: carpeta => navegar; archivo => abrir
    li.addEventListener("click", () => {
      if (f.dir) {
        ensureIdle().then(() => listDir(joinPath(state.cwd, f.name)))
                    .catch(e => alert(e.message));
        return;
      }
      openFile(joinPath(state.cwd, f.name));
    });

    // click medio => seleccionar
    li.addEventListener("auxclick", (e) => { if (e.button === 1) selectItem(f.name, !!f.dir); });

    // menú contextual
    li.addEventListener("contextmenu", (e) => {
      e.preventDefault();
      selectItem(f.name, !!f.dir);
      openContextMenu(e.clientX, e.clientY, { name: f.name, dir: !!f.dir, path: joinPath(state.cwd, f.name) });
    });

    els.tree.appendChild(li);
  });

  if (state.selected && state.selected.path.startsWith(state.cwd + "/")) {
    highlightSelected();
  }
}

// ===== Operaciones de FS =====
export function listDir(path){
  return fetchJSON("/api/fs/list?path=" + encodeURIComponent(path)).then(j => {
    if (!j.ok) throw new Error(j.error || "list failed");
    state.cwd = j.path || path;
    clearSelection();
    renderTree(j.files || []);
  });
}

export function openFile(path){
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/read?path=" + encodeURIComponent(path)))
    .then(j => {
      if (!j.ok) throw new Error(j.error || "read failed");
      const text = base64Decode(j.base64 || "");
      state.openFile = path;

      if (externalEditor) {
        // Notificar al editor modular
        emitter.dispatchEvent(new CustomEvent("file:opened", {
          detail: { path, text }
        }));
      } else {
        // Comportamiento legacy (sin editor modular)
        els.editor.value = text;
        els.tab.textContent = path.split("/").pop() || path;
        markSelectedFileOpen(path);
      }
    })
    .catch(e => alert(e.message));
}


export function saveActiveFile(){
  if (!state.openFile) { alert("No hay archivo activo."); return Promise.resolve(); }
  const b64 = base64Encode(els.editor.value);
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/write?path=" + encodeURIComponent(state.openFile), {
      method: "POST", headers: { "Content-Type": "text/plain" }, body: b64
    }))
    .then(res => {
      if (!res.ok) throw new Error(res.error || "save failed");
      return listDir(state.cwd).catch(()=>{});
    })
    .then(() => alert("Archivo guardado ✅"))
    .catch(err => alert(err.message));
}

export function downloadActiveFile(){
  if (!state.openFile) { alert("No hay archivo activo."); return; }
  ensureIdle()
    .then(() => {
      const url = "/api/fs/download?path=" + encodeURIComponent(state.openFile);
      window.open(url, "_blank");
    })
    .catch(e => alert(e.message));
}
export function uploadToCwd(){
  const inp = document.createElement("input");
  inp.type = "file";
  inp.onchange = async () => {
    const f = inp.files && inp.files[0];
    if (!f) return;

    try {
      await ensureIdle();

      const path = joinPath(state.cwd, f.name);
      const CHUNK = 64 * 1024; // 64KB (ajustable)
      let offset = 0;
      let append = 0;

      while (offset < f.size) {
        const end  = Math.min(offset + CHUNK, f.size);
        const blob = f.slice(offset, end);
        const buf  = await blob.arrayBuffer();

        const url = `/api/fs/upload?path=${encodeURIComponent(path)}&append=${append}`;
        const res = await fetch(url, {
          method: "POST",
          headers: { "Content-Type": "application/octet-stream" },
          body: buf
        });
        if (!res.ok) {
          const txt = await res.text().catch(()=> "");
          throw new Error(`Upload HTTP ${res.status} ${txt}`);
        }

        offset = end;
        append = 1; // siguientes trozos: append
      }

      await listDir(state.cwd);
      alert(`Subido: ${f.name} ✅`);
    } catch (e) {
      alert("Error al subir: " + e.message);
    }
  };
  inp.click();
}

// ---- utilidades REST extra ----
export function apiInfo(path){
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/info?path=" + encodeURIComponent(path)))
    .then(j => { if (!j.ok) throw new Error(j.error || "info failed"); return j.info; });
}
export function apiExists(path){
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/exists?path=" + encodeURIComponent(path)))
    .then(j => { if (!j.ok) throw new Error(j.error || "exists failed"); return !!j.exists; });
}
export function apiMkdir(path){
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/mkdir?path=" + encodeURIComponent(path), { method:"POST" }))
    .then(j => { if (!j.ok) throw new Error(j.error || "mkdir failed"); });
}
export function apiRename(fromPath, toPath){
  return ensureIdle()
    .then(() => {
      const q = "/api/fs/rename?from=" + encodeURIComponent(fromPath) + "&to=" + encodeURIComponent(toPath);
      return fetchJSON(q, { method:"POST" });
    })
    .then(j => { if (!j.ok) throw new Error(j.error || "rename failed"); });
}
export function apiDeleteFile(path){
  return ensureIdle()
    .then(() => fetchJSON("/api/fs/delete?path=" + encodeURIComponent(path), { method:"POST" }))
    .then(j => { if (!j.ok) throw new Error(j.error || "delete failed"); });
}
export function apiRmdir(path, recursive){
  return ensureIdle()
    .then(() => {
      const url = "/api/fs/rmdir?path=" + encodeURIComponent(path) + (recursive ? "&recursive=1" : "");
      return fetchJSON(url, { method:"POST" });
    })
    .then(j => { if (!j.ok) throw new Error(j.error || "rmdir failed"); });
}

// ===== Menú contextual =====
const ctx = (() => {
  const div = document.createElement("div");
  div.className = "ctx-menu";
  document.body.appendChild(div);

  function hide(){ div.style.display = "none"; }
  function show(x,y, items){
    div.innerHTML = "";
    items.forEach(it => {
      if (it === "|") {
        const s = document.createElement("div"); s.className="sep"; div.appendChild(s); return;
      }
      const el = document.createElement("div"); el.className="item";
      el.innerHTML = (it.icon ? '<i class="'+it.icon+'"></i>' : '') + '<span>'+it.label+'</span>';
      el.addEventListener("click", () => { hide(); it.onClick && it.onClick(); });
      div.appendChild(el);
    });
    div.style.display = "block";
    div.style.left = Math.min(x, window.innerWidth - div.offsetWidth - 4) + "px";
    div.style.top  = Math.min(y, window.innerHeight - div.offsetHeight - 4) + "px";
  }
  window.addEventListener("click", hide);
  window.addEventListener("scroll", hide, true);
  return { show, hide };
})();

function openContextMenu(x, y, target){
  const isDir = !!target.dir;
  const items = [];

  if (isDir) {
    items.push({ icon:"bi bi-folder2-open", label:"Abrir carpeta", onClick:() => listDir(target.path) });
    items.push({ icon:"bi bi-file-earmark-plus", label:"Nuevo archivo aquí", onClick:() => {
      const fname = prompt("Nombre de archivo:"); if (!fname) return;
      const full = joinPath(target.path, fname);
      ensureIdle()
        .then(() => {
          const b64 = base64Encode("");
          return fetchJSON("/api/fs/write?path=" + encodeURIComponent(full), {
            method: "POST", headers: { "Content-Type":"text/plain" }, body:b64
          });
        })
        .then(j => { if (!j.ok) throw new Error(j.error||"write failed"); return listDir(state.cwd); })
        .catch(e => alert(e.message));
    }});
    items.push({ icon:"bi bi-folder-plus", label:"Nueva carpeta aquí", onClick:() => {
      const d = prompt("Nombre de carpeta:"); if (!d) return;
      apiMkdir(joinPath(target.path, d)).then(() => listDir(state.cwd)).catch(e => alert(e.message));
    }});
    items.push("|");
  } else {
    items.push({ icon:"bi bi-journal-code", label:"Abrir archivo", onClick:() => openFile(target.path) });
  }

  items.push({ icon:"bi bi-info-circle", label:"Info", onClick:() => {
    apiInfo(target.path)
      .then(info => alert(
        "Nombre: " + info.name + "\n" +
        "Tamaño: " + info.size + " bytes\n" +
        "Directorio: " + (info.dir ? "sí":"no")
      ))
      .catch(e => alert(e.message));
  }});

  items.push({ icon:"bi bi-input-cursor-text", label:"Renombrar", onClick:() => {
    const base = target.path.split("/").slice(0,-1).join("/") || "/";
    const nuevo = prompt("Nuevo nombre:", target.name);
    if (!nuevo || nuevo === target.name) return;
    apiRename(target.path, joinPath(base, nuevo))
      .then(() => listDir(state.cwd))
      .catch(e => alert(e.message));
  }});

  items.push({ icon:"bi bi-trash3", label:"Eliminar", onClick:() => {
    if (isDir) {
      const rec = confirm("¿Eliminar carpeta de forma recursiva?\n(Aceptar = SI, Cancelar = NO)");
      apiRmdir(target.path, rec)
        .then(() => listDir(state.cwd))
        .catch(e => alert(e.message));
    } else {
      if (!confirm("¿Eliminar archivo '"+ target.name +"'?")) return;
      apiDeleteFile(target.path)
        .then(() => listDir(state.cwd))
        .catch(e => alert(e.message));
    }
  }});

  ctx.show(x, y, items);
}

// ===== Wire de botones (FS) =====
function wireButtons(){
  if (els.btnRefresh)   els.btnRefresh.addEventListener("click", () => ensureIdle().then(() => listDir(state.cwd)));
  if (els.btnNewFolder) els.btnNewFolder.addEventListener("click", () => {
    const d = prompt("Nombre de carpeta (en " + state.cwd + "):"); if (!d) return;
    apiMkdir(joinPath(state.cwd, d)).then(() => listDir(state.cwd)).catch(e => alert(e.message));
  });
  if (els.btnRename)    els.btnRename.addEventListener("click", () => {
    if (!state.selected){ alert("Selecciona un elemento (clic derecho o clic medio)."); return; }
    const base = state.selected.path.split("/").slice(0,-1).join("/") || "/";
    const nuevo = prompt("Nuevo nombre:", state.selected.name);
    if (!nuevo || nuevo === state.selected.name) return;
    apiRename(state.selected.path, joinPath(base, nuevo))
      .then(() => listDir(state.cwd))
      .catch(e => alert(e.message));
  });
  if (els.btnDelete)    els.btnDelete.addEventListener("click", () => {
    if (!state.selected){ alert("Selecciona un elemento (clic derecho o clic medio)."); return; }
    if (state.selected.dir) {
      const rec = confirm("¿Eliminar carpeta de forma recursiva?\n(Aceptar = SI, Cancelar = NO)");
      apiRmdir(state.selected.path, rec).then(() => listDir(state.cwd)).catch(e => alert(e.message));
    } else {
      if (!confirm("¿Eliminar archivo '"+ state.selected.name +"'?")) return;
      apiDeleteFile(state.selected.path).then(() => listDir(state.cwd)).catch(e => alert(e.message));
    }
  });
  if (els.btnInfo)      els.btnInfo.addEventListener("click", () => {
    if (!state.selected){ alert("Selecciona un elemento (clic derecho o clic medio)."); return; }
    apiInfo(state.selected.path)
      .then(info => alert(
        "Nombre: " + info.name + "\n" +
        "Tamaño: " + info.size + " bytes\n" +
        "Directorio: " + (info.dir ? "sí":"no")
      ))
      .catch(e => alert(e.message));
  });

  // Editor
  if (els.btnSave)      els.btnSave.addEventListener("click", () => saveActiveFile());
  if (els.btnDownload)  els.btnDownload.addEventListener("click", () => downloadActiveFile());
  if (els.btnUpload)    els.btnUpload.addEventListener("click", () => uploadToCwd());

  // Ctrl+S para guardar
  window.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
      e.preventDefault(); saveActiveFile();
    }
  });
}

// ===== Inicialización pública =====
export function initFS(){
  els.tree        = $("#fileTree");
  els.editor      = $("#codeEditor");
  els.tab         = $("#activeTab");
  els.btnStop     = $("#btnStop");
  els.btnSave     = $("#btnSave");
  els.btnDownload = $("#btnDownload");
  els.btnUpload   = $("#btnUpload");
  els.btnRefresh  = $("#btnRefresh");
  els.btnNewFolder= $("#btnNewFolder");
  els.btnRename   = $("#btnRename");
  els.btnDelete   = $("#btnDelete");
  els.btnInfo     = $("#btnInfo");

  if (!els.tree || !els.editor || !els.tab) {
    console.warn("[FS] Elementos base no presentes; omitiendo init FS.");
    return;
  }
  wireButtons();
  // Si quieres, puedes arrancar con el árbol vacío y esperar al STOP
  // ensureIdle().then(() => listDir("/"));
}

// ===== API pública (para otros módulos) =====
export const FS = {
  listDir, openFile, saveActiveFile, downloadActiveFile, uploadToCwd,
  apiInfo, apiExists, apiMkdir, apiRename, apiDeleteFile, apiRmdir,
  ensureIdle, writeFile, // ← nuevo
  get state(){ return Object.assign({}, state); }
};
