const refs = {
  sampleNoInput: document.getElementById("sampleNoInput"),
  loadBtn: document.getElementById("loadBtn"),
  detailGrid: document.getElementById("detailGrid"),
  canvas: document.getElementById("curveCanvas"),
  stats: document.getElementById("stats"),
  error: document.getElementById("detectError"),
};

const state = {
  points: [],
  xAxisName: "数据点",
  yAxisName: "电压值",
};

function renderDetail(data) {
  const unit = data.detectedUnit ?? "";
  const formatNum = (v) => {
    const n = Number(v);
    return Number.isFinite(n) ? n.toFixed(3) : "0.000";
  };
  const fields = [
    ["项目名称", data.projectName],
    ["样品编号", data.sampleNo],
    ["样品来源", data.sampleSource],
    ["样品名称", data.sampleName],
    ["标准曲线", data.standardCurve],
    ["批次号", data.batchCode],
    ["检测浓度", `${Number(data.detectedConc).toFixed(3)} ${unit}`.trim()],
    ["参考值", `${Number(data.referenceValue).toFixed(3)} ${unit}`.trim()],
    ["C", formatNum(data.C)],
    ["T", formatNum(data.T)],
    ["C/T比值", formatNum(data.radio ?? data.ratio)],
    ["检测结果", data.result],
    ["检测时间", data.detectedTime],
    ["检测单位", unit || "-"],
    ["检测人", data.detectedPerson],
    ["稀释倍数", data.dilutionInfo],
  ];

  refs.detailGrid.textContent = "";
  for (const [k, v] of fields) {
    const box = document.createElement("div");
    const dt = document.createElement("dt");
    const dd = document.createElement("dd");
    dt.textContent = k;
    dd.textContent = v ?? "-";
    box.append(dt, dd);
    refs.detailGrid.appendChild(box);
  }
}

function renderStats(_pointCount, stats) {
  refs.stats.textContent = "";
  const cells = [
    `最小值：${Number(stats.min).toFixed(3)}`,
    `最大值：${Number(stats.max).toFixed(3)}`,
    `平均值：${Number(stats.avg).toFixed(3)}`,
  ];
  for (const text of cells) {
    const span = document.createElement("span");
    span.textContent = text;
    refs.stats.appendChild(span);
  }
}

