const state = {
  path: ".",
};

const refs = {
  pathInput: document.getElementById("pathInput"),
  pathView: document.getElementById("pathView"),
  rows: document.getElementById("fileRows"),
  error: document.getElementById("errorMsg"),
  goBtn: document.getElementById("goBtn"),
  upBtn: document.getElementById("upBtn"),
  refreshBtn: document.getElementById("refreshBtn"),
};

function normalizePath(raw) {
  if (!raw || raw.trim() === "" || raw === "/") {
    return ".";
  }
  return raw.replaceAll("\\", "/").replace(/^\/+/, "") || ".";
}

function toDisplayPath(path) {
  if (path === ".") {
    return "/";
  }
  return `/${path}`;
}

function parentPath(path) {
  if (!path || path === ".") {
    return ".";
  }
  const idx = path.lastIndexOf("/");
  return idx <= 0 ? "." : path.slice(0, idx);
}

function joinPath(base, name) {
  if (!base || base === ".") {
    return name;
  }
  return `${base}/${name}`;
}

function formatSize(size) {
  if (size < 1024) return `${size} B`;
  if (size < 1024 * 1024) return `${(size / 1024).toFixed(1)} KB`;
  if (size < 1024 * 1024 * 1024) return `${(size / 1024 / 1024).toFixed(1)} MB`;
  return `${(size / 1024 / 1024 / 1024).toFixed(1)} GB`;
}

function renderRows(entries) {
  refs.rows.textContent = "";
  const sorted = [...entries].sort((a, b) => {
    if (a.type === b.type) {
      return a.name.localeCompare(b.name);
    }
    return a.type === "directory" ? -1 : 1;
  });

  for (const item of sorted) {
    const tr = document.createElement("tr");
    tr.dataset.kind = item.type;

    const nameTd = document.createElement("td");
    if (item.type === "directory") {
      const btn = document.createElement("button");
      btn.className = "link-btn";
      btn.textContent = item.name;
      btn.addEventListener("click", () => {
        void loadList(joinPath(state.path, item.name));
      });
      nameTd.appendChild(btn);
    } else {
      nameTd.textContent = item.name;
    }

    const typeTd = document.createElement("td");
    typeTd.textContent = item.type;

    const sizeTd = document.createElement("td");
    sizeTd.textContent = item.type === "directory" ? "-" : formatSize(item.size);

    const mtimeTd = document.createElement("td");
    mtimeTd.textContent = item.mtime || "-";

    tr.append(nameTd, typeTd, sizeTd, mtimeTd);
    refs.rows.appendChild(tr);
  }
}

async function loadList(path) {
  const safePath = normalizePath(path);
  refs.error.textContent = "";
  refs.pathView.textContent = `Loading ${toDisplayPath(safePath)} ...`;

  try {
    const res = await fetch(`/api/files?path=${encodeURIComponent(safePath)}`);
    const payload = await res.json();
    if (!res.ok || payload.code !== 0) {
      throw new Error(payload.message || `HTTP ${res.status}`);
    }
    state.path = payload.data.path || ".";
    refs.pathInput.value = state.path;
    refs.pathView.textContent = `Current path: ${toDisplayPath(state.path)}`;
    renderRows(payload.data.entries || []);
  } catch (err) {
    refs.error.textContent = `Failed: ${err.message}`;
    refs.pathView.textContent = `Current path: ${toDisplayPath(state.path)}`;
  }
}

refs.goBtn.addEventListener("click", () => void loadList(refs.pathInput.value));
refs.refreshBtn.addEventListener("click", () => void loadList(state.path));
refs.upBtn.addEventListener("click", () => void loadList(parentPath(state.path)));
refs.pathInput.addEventListener("keydown", (ev) => {
  if (ev.key === "Enter") {
    void loadList(refs.pathInput.value);
  }
});

void loadList(".");
