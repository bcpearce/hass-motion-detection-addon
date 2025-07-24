window.addEventListener("load", (event) => {
  const queryString = window.location.search;
  const urlParams = new URLSearchParams(queryString);
  const feedIdParam = urlParams.get("feedId");

  var ws = new WebSocket("/websocket");

  ws.onmessage = (event) => {
    const logElement = document.getElementById("logs");
    let data = JSON.parse(event.data);
    if ("log" in data) {
      logElement.innerHTML += "\n";
      logElement.innerHTML += data.log.timestamp;
      logElement.innerHTML += "[" + data.log.level + "]";
      logElement.innerHTML += data.log.payload;
      logElement.scrollTop = logElement.scrollHeight;
    }
  };

  document.getElementById("img-live").src = `/media/live/${feedIdParam}`;
  document.getElementById("img-model").src = `/media/model/${feedIdParam}`;
  document.getElementById("saved-images").href =
      `/saved_images.html?feedId=${feedIdParam}`;
});