function drawCurve(points) {
  const canvas = refs.canvas;
  const dpr = window.devicePixelRatio || 1;
  const width = Math.max(320, Math.floor(canvas.clientWidth));
  const height = Math.max(220, Math.floor(canvas.clientHeight));
  canvas.width = Math.floor(width * dpr);
  canvas.height = Math.floor(height * dpr);

  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);

  const margin = {
    top: 20,
    right: 16,
    bottom: 48,
    left: 64,
  };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;

  ctx.fillStyle = "#fcfeff";
  ctx.fillRect(0, 0, width, height);

  if (!points || points.length === 0 || plotW <= 0 || plotH <= 0) {
    return;
  }

  let min = Number.POSITIVE_INFINITY;
  let max = Number.NEGATIVE_INFINITY;
  for (const v of points) {
    if (v < min) min = v;
    if (v > max) max = v;
  }
  if (Math.abs(max - min) < 1e-9) {
    max = min + 1;
  }

  const yPad = (max - min) * 0.08;
  const yMin = min - yPad;
  const yMax = max + yPad;

  const xTicks = 6;
  const yTicks = 5;

  ctx.strokeStyle = "#d8e0ea";
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (let i = 0; i <= xTicks; i += 1) {
    const x = margin.left + (plotW * i) / xTicks + 0.5;
    ctx.moveTo(x, margin.top);
    ctx.lineTo(x, margin.top + plotH);
  }
  for (let i = 0; i <= yTicks; i += 1) {
    const y = margin.top + (plotH * i) / yTicks + 0.5;
    ctx.moveTo(margin.left, y);
    ctx.lineTo(margin.left + plotW, y);
  }
  ctx.stroke();

  ctx.strokeStyle = "#60758b";
  ctx.lineWidth = 1.2;
  ctx.beginPath();
  ctx.moveTo(margin.left + 0.5, margin.top);
  ctx.lineTo(margin.left + 0.5, margin.top + plotH);
  ctx.lineTo(margin.left + plotW, margin.top + plotH + 0.5);
  ctx.stroke();

  ctx.fillStyle = "#2d3f53";
  ctx.font = "12px sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  for (let i = 0; i <= yTicks; i += 1) {
    const y = margin.top + (plotH * i) / yTicks;
    const v = yMax - ((yMax - yMin) * i) / yTicks;
    ctx.fillText(v.toFixed(1), margin.left - 8, y);
  }

  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  const hasMultiplePoints = points.length > 1;
  for (let i = 0; i <= xTicks; i += 1) {
    const x = margin.left + (plotW * i) / xTicks;
    const idx = hasMultiplePoints ? Math.round(((points.length - 1) * i) / xTicks) + 1 : 1;
    ctx.fillText(String(idx), x, margin.top + plotH + 8);
  }

  ctx.fillStyle = "#1f3449";
  ctx.font = "13px sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "bottom";
  ctx.fillText(state.xAxisName, margin.left + plotW / 2, height - 4);

  ctx.save();
  ctx.translate(14, margin.top + plotH / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillText(state.yAxisName, 0, 0);
  ctx.restore();

  ctx.strokeStyle = "#0f6ab4";
  ctx.lineWidth = 1.6;
  if (hasMultiplePoints) {
    ctx.beginPath();
    for (let i = 0; i < points.length; i += 1) {
      const x = margin.left + (plotW * i) / (points.length - 1);
      const y = margin.top + ((yMax - points[i]) / (yMax - yMin)) * plotH;
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
    return;
  }

  const y = margin.top + ((yMax - points[0]) / (yMax - yMin)) * plotH;
  const x = margin.left + plotW / 2;
  ctx.fillStyle = "#0f6ab4";
  ctx.beginPath();
  ctx.arc(x, y, 2.5, 0, Math.PI * 2);
  ctx.fill();
}

async function loadData() {
  refs.error.textContent = "";
  const sampleNo = (refs.sampleNoInput.value || "").trim();
  if (!sampleNo) {
    refs.error.textContent = "请输入样品编号后查询";
    return;
  }
  refs.sampleNoInput.value = sampleNo;

  try {
    const [detailRes, curveRes] = await Promise.all([
      fetch(`/api/detect/detail?sampleNo=${encodeURIComponent(sampleNo)}`),
      fetch(`/api/detect/curve?sampleNo=${encodeURIComponent(sampleNo)}`),
    ]);

    const detailPayload = await detailRes.json();
    const curvePayload = await curveRes.json();

    if (!detailRes.ok || detailPayload.code !== 0) {
      throw new Error(detailPayload.message || "详情接口失败");
    }
    if (!curveRes.ok || curvePayload.code !== 0) {
      throw new Error(curvePayload.message || "曲线接口失败");
    }

    renderDetail(detailPayload.data);
    renderStats(curvePayload.data.pointCount, curvePayload.data.stats);

    state.points = curvePayload.data.adcValues || [];
    state.xAxisName = curvePayload.data.xAxisName || "数据点";
    state.yAxisName = curvePayload.data.yAxisName || "电压值";
    drawCurve(state.points);
  } catch (err) {
    refs.error.textContent = `加载失败：${err.message}`;
  }
}

refs.loadBtn.addEventListener("click", () => {
  void loadData();
});

refs.sampleNoInput.addEventListener("keydown", (ev) => {
  if (ev.key === "Enter") {
    void loadData();
  }
});

window.addEventListener("resize", () => {
  if (state.points.length > 0) {
    drawCurve(state.points);
  }
});

const urlSampleNo = new URLSearchParams(window.location.search).get("sampleNo");
if (urlSampleNo) {
  refs.sampleNoInput.value = urlSampleNo;
}

if (urlSampleNo) {
  void loadData();
}
