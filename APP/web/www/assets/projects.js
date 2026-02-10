const PAGE_SIZE = 20;

const state = {
  keyword: "",
  page: 1,
  pageSize: PAGE_SIZE,
  total: 0,
  currentItems: [],
  selectedMap: new Map(), // rowKey -> project row
};

const refs = {
  keywordInput: document.getElementById("keywordInput"),
  searchBtn: document.getElementById("searchBtn"),
  resetBtn: document.getElementById("resetBtn"),
  exportBtn: document.getElementById("exportBtn"),
  summary: document.getElementById("summaryText"),
  selectedInfo: document.getElementById("selectedInfo"),
  tableWrap: document.getElementById("projectTableWrap"),
  rows: document.getElementById("projectRows"),
  error: document.getElementById("projectError"),
  prevBtn: document.getElementById("prevBtn"),
  nextBtn: document.getElementById("nextBtn"),
  pageInfo: document.getElementById("pageInfo"),
  selectPageCheckbox: document.getElementById("selectPageCheckbox"),
};

function getRowKey(item) {
  if (item && item.id !== undefined && item.id !== null && item.id !== "") {
    return `id:${item.id}`;
  }
  return `sampleNo:${item?.sampleNo ?? ""}|detectedTime:${item?.detectedTime ?? ""}|batchCode:${item?.batchCode ?? ""}`;
}

function formatNumber(v) {
  const num = Number(v);
  return Number.isFinite(num) ? num.toFixed(3) : "0.000";
}

function toDetailUrl(sampleNo) {
  return `/detect?sampleNo=${encodeURIComponent(sampleNo)}`;
}

