(() => {
  "use strict";

  const history = document.getElementById("commit-list");
  const status = document.getElementById("history-status");
  const more = document.getElementById("load-more");
  const sourceLink = document.getElementById("history-external");
  const buttons = [...document.querySelectorAll("[data-branch]")];
  if (!history || !status || !more || !sourceLink) return;

  const filters = {
    dev: {
      path: "native",
      label: "native/ on dev",
      description: "Only commits changing native/ (the experimental C++ app).",
      url: "https://github.com/lsvjdev/GameOverlay/commits/dev/native/"
    },
    main: {
      path: null,
      label: "main",
      description: "The original application's full main branch history.",
      url: "https://github.com/lsvjdev/GameOverlay/commits/main/"
    },
    "gh-pages": {
      path: "docs",
      label: "docs/ on gh-pages",
      description: "Only commits changing docs/ (our GitHub Pages website).",
      url: "https://github.com/lsvjdev/GameOverlay/commits/gh-pages/docs/"
    }
  };
  const branches = Object.keys(filters);
  const scope = document.getElementById("history-scope");
  const pageSize = 50;
  let branch = "dev";
  let page = 1;
  let loading = false;
  let requestId = 0;
  let controller = null;

  const dateFormat = new Intl.DateTimeFormat("en", {
    day: "2-digit", month: "short", year: "numeric", timeZone: "UTC"
  });

  function setStatus(message) {
    status.textContent = message;
  }

  function makeEntry(commit) {
    const sha = commit.sha;
    const message = commit.commit && commit.commit.message;
    const isoDate = commit.commit && commit.commit.author && commit.commit.author.date;
    if (typeof sha !== "string" || !/^[0-9a-f]{40}$/.test(sha) ||
        typeof message !== "string") return null;

    const entry = document.createElement("li");
    const time = document.createElement("time");
    time.dateTime = isoDate || "";
    time.textContent = isoDate ? dateFormat.format(new Date(isoDate)).toUpperCase() : "NO DATE";

    const title = document.createElement("div");
    title.className = "commit-title";
    const firstLine = message.split("\n", 1)[0].trim() || "(no commit message)";
    const prefix = firstLine.match(/^(feat|fix|docs|style|build|chore|refactor|test|perf|ci):\s*/i);
    if (prefix) {
      const type = document.createElement("span");
      type.textContent = prefix[1].toLowerCase() + " /";
      title.append(type);
      title.append(document.createTextNode(firstLine.slice(prefix[0].length)));
    } else {
      title.textContent = firstLine;
    }

    const link = document.createElement("a");
    link.href = "https://github.com/lsvjdev/GameOverlay/commit/" + sha;
    link.textContent = sha.slice(0, 7) + " ↗";
    link.setAttribute("aria-label", "View commit " + sha.slice(0, 7) + " on GitHub");
    link.target = "_blank";
    link.rel = "noopener noreferrer";
    entry.append(time, title, link);
    return entry;
  }

  async function loadHistory() {
    if (loading) return;
    loading = true;
    more.disabled = true;
    more.hidden = true;
    const id = ++requestId;
    const activeBranch = branch;
    const config = filters[activeBranch];
    controller = new AbortController();
    setStatus("Loading commits from " + config.label + "…");

    const endpoint = "https://api.github.com/repos/lsvjdev/GameOverlay/commits" +
      "?sha=" + encodeURIComponent(activeBranch) +
      (config.path ? "&path=" + encodeURIComponent(config.path) : "") +
      "&per_page=" + pageSize + "&page=" + page;

    try {
      const response = await fetch(endpoint, {
        headers: { "Accept": "application/vnd.github+json" },
        signal: controller.signal
      });
      if (!response.ok) throw new Error("HTTP " + response.status);
      const commits = await response.json();
      if (id !== requestId || activeBranch !== branch) return;
      if (!Array.isArray(commits)) throw new Error("Unexpected response");

      const fragment = document.createDocumentFragment();
      for (const commit of commits) {
        const entry = makeEntry(commit);
        if (entry) fragment.append(entry);
      }
      history.append(fragment);

      const count = history.children.length;
      setStatus(count ? "Showing " + count + " matching commits from " + config.label + "." :
        "No matching commits found for this selection.");
      page += 1;
      more.hidden = commits.length < pageSize;
      more.disabled = false;
    } catch (error) {
      if (id !== requestId || error.name === "AbortError") return;
      setStatus("Couldn't load the GitHub commit feed (" + error.message +
        "). You can still browse all commits using the GitHub link above.");
      more.hidden = false;
      more.disabled = false;
      more.textContent = "Try again ↻";
    } finally {
      if (id === requestId) {
        loading = false;
      }
    }
  }

  function selectBranch(next) {
    if (!branches.includes(next)) return;
    if (controller) controller.abort();
    requestId += 1;
    loading = false;
    branch = next;
    page = 1;
    history.replaceChildren();
    more.hidden = true;
    more.textContent = "Load older commits ↓";
    sourceLink.href = filters[next].url;
    if (scope) scope.textContent = filters[next].description;
    buttons.forEach(button => {
      button.setAttribute("aria-pressed", String(button.dataset.branch === next));
    });
    loadHistory();
  }

  buttons.forEach(button => button.addEventListener("click", () => {
    if (button.dataset.branch !== branch) selectBranch(button.dataset.branch);
  }));
  more.addEventListener("click", () => {
    if (!loading) {
      more.textContent = "Load older commits ↓";
      loadHistory();
    }
  });

  selectBranch("dev");
})();