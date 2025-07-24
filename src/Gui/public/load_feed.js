const loadFeed =
    (feedId) => {
      const currentUrl = new URL(window.location.href);
      currentUrl.searchParams.set("feedId", feedId);
      window.location.href = currentUrl.toString();
    }

                window.addEventListener("load", (event) => {
                  const queryString = window.location.search;
                  const urlParams = new URLSearchParams(queryString);
                  const feedIdParam = urlParams.get("feedId");

                  fetch("/media/feeds")
                      .then((response) => {
                        if (!response.ok) {
                          throw new Error("Failed to get saved images list");
                        }
                        return response.text();
                      })
                      .then((data) => {
                        let feeds = JSON.parse(data);
                        const feedSelector =
                            document.getElementById("feed-selector");
                        feeds.forEach((feed, idx) => {
                          const optElement = document.createElement("option");
                          optElement.value = feed;
                          optElement.text = feed;
                          feedSelector.appendChild(optElement);
                          if (feed == feedIdParam) {
                            feedSelector.selectedIndex = idx + 1;
                          }
                        });
                      });
                });
