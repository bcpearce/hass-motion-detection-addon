const directoryPath = "/media/saved/";
const gallery = document.getElementById("gallery");

const queryString = window.location.search;
const urlParams = new URLSearchParams(queryString);

const page = Number(urlParams.get("page")) || 0;
const imgsPerPage = Number(urlParams.get("imgsPerPage")) || 20;

fetch(directoryPath)
    .then((response) => {
      if (!response.ok) {
        throw new Error("Failed to get saved images list");
      }
      return response.text();
    })
    .then((data) => {
      const parser = new DOMParser();
      const doc = parser.parseFromString(data, "text/html");

      const linkElements = doc.querySelectorAll("a");

      const links = [];
      linkElements.forEach((element) => {
        if (element.href.endsWith(".jpg")) {
          links.push({
            src : directoryPath + element.href.split("/").pop(),
            alt : element.href,
          });
        }
      });

      insertPageLinks(links, "page-list-top");
      insertPageLinks(links, "page-list-bottom");

      const totalImageCount = document.getElementById("total-image-count");
      if (links.length > 0) {
        totalImageCount.innerHTML = `(${links.length})`;
      }

      links.sort().reverse();

      let inGallery =
          page > 0 ? links.slice((page - 1) * imgsPerPage, page * imgsPerPage)
                   : links;
      inGallery.forEach((link, idx) => {
        const imgElement = document.createElement("img");
        imgElement.src = link.src;
        imgElement.alt = link.alt;
        imgElement.id = `saved-image-${idx + 1}`;
        gallery.appendChild(imgElement);
      });
    });

const insertPageLinks = (links, listElementName) => {
  const pageCount = Math.ceil(links.length / imgsPerPage);
  const pageList = document.getElementById(listElementName);

  if (pageCount > 1 && page != 1) {
    const prevElement = document.createElement("li");
    const linkElement = document.createElement("a");
    linkElement.href = `?page=${page - 1}&imgsPerPage=${imgsPerPage}`;
    const span1Element = document.createElement("span");
    span1Element.setAttribute("aria-hidden", "true");
    span1Element.innerHTML =
        "&laquo;"; // Use a left arrow for the previous link
    const span2Element = document.createElement("span");
    span2Element.classList.add("visuallyhidden");
    span2Element.innerHTML = "previous page";
    linkElement.appendChild(span1Element);
    linkElement.appendChild(span2Element);
    prevElement.appendChild(linkElement);
    pageList.appendChild(prevElement);
  }

  for (let i = 1; i <= pageCount; i++) {
    const pageElement = document.createElement("li");
    const linkElement = document.createElement("a");
    linkElement.href = `?page=${i}&imgsPerPage=${imgsPerPage}`;
    if (i == page) {
      linkElement.ariaCurrent = "page"; // Set aria-current for the current page
    }
    linkElement.innerHTML = i;
    const span1Element = document.createElement("span");
    span1Element.classList.add("visuallyhidden");
    span1Element.innerHTML = "page"; // Use a right arrow for the previous link
    linkElement.appendChild(span1Element);
    pageElement.appendChild(linkElement);
    pageList.appendChild(pageElement);
  }

  if (pageCount > 1 && page != pageCount) {
    const nextElement = document.createElement("li");
    const linkElement = document.createElement("a");
    linkElement.href = `?page=${page + 1}&imgsPerPage=${imgsPerPage}`;
    const span1Element = document.createElement("span");
    span1Element.setAttribute("aria-hidden", "true");
    span1Element.innerHTML =
        "&raquo;"; // Use a right arrow for the previous link
    const span2Element = document.createElement("span");
    span2Element.classList.add("visuallyhidden");
    span2Element.innerHTML = "next page";
    linkElement.appendChild(span1Element);
    linkElement.appendChild(span2Element);
    nextElement.appendChild(linkElement);
    pageList.appendChild(nextElement);
  }
};
