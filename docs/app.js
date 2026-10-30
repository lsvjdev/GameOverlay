(() => {
  "use strict";

  const menuButton = document.querySelector(".nav-toggle");
  const navigation = document.getElementById("site-navigation");

  if (menuButton && navigation) {
    const setOpen = (open) => {
      navigation.dataset.open = String(open);
      menuButton.setAttribute("aria-expanded", String(open));
      menuButton.setAttribute("aria-label", open ? "Close navigation" : "Open navigation");
    };

    menuButton.addEventListener("click", () => {
      setOpen(menuButton.getAttribute("aria-expanded") !== "true");
    });

    navigation.querySelectorAll("a").forEach((link) => {
      link.addEventListener("click", () => setOpen(false));
    });

    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") setOpen(false);
    });

    window.matchMedia("(min-width: 971px)").addEventListener("change", (event) => {
      if (event.matches) setOpen(false);
    });
  }

  const preview = document.querySelector("[data-demo]");
  if (preview) {
    preview.querySelectorAll("[data-preview-mode]").forEach((button) => {
      button.addEventListener("click", () => {
        const mode = button.dataset.previewMode;
        if (mode !== "strip" && mode !== "clear") return;

        preview.dataset.style = mode;
        preview.querySelectorAll("[data-preview-mode]").forEach((item) => {
          item.setAttribute("aria-pressed", String(item === button));
        });
      });
    });
  }
})();
