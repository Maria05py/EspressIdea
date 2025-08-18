    const chatInput = document.getElementById("chatInput");
    const sendBtn = document.getElementById("sendBtn");
    const chatMessages = document.getElementById("chatMessages");

    sendBtn.addEventListener("click", () => {
      const text = chatInput.value.trim();
      if (text !== "") {
        const msg = document.createElement("div");
        msg.className = "chat-bubble-user";
        msg.innerText = text;
        chatMessages.appendChild(msg);
        chatInput.value = "";
        chatMessages.scrollTop = chatMessages.scrollHeight;
      }
    });

    chatInput.addEventListener("keypress", function (e) {
      if (e.key === "Enter") {
        e.preventDefault();
        sendBtn.click();
      }
    });
