const directoryPath = '/media/saved/';
const gallery = document.getElementById('gallery');

fetch(directoryPath)
    .then(response => {
      if (!response.ok) {
        throw new Error("Failed to get saved images list");
      }
      return response.text();
    })
    .then(data => {
      const parser = new DOMParser();
      const doc = parser.parseFromString(data, "text/html");

      const linkElements = doc.querySelectorAll("a");

      const links = [];
      linkElements.forEach(element => {
        if (element.href.endsWith(".jpg")) {
          const filePath = directoryPath + element.href.split('/').pop();
          const imgElement = document.createElement('img');
          imgElement.src = filePath;
          imgElement.alt = element.href;
          gallery.appendChild(imgElement);
        }
      });
    });
