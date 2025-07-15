document.addEventListener("DOMContentLoaded", () => {
  const startBtn = document.querySelector(".btn:nth-child(1)");
  const stopBtn = document.querySelector(".btn:nth-child(2)");
  const restartBtn = document.querySelector(".btn:nth-child(3)");

  // WebSocket base URL
  const wsBase = `ws://${location.host}`;

  // Sockets
  const sockets = {
    run: new WebSocket(`${wsBase}/ws/run`),
    stop: new WebSocket(`${wsBase}/ws/stop`),
    fs_ls: new WebSocket(`${wsBase}/ws/fs_ls`),
  };

  // Print received messages
  Object.values(sockets).forEach(sock => {
    sock.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        console.log(`[${msg.type}]`, msg.text || msg);
      } catch {
        console.warn("Mensaje inválido", e.data);
      }
    };
  });

  // Enviar comandos
  const sendJson = (sock, obj) => {
    if (sock.readyState === WebSocket.OPEN) {
      sock.send(JSON.stringify(obj));
    } else {
      console.warn("WebSocket no conectado aún.");
    }
  };

  startBtn.onclick = () => {
    const code = `from ideaboard import IdeaBoard\r\nib = IdeaBoard()\r\nib.pixel = (0, 255, 0)`;
    sendJson(sockets.run, { type: "run", code });
  };

  stopBtn.onclick = () => {
    sendJson(sockets.stop, { type: "stop" });
  };

  restartBtn.onclick = () => {
    alert("Aún no implementado");
    // Podrías añadir un socket para reset y llamarlo aquí
  };
});
