var ws = new WebSocket("/websocket");

ws.onmessage = (event) => {
  const logElement = document.getElementById("logs");
  let data = JSON.parse(event.data);
  logElement.innerHTML += "\n";
  logElement.innerHTML += data.timestamp;
  logElement.innerHTML += ("[" + data.level + "]");
  logElement.innerHTML += data.payload;
  logElement.scrollTop = logElement.scrollHeight;
};