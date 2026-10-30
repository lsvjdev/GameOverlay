(() => {
  "use strict";

  const screen = document.getElementById("game-screen");
  const hud = document.getElementById("demo-hud");
  const metricsArea = document.getElementById("metric-controls");
  const opacityInput = document.getElementById("opacity-range");
  const opacityLabel = document.getElementById("opacity-label");
  const opacityHint = document.getElementById("opacity-hint");
  if (!screen || !hud || !metricsArea) return;

  const source = [
    { id: "FPS", value: "60", enabled: true },
    { id: "Time", value: "07:42 PM", enabled: true },
    { id: "CPU", value: "42% 74°C", enabled: true },
    { id: "GPU", value: "67% 71°C", enabled: true },
    { id: "VRAM", value: "54%", enabled: true },
    { id: "RAM", value: "58%", enabled: true }
  ];
  const scenes = {
    wuwa: "Wuthering Waves gameplay screenshot",
    zzz: "Zenless Zone Zero gameplay screenshot",
    minecraft: "Minecraft gameplay screenshot"
  };

  function fitOverlay() {
    // Keep the simulated strip on one line, even on narrow phone screens.
    const available = Math.max(0, screen.clientWidth - 12);
    const naturalWidth = hud.scrollWidth;
    const scale = naturalWidth > available && naturalWidth > 0
      ? available / naturalWidth : 1;
    hud.style.setProperty("--hud-scale", String(Math.min(1, scale)));
  }

  function drawOverlay() {
    hud.replaceChildren();
    source.filter(metric => metric.enabled).forEach(metric => {
      const item = document.createElement("span");
      item.className = "metric";
      const label = document.createElement("span");
      label.className = "key";
      label.textContent = metric.id;
      const value = document.createElement("span");
      value.className = "value";
      value.textContent = metric.value;
      item.append(label, value);
      hud.append(item);
    });
    hud.setAttribute("aria-label", source.filter(metric => metric.enabled)
      .map(metric => metric.id + " " + metric.value).join(", ") + " (simulated)");
    fitOverlay();
  }

  function drawControls() {
    metricsArea.replaceChildren();
    source.forEach((metric, index) => {
      const row = document.createElement("div");
      row.className = "metric-row";
      const label = document.createElement("label");
      const toggle = document.createElement("input");
      toggle.type = "checkbox";
      toggle.checked = metric.enabled;
      toggle.addEventListener("change", () => {
        metric.enabled = toggle.checked;
        drawOverlay();
      });
      label.append(toggle, document.createTextNode(metric.id));
      const move = document.createElement("div");
      move.className = "move-buttons";
      for (const [direction, arrow, title] of [[-1, "↑", "Move up"], [1, "↓", "Move down"]]) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "move-button";
        button.textContent = arrow;
        button.title = title + ": " + metric.id;
        button.setAttribute("aria-label", title + ": " + metric.id);
        button.disabled = index + direction < 0 || index + direction >= source.length;
        button.addEventListener("click", () => {
          const other = index + direction;
          [source[index], source[other]] = [source[other], source[index]];
          drawControls();
          drawOverlay();
        });
        move.append(button);
      }
      row.append(label, move);
      metricsArea.append(row);
    });
  }

  document.querySelectorAll("[data-scene]").forEach(button => {
    if (button === screen) return;
    button.addEventListener("click", () => {
      const scene = button.dataset.scene;
      if (!Object.hasOwn(scenes, scene)) return;
      screen.dataset.scene = scene;
      screen.querySelector(".game-scene").setAttribute("aria-label", scenes[scene]);
      document.querySelectorAll(".scene-button").forEach(item => {
        item.setAttribute("aria-pressed", String(item === button));
      });
    });
  });

  document.querySelectorAll("button[data-style]").forEach(button => {
    button.addEventListener("click", () => {
      const style = button.dataset.style;
      screen.dataset.style = style;
      document.querySelectorAll("button[data-style]").forEach(item => {
        item.setAttribute("aria-pressed", String(item === button));
      });
      const disabled = style === "clear";
      opacityInput.disabled = disabled;
      opacityLabel.textContent = disabled ? "N/A" : opacityInput.value + "%";
      opacityHint.textContent = disabled
        ? "Transparent text always stays fully opaque."
        : "Text stays opaque. Adjust the background only.";
      fitOverlay();
    });
  });

  document.querySelectorAll("button[data-position]").forEach(button => {
    button.addEventListener("click", () => {
      screen.dataset.position = button.dataset.position;
      document.querySelectorAll("button[data-position]").forEach(item => {
        item.setAttribute("aria-pressed", String(item === button));
      });
    });
  });

  opacityInput.addEventListener("input", () => {
    const value = Math.max(0, Math.min(100, Number(opacityInput.value)));
    screen.style.setProperty("--bar-opacity", String(value / 100));
    opacityLabel.textContent = value + "%";
  });

  drawControls();
  drawOverlay();
  if ("ResizeObserver" in window) {
    new ResizeObserver(fitOverlay).observe(screen);
  } else {
    window.addEventListener("resize", fitOverlay);
  }
})();