function xmlEscape(text) {
  return String(text ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&apos;");
}

function makeCell(value, typeHint) {
  if (typeHint === "String") {
    return `<Cell><Data ss:Type="String">${xmlEscape(value)}</Data></Cell>`;
  }
  if (typeHint === "Number") {
    const num = Number(value);
    return `<Cell><Data ss:Type="Number">${Number.isFinite(num) ? num : 0}</Data></Cell>`;
  }

  const num = Number(value);
  if (typeof value === "number" || (value !== "" && Number.isFinite(num) && String(value).trim() !== "")) {
    return `<Cell><Data ss:Type="Number">${num}</Data></Cell>`;
  }
  return `<Cell><Data ss:Type="String">${xmlEscape(value)}</Data></Cell>`;
}

function makeRow(cells) {
  return `<Row>${cells
    .map((c) => {
      if (c && typeof c === "object" && !Array.isArray(c) && Object.prototype.hasOwnProperty.call(c, "value")) {
        return makeCell(c.value, c.type);
      }
      return makeCell(c);
    })
    .join("")}</Row>`;
}

function buildWorkbookXml(projectRows, adcColumns) {
  const projectHeaders = [
    "项目名称",
    "样品编号",
    "样品来源",
    "样品名称",
    "标准曲线",
    "批次号",
    "检测浓度",
    "参考值",
    "C",
    "T",
    "C/T比值",
    "检测结果",
    "检测时间",
    "检测单位",
    "检测人",
    "稀释倍数",
  ];

  const projectSheetRows = [
    makeRow(projectHeaders),
    ...projectRows.map((p) =>
      makeRow([
        p.projectName ?? "",
        { value: p.sampleNo ?? "", type: "String" },
        p.sampleSource ?? "",
        p.sampleName ?? "",
        p.standardCurve ?? "",
        p.batchCode ?? "",
        p.detectedConc ?? "",
        p.referenceValue ?? "",
        p.C ?? "",
        p.T ?? "",
        p.radio ?? p.ratio ?? "",
        p.result ?? "",
        p.detectedTime ?? "",
        p.detectedUnit ?? "",
        p.detectedPerson ?? "",
        p.dilutionInfo ?? "",
      ])
    ),
  ].join("");

  const adcHeaders = [{ value: "数据点序号", type: "String" }, ...projectRows.map((p) => ({ value: p.sampleNo ?? "", type: "String" }))];
  let maxLen = 0;
  for (const arr of adcColumns) {
    if (arr.length > maxLen) {
      maxLen = arr.length;
    }
  }
  const adcRows = [makeRow(adcHeaders)];
  for (let i = 0; i < maxLen; i += 1) {
    const row = [i + 1];
    for (const arr of adcColumns) {
      row.push(i < arr.length ? arr[i] : "");
    }
    adcRows.push(makeRow(row));
  }

  return `<?xml version="1.0" encoding="UTF-8"?>
<?mso-application progid="Excel.Sheet"?>
<Workbook xmlns="urn:schemas-microsoft-com:office:spreadsheet"
 xmlns:o="urn:schemas-microsoft-com:office:office"
 xmlns:x="urn:schemas-microsoft-com:office:excel"
 xmlns:ss="urn:schemas-microsoft-com:office:spreadsheet"
 xmlns:html="http://www.w3.org/TR/REC-html40">
  <Worksheet ss:Name="结果">
    <Table>
      ${projectSheetRows}
    </Table>
  </Worksheet>
  <Worksheet ss:Name="曲线数据">
    <Table>
      ${adcRows.join("")}
    </Table>
  </Worksheet>
</Workbook>`;
}

function downloadXls(xml, fileName) {
  const blob = new Blob([xml], { type: "application/vnd.ms-excel;charset=utf-8;" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

function updateSelectionInfo() {
  const count = state.selectedMap.size;
  refs.selectedInfo.textContent = `已勾选 ${count} 项`;
  refs.exportBtn.disabled = count === 0;
}

function updateSelectPageCheckbox() {
  const pageItems = state.currentItems || [];
  if (!refs.selectPageCheckbox) {
    return;
  }
  if (pageItems.length === 0) {
    refs.selectPageCheckbox.checked = false;
    refs.selectPageCheckbox.indeterminate = false;
    refs.selectPageCheckbox.disabled = true;
    return;
  }

  refs.selectPageCheckbox.disabled = false;
  const selectedCount = pageItems.filter((item) => state.selectedMap.has(getRowKey(item))).length;
  refs.selectPageCheckbox.checked = selectedCount === pageItems.length;
  refs.selectPageCheckbox.indeterminate = selectedCount > 0 && selectedCount < pageItems.length;
}

function renderRows(items) {
  refs.rows.textContent = "";
  state.currentItems = items || [];
  if (!items || items.length === 0) {
    const tr = document.createElement("tr");
    const td = document.createElement("td");
    td.colSpan = 16;
    td.textContent = "当前页无数据";
    tr.appendChild(td);
    refs.rows.appendChild(tr);
    updateSelectPageCheckbox();
    return;
  }

  for (const item of items) {
    const tr = document.createElement("tr");
    const rowKey = getRowKey(item);

    const checkTd = document.createElement("td");
    const checkbox = document.createElement("input");
    checkbox.type = "checkbox";
    checkbox.className = "table-check row-checkbox";
    checkbox.checked = state.selectedMap.has(rowKey);
    checkbox.addEventListener("change", () => {
      if (checkbox.checked) {
        state.selectedMap.set(rowKey, item);
      } else {
        state.selectedMap.delete(rowKey);
      }
      updateSelectionInfo();
      updateSelectPageCheckbox();
    });
    checkTd.appendChild(checkbox);

    const sampleTd = document.createElement("td");
    const jumpBtn = document.createElement("button");
    jumpBtn.className = "link-btn";
    jumpBtn.type = "button";
    jumpBtn.textContent = item.sampleNo || "-";
    jumpBtn.addEventListener("click", () => {
      window.location.href = toDetailUrl(item.sampleNo || "");
    });
    sampleTd.appendChild(jumpBtn);

    const cells = [
      item.projectName ?? "-",
      item.sampleSource ?? "-",
      item.sampleName ?? "-",
      `${formatNumber(item.detectedConc)} ${item.detectedUnit ?? ""}`.trim(),
      `${formatNumber(item.referenceValue)} ${item.detectedUnit ?? ""}`.trim(),
      formatNumber(item.C),
      formatNumber(item.T),
      formatNumber(item.radio ?? item.ratio),
      item.result ?? "-",
      item.detectedTime ?? "-",
      item.detectedPerson ?? "-",
      item.standardCurve ?? "-",
      item.batchCode ?? "-",
      item.dilutionInfo ?? "-",
    ];

    tr.append(checkTd, sampleTd);
    for (const text of cells) {
      const td = document.createElement("td");
      td.textContent = text;
      tr.appendChild(td);
    }
    refs.rows.appendChild(tr);
  }
  updateSelectPageCheckbox();
}

function renderPager() {
  const totalPages = Math.max(1, Math.ceil(state.total / state.pageSize));
  refs.pageInfo.textContent = `第 ${state.page} / ${totalPages} 页`;
  refs.prevBtn.disabled = state.page <= 1;
  refs.nextBtn.disabled = state.page >= totalPages;
}

function setupTableDrag() {
  const wrap = refs.tableWrap;
  if (!wrap) {
    return;
  }

  const drag = {
    active: false,
    startX: 0,
    startScrollLeft: 0,
  };

  wrap.addEventListener("mousedown", (ev) => {
    if (ev.button !== 0) {
      return;
    }
    if (ev.target.closest("button,input,a,label")) {
      return;
    }

    drag.active = true;
    drag.startX = ev.clientX;
    drag.startScrollLeft = wrap.scrollLeft;
    wrap.classList.add("dragging");
  });

  window.addEventListener("mousemove", (ev) => {
    if (!drag.active) {
      return;
    }
    const delta = ev.clientX - drag.startX;
    wrap.scrollLeft = drag.startScrollLeft - delta;
    ev.preventDefault();
  });

  const stopDrag = () => {
    if (!drag.active) {
      return;
    }
    drag.active = false;
    wrap.classList.remove("dragging");
  };

  window.addEventListener("mouseup", stopDrag);
  window.addEventListener("blur", stopDrag);
}

async function loadProjects() {
  refs.error.textContent = "";
  refs.summary.textContent = "加载中...";
  refs.pageInfo.textContent = "";
  state.keyword = (refs.keywordInput.value || "").trim();

  const params = new URLSearchParams();
  params.set("limit", String(state.pageSize));
  params.set("page", String(state.page));
  if (state.keyword) {
    params.set("keyword", state.keyword);
  }

  try {
    const res = await fetch(`/api/project/list?${params.toString()}`);
    const payload = await res.json();
    if (!res.ok || payload.code !== 0) {
      throw new Error(payload.message || `HTTP状态 ${res.status}`);
    }
    const data = payload.data || {};
    const items = data.items || [];
    state.total = Number(data.total) || 0;
    state.page = Number(data.page) || state.page;
    state.pageSize = Number(data.pageSize) || PAGE_SIZE;
    refs.summary.textContent = `共 ${state.total} 条，点击“样品编号”进入检测详情`;
    renderRows(items);
    renderPager();
    updateSelectionInfo();
  } catch (err) {
    refs.summary.textContent = "";
    refs.error.textContent = `加载失败：${err.message}`;
  }
}

async function exportSelected() {
  if (state.selectedMap.size === 0) {
    refs.error.textContent = "请先勾选要导出的项目";
    return;
  }

  refs.error.textContent = "";
  refs.exportBtn.disabled = true;
  refs.exportBtn.textContent = "导出中...";
  try {
    const projectRows = Array.from(state.selectedMap.values());
    const curvePayloads = await Promise.all(
      projectRows.map(async (p) => {
        const sampleNo = p.sampleNo || "";
        const res = await fetch(`/api/detect/curve?sampleNo=${encodeURIComponent(sampleNo)}`);
        const payload = await res.json();
        if (!res.ok || payload.code !== 0) {
          throw new Error(`样品 ${sampleNo} 曲线查询失败：${payload.message || `HTTP ${res.status}`}`);
        }
        return payload.data?.adcValues || [];
      })
    );

    const workbookXml = buildWorkbookXml(projectRows, curvePayloads);
    const now = new Date();
    const stamp = `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, "0")}${String(now.getDate()).padStart(
      2,
      "0"
    )}_${String(now.getHours()).padStart(2, "0")}${String(now.getMinutes()).padStart(2, "0")}${String(
      now.getSeconds()
    ).padStart(2, "0")}`;
    downloadXls(workbookXml, `项目导出_${stamp}.xls`);
  } catch (err) {
    refs.error.textContent = `导出失败：${err.message}`;
  } finally {
    refs.exportBtn.textContent = "导出已勾选.xls";
    updateSelectionInfo();
  }
}

refs.searchBtn.addEventListener("click", () => {
  state.page = 1;
  void loadProjects();
});

refs.resetBtn.addEventListener("click", () => {
  refs.keywordInput.value = "";
  state.page = 1;
  void loadProjects();
});

refs.keywordInput.addEventListener("keydown", (ev) => {
  if (ev.key === "Enter") {
    state.page = 1;
    void loadProjects();
  }
});

refs.prevBtn.addEventListener("click", () => {
  if (state.page > 1) {
    state.page -= 1;
    void loadProjects();
  }
});

refs.nextBtn.addEventListener("click", () => {
  const totalPages = Math.max(1, Math.ceil(state.total / state.pageSize));
  if (state.page < totalPages) {
    state.page += 1;
    void loadProjects();
  }
});

if (refs.selectPageCheckbox) {
  refs.selectPageCheckbox.addEventListener("change", () => {
    const checked = refs.selectPageCheckbox.checked;
    for (const item of state.currentItems) {
      const rowKey = getRowKey(item);
      if (checked) {
        state.selectedMap.set(rowKey, item);
      } else {
        state.selectedMap.delete(rowKey);
      }
    }
    renderRows(state.currentItems);
    updateSelectionInfo();
  });
}

refs.exportBtn.addEventListener("click", () => {
  void exportSelected();
});

setupTableDrag();
updateSelectionInfo();
void loadProjects();
