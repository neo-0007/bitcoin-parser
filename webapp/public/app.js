/* ════════════════════════════════════════════════════
   Utilities
════════════════════════════════════════════════════ */
const API = ""; // same origin

function sats(n) {
  if (n === undefined || n === null) return "—";
  if (n >= 1e8) return (n / 1e8).toFixed(8).replace(/\.?0+$/, "") + " BTC";
  return n.toLocaleString() + " sats";
}
function satsBTC(n) {
  if (n === undefined || n === null) return "—";
  return (n / 1e8).toFixed(8) + " BTC";
}
function trunc(s, n = 24) {
  if (!s) return "—";
  if (s.length <= n * 2 + 3) return s;
  return s.slice(0, n) + "…" + s.slice(-n);
}
function esc(s) {
  if (!s) return "";
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

/* ════════════════════════════════════════════════════
   Health check
════════════════════════════════════════════════════ */
async function checkHealth() {
  try {
    const r = await fetch(`${API}/api/health`);
    const j = await r.json();
    const dot = document.getElementById("healthDot");
    const lbl = document.getElementById("healthLabel");
    if (j.ok) {
      dot.classList.add("ok");
      lbl.textContent = "API online";
    } else {
      lbl.textContent = "API error";
    }
  } catch {
    document.getElementById("healthLabel").textContent = "API offline";
  }
}
checkHealth();

/* ════════════════════════════════════════════════════
   Tab switching
════════════════════════════════════════════════════ */
function switchTab(name) {
  document
    .querySelectorAll(".tab")
    .forEach((t) => t.classList.remove("active"));
  document
    .querySelectorAll(".tab-panel")
    .forEach((p) => p.classList.remove("active"));
  document.querySelectorAll(".tab").forEach((t) => {
    if (t.textContent.toLowerCase().includes(name)) t.classList.add("active");
  });
  document.getElementById(`tab-${name}`).classList.add("active");
}

/* ════════════════════════════════════════════════════
   File helpers
════════════════════════════════════════════════════ */
function triggerFile(id) {
  document.getElementById(id).click();
}
function showFileName(input, dropId) {
  const name = input.files[0] ? input.files[0].name : "";
  document.getElementById(dropId.replace("Drop", "Name")).textContent = name;
}

/* ════════════════════════════════════════════════════
   Analyze fixture JSON
════════════════════════════════════════════════════ */
async function analyzeFixture() {
  const raw = document.getElementById("fixtureInput").value.trim();
  if (!raw) return showError("Please paste a fixture JSON first.");
  let parsed;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return showError("Invalid JSON — please check your input.");
  }
  await runAnalysis(parsed);
}

async function analyzeRaw() {
  const tx = document.getElementById("rawTxInput").value.trim();
  const po = document.getElementById("prevoutsInput").value.trim();
  if (!tx) return showError("Please enter a raw TX hex.");
  let prevouts = [];
  if (po) {
    try {
      prevouts = JSON.parse(po);
    } catch {
      return showError("Invalid prevouts JSON array.");
    }
  }
  await runAnalysis({ network: "mainnet", raw_tx: tx, prevouts });
}

async function runAnalysis(body) {
  setLoading(true);
  hideError();
  try {
    const r = await fetch(`${API}/api/analyze`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const data = await r.json();
    if (!r.ok || data.ok === false) {
      showError(
        (data.error?.message || data.error || "Analysis failed") +
          (data.details ? "\n" + data.details : ""),
      );
    } else {
      renderTx(data);
    }
  } catch (e) {
    showError("Network error: " + e.message);
  } finally {
    setLoading(false);
  }
}

async function analyzeBlock() {
  const blk = document.getElementById("blkFile").files[0];
  const rev = document.getElementById("revFile").files[0];
  const xor = document.getElementById("xorFile").files[0];
  if (!blk || !rev || !xor)
    return showError("Please select all three block files.");
  const fd = new FormData();
  fd.append("blk", blk);
  fd.append("rev", rev);
  fd.append("xor", xor);
  setLoading(true);
  hideError();
  try {
    const r = await fetch(`${API}/api/analyze-block`, {
      method: "POST",
      body: fd,
    });
    const data = await r.json();
    if (!r.ok || data.ok === false) {
      showError(data.error?.message || data.error || "Block analysis failed");
    } else {
      renderBlock(data);
    }
  } catch (e) {
    showError("Network error: " + e.message);
  } finally {
    setLoading(false);
  }
}

function setLoading(on) {
  document.querySelectorAll(".btn-analyze").forEach((b) => {
    b.disabled = on;
    b.classList.toggle("loading", on);
  });
}
function showError(msg) {
  const el = document.getElementById("errorBox");
  el.textContent = msg;
  el.style.display = "block";
}
function hideError() {
  document.getElementById("errorBox").style.display = "none";
}

/* ════════════════════════════════════════════════════
   Render single transaction
════════════════════════════════════════════════════ */
function renderTx(d) {
  const el = document.getElementById("results");
  el.innerHTML = buildTxHTML(d);
  el.style.display = "block";
  el.scrollIntoView({ behavior: "smooth", block: "start" });
}

function buildTxHTML(d) {
  let html = "";

  // TXID banner
  html += `<div class="txid-banner fade-in">
    <div>
      <div class="txid-label">Transaction ID</div>
      <div class="txid-val">${esc(d.txid)}</div>
      ${d.wtxid && d.wtxid !== d.txid ? `<div class="txid-label" style="margin-top:8px;">Witness TXID (wtxid)</div><div class="txid-val" style="color:var(--blue)">${esc(d.wtxid)}</div>` : ""}
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center;">
      <span class="badge ${d.segwit ? "segwit" : "legacy"}" data-tip="${d.segwit ? "SegWit: witness data is separate, enabling discount on fees" : "Legacy: no witness data"}">
        ${d.segwit ? "SegWit" : "Legacy"}
      </span>
      ${d.rbf_signaling ? `<span class="badge rbf" data-tip="RBF: This transaction can be replaced (bumped) with a higher fee before confirming">RBF</span>` : ""}
      <span style="font-family:var(--mono);font-size:0.7rem;color:var(--muted)">v${d.version}</span>
    </div>
  </div>`;

  // Story cards
  html += `<div class="story-grid fade-in">
    <div class="story-card orange">
      <div class="card-label">Total In</div>
      <div class="card-value">${satsBTC(d.total_input_sats)}</div>
      <div class="card-sub">${sats(d.total_input_sats)}</div>
    </div>
    <div class="story-card green">
      <div class="card-label">Total Out</div>
      <div class="card-value">${satsBTC(d.total_output_sats)}</div>
      <div class="card-sub">${sats(d.total_output_sats)}</div>
    </div>
    <div class="story-card red">
      <div class="card-label"><span data-tip="Fee = Inputs − Outputs. Paid to miners to include your tx in a block.">Fee</span></div>
      <div class="card-value">${sats(d.fee_sats)}</div>
      <div class="card-sub">${d.fee_rate_sat_vb} sat/vB</div>
    </div>
    <div class="story-card blue">
      <div class="card-label"><span data-tip="Virtual bytes (vbytes) is the effective size used to calculate fees. SegWit witness data is discounted.">Size / vbytes</span></div>
      <div class="card-value">${d.vbytes} <span style="font-size:0.8rem;color:var(--text2)">vB</span></div>
      <div class="card-sub">${d.size_bytes}B raw · ${d.weight} WU</div>
    </div>
    <div class="story-card purple">
      <div class="card-label">Inputs / Outputs</div>
      <div class="card-value">${d.vin.length} → ${d.vout.length}</div>
      <div class="card-sub">locktime ${d.locktime_value}</div>
    </div>
  </div>`;

  // Flow diagram
  html += buildFlowDiagram(d);

  // Warnings
  if (d.warnings && d.warnings.length > 0) {
    html += buildWarnings(d.warnings);
  }

  // Segwit savings
  if (d.segwit && d.segwit_savings) {
    html += buildWeightSection(d.segwit_savings);
  }

  // Locktime + RBF meta
  html += buildMetaSection(d);

  // Inputs
  html += buildIOSection("Inputs", d.vin, true);

  // Outputs
  html += buildIOSection("Outputs", d.vout, false);

  return html;
}

/* ── Flow Diagram ── */
function buildFlowDiagram(d) {
  const MAX_ITEMS = 4;

  function inputItems() {
    let s = "";
    const items = d.vin.slice(0, MAX_ITEMS);
    for (const inp of items) {
      const v = inp.prevout?.value_sats;
      const addr = inp.address || "unknown";
      const rtl = inp.relative_timelock?.enabled
        ? `<span class="rtl-pill">🔒 ${inp.relative_timelock.value} ${inp.relative_timelock.type}</span>`
        : "";
      s += `<div class="flow-item input-item">
        <div class="flow-item-label">Input</div>
        <div class="flow-item-value">${v !== undefined ? sats(v) : "—"} ${rtl}</div>
        <div class="flow-item-sub">${esc(trunc(addr, 14))}</div>
        <div class="flow-item-sub" style="color:var(--muted)">${esc(inp.script_type || "")}</div>
      </div>`;
    }
    if (d.vin.length > MAX_ITEMS)
      s += `<div class="flow-item input-item" style="opacity:0.5;text-align:center;padding:8px;font-family:var(--mono);font-size:0.7rem;color:var(--muted)">+${d.vin.length - MAX_ITEMS} more</div>`;
    return s;
  }

  function outputItems() {
    let s = "";
    const items = d.vout.slice(0, MAX_ITEMS);
    for (const out of items) {
      const isOR = out.script_type === "op_return";
      s += `<div class="flow-item output-item${isOR ? " fee-item" : ""}">
        <div class="flow-item-label">Output #${out.n}</div>
        <div class="flow-item-value">${sats(out.value_sats)}</div>
        <div class="flow-item-sub">${esc(trunc(out.address || (isOR ? "OP_RETURN data" : out.script_type), 14))}</div>
        <div class="flow-item-sub" style="color:var(--muted)">${esc(out.script_type || "")}</div>
      </div>`;
    }
    if (d.vout.length > MAX_ITEMS)
      s += `<div class="flow-item output-item" style="opacity:0.5;text-align:center;padding:8px;font-family:var(--mono);font-size:0.7rem;color:var(--muted)">+${d.vout.length - MAX_ITEMS} more</div>`;
    // Fee slice
    s += `<div class="flow-item fee-item">
      <div class="flow-item-label">⚡ Miner Fee</div>
      <div class="flow-item-value" style="color:var(--accent)">${sats(d.fee_sats)}</div>
      <div class="flow-item-sub">${d.fee_rate_sat_vb} sat/vB</div>
    </div>`;
    return s;
  }

  return `<div class="flow-section fade-in">
    <h2>💸 Value Flow <span style="font-family:var(--mono);font-size:0.7rem;color:var(--muted);font-weight:400;">— who paid whom?</span></h2>
    <div class="flow-diagram">
      <div class="flow-col">${inputItems()}</div>
      <div class="flow-col center-col">
        <div>
          <div class="flow-arrows"><div class="arrow-line"></div></div>
          <div class="tx-box">TX</div>
          <div class="flow-arrows"><div class="arrow-line"></div></div>
        </div>
      </div>
      <div class="flow-col">${outputItems()}</div>
    </div>
    <div style="margin-top:14px;font-size:0.8rem;color:var(--text2);line-height:1.6;">
      A Bitcoin transaction consumes <strong style="color:var(--blue)">inputs</strong> (previously unspent coins) and creates new <strong style="color:var(--green)">outputs</strong> (new coins for recipients). The difference between total inputs and total outputs is the <strong style="color:var(--accent)">miner fee</strong>.
    </div>
  </div>`;
}

/* ── Warnings ── */
const WARNING_INFO = {
  HIGH_FEE: {
    icon: "🔴",
    desc: "The fee is unusually high — either in absolute sats or in sat/vB rate. Double-check before broadcasting.",
    cls: "warn-high-fee",
  },
  DUST_OUTPUT: {
    icon: "⚠️",
    desc: 'One or more outputs are below the "dust limit" (~546 sats) and may be unspendable.',
  },
  UNKNOWN_OUTPUT_SCRIPT: {
    icon: "❓",
    desc: "An output uses an unrecognized script type — could be non-standard or experimental.",
  },
  RBF_SIGNALING: {
    icon: "🔄",
    desc: "This transaction signals Replace-By-Fee (BIP125), meaning it can be replaced with a higher-fee version before confirming.",
  },
};

function buildWarnings(warnings) {
  let html = `<div class="warnings-section fade-in"><div class="section-title">Warnings</div>`;
  for (const w of warnings) {
    const info = WARNING_INFO[w.code] || { icon: "⚠️", desc: "" };
    html += `<div class="warning-item ${info.cls || ""}">
      <div class="warning-icon">${info.icon}</div>
      <div class="warning-content">
        <div class="warning-code">${esc(w.code)}</div>
        <div class="warning-desc">${info.desc || ""}</div>
      </div>
    </div>`;
  }
  html += `</div>`;
  return html;
}

/* ── Weight Section ── */
function buildWeightSection(s) {
  const actualPct = Math.min(100, (s.weight_actual / s.weight_if_legacy) * 100);
  const witnessPct = Math.min(100, (s.witness_bytes / s.total_bytes) * 100);
  const nonWitnessPct = Math.min(
    100,
    (s.non_witness_bytes / s.total_bytes) * 100,
  );

  return `<div class="weight-section fade-in">
    <h2>⚡ SegWit Weight Discount <span style="font-family:var(--mono);font-size:0.7rem;color:var(--muted);font-weight:400;">— why SegWit saves fees</span></h2>
    <div class="weight-compare">
      <div class="weight-box">
        <div class="weight-box-label">Actual Weight</div>
        <div class="weight-box-val actual">${s.weight_actual}<span class="weight-box-unit">WU</span></div>
        <div style="font-family:var(--mono);font-size:0.65rem;color:var(--text2);margin-top:4px;">${s.total_bytes} bytes</div>
      </div>
      <div class="weight-box">
        <div class="weight-box-label">If it were Legacy</div>
        <div class="weight-box-val legacy">${s.weight_if_legacy}<span class="weight-box-unit">WU</span></div>
        <div style="font-family:var(--mono);font-size:0.65rem;color:var(--text2);margin-top:4px;">no discount</div>
      </div>
    </div>
    <div class="bar-wrap">
      <div class="bar-label"><span>Actual vs Legacy weight</span><span>${actualPct.toFixed(0)}%</span></div>
      <div class="bar-track" style="height:28px;">
        <div class="bar-fill actual-bar" style="width:${actualPct.toFixed(1)}%">
          <span class="bar-val">${s.weight_actual} WU</span>
        </div>
      </div>
    </div>
    <div class="bar-wrap">
      <div class="bar-label"><span><span style="color:var(--blue)">■</span> Witness bytes <span style="color:var(--muted);margin-left:4px;">(discounted ×0.25)</span></span><span>${s.witness_bytes}B</span></div>
      <div class="bar-track">
        <div class="bar-fill witness" style="width:${witnessPct.toFixed(1)}%">
          <span class="bar-val">${s.witness_bytes}B</span>
        </div>
      </div>
    </div>
    <div class="bar-wrap">
      <div class="bar-label"><span><span style="color:var(--purple)">■</span> Non-witness bytes <span style="color:var(--muted);margin-left:4px;">(normal ×4)</span></span><span>${s.non_witness_bytes}B</span></div>
      <div class="bar-track">
        <div class="bar-fill nonwitness" style="width:${nonWitnessPct.toFixed(1)}%">
          <span class="bar-val">${s.non_witness_bytes}B</span>
        </div>
      </div>
    </div>
    <div class="savings-pill">✅ ${s.savings_pct}% smaller weight than equivalent legacy transaction</div>
    <div style="margin-top:12px;font-size:0.8rem;color:var(--text2);line-height:1.6;">
      SegWit moves signature/witness data into a separate area counted at <strong>¼ weight</strong>. This means SegWit transactions are effectively cheaper to broadcast — the same signature data costs 4× less in weight units than it would in a legacy transaction.
    </div>
  </div>`;
}

/* ── Meta section (locktime + RBF) ── */
function buildMetaSection(d) {
  const lockDesc = {
    none: "No locktime — this transaction can be included in any block.",
    block_height: `Locked until block <strong>${d.locktime_value.toLocaleString()}</strong>. The transaction cannot be included before this block height.`,
    unix_timestamp: `Locked until Unix timestamp <strong>${d.locktime_value}</strong> (${new Date(d.locktime_value * 1000).toUTCString()}). Cannot be mined before this time.`,
  };
  const rbfDesc = d.rbf_signaling
    ? "At least one input has <code>nSequence &lt; 0xFFFFFFFE</code>, opting into BIP125 RBF. The sender can broadcast a replacement transaction with a higher fee before this one confirms."
    : "No inputs signal RBF. This transaction cannot be replaced by a higher-fee version (unless the mempool applies full-RBF).";

  return `<div class="meta-section fade-in">
    <h2>🔒 Timelocks & Replaceability</h2>
    <div class="meta-row">
      <div class="meta-icon">⏰</div>
      <div class="meta-content">
        <div class="meta-title">Absolute Locktime — <span style="font-family:var(--mono);font-size:0.8rem;color:var(--accent)">${d.locktime_type}</span></div>
        <div class="meta-desc">${lockDesc[d.locktime_type] || d.locktime_value}</div>
      </div>
    </div>
    <div class="meta-row">
      <div class="meta-icon">${d.rbf_signaling ? "🔄" : "🔒"}</div>
      <div class="meta-content">
        <div class="meta-title">Replace-By-Fee (RBF) — <span style="font-family:var(--mono);font-size:0.8rem;color:${d.rbf_signaling ? "var(--red)" : "var(--green)"}">${d.rbf_signaling ? "ENABLED" : "disabled"}</span></div>
        <div class="meta-desc">${rbfDesc}</div>
      </div>
    </div>
  </div>`;
}

/* ── I/O sections ── */
function buildIOSection(title, items, isInput) {
  const icon = isInput ? "📥" : "📤";
  let html = `<div class="io-section fade-in"><h2>${icon} ${title}</h2>`;
  for (let i = 0; i < items.length; i++) {
    html += buildIOItem(items[i], i, isInput);
  }
  html += `</div>`;
  return html;
}

function buildIOItem(item, i, isInput) {
  const idx = isInput ? i : item.n;
  const scriptType = item.script_type || "unknown";
  const typeClass =
    "tp-" +
    scriptType
      .replace(/-/g, "")
      .replace("_", "")
      .replace("opreturn", "opreturn")
      .toLowerCase();
  const addr =
    item.address ||
    (item.script_type === "op_return" ? "(data output)" : "(unknown)");
  const val = isInput ? item.prevout?.value_sats : item.value_sats;

  const rtl =
    isInput && item.relative_timelock?.enabled
      ? `<span class="rtl-pill">🔒 ${item.relative_timelock.value} ${item.relative_timelock.type}</span>`
      : "";

  let html = `<div class="io-item" id="io-${isInput ? "in" : "out"}-${i}">
    <div class="io-header" onclick="toggleIO(this)">
      <span class="io-index">#${idx}</span>
      <span class="io-addr">${esc(trunc(addr, 18))}${rtl}</span>
      <span class="type-pill ${typeClass}">${esc(scriptType)}</span>
      <span class="io-val">${val !== undefined ? sats(val) : ""}</span>
      <span class="caret">▶</span>
    </div>
    <div class="io-body">`;

  // Detail grid
  html += `<div class="detail-grid">`;
  if (isInput) {
    html += dc("TXID (spending)", trunc(item.txid, 14));
    html += dc("Vout", item.vout);
    html += dc(
      "Sequence",
      "0x" + item.sequence?.toString(16).toUpperCase().padStart(8, "0"),
    );
    html += dc("Script Type", scriptType);
    if (item.address) html += dc("Address", item.address);
    if (item.prevout)
      html += dc("Prevout Value", sats(item.prevout.value_sats));
    if (item.relative_timelock?.enabled) {
      html += dc(
        "Relative Timelock",
        `${item.relative_timelock.value} ${item.relative_timelock.type}`,
      );
    }
  } else {
    html += dc("Index", item.n);
    html += dc("Value", sats(item.value_sats));
    html += dc("Script Type", scriptType);
    if (item.address) html += dc("Address", item.address);
  }
  html += `</div>`;

  // Script ASM
  if (item.script_asm !== undefined && item.script_asm !== "") {
    html += `<div style="margin-top:12px">
      <div class="dc-label">Script ASM</div>
      <div style="font-family:var(--mono);font-size:0.68rem;color:var(--text2);background:var(--bg);border-radius:6px;padding:10px 12px;word-break:break-all;line-height:1.7;">${esc(item.script_asm)}</div>
    </div>`;
  }

  // Witness
  if (isInput && item.witness && item.witness.length > 0) {
    html += `<div style="margin-top:10px">
      <div class="dc-label">Witness Stack (${item.witness.length} items)</div>`;
    item.witness.forEach((w, j) => {
      html += `<div style="font-family:var(--mono);font-size:0.68rem;color:var(--text2);background:var(--bg);border-radius:5px;padding:6px 10px;margin-top:4px;word-break:break-all;">[${j}] ${w ? esc(w) : '<em style="color:var(--muted)">(empty)</em>'}</div>`;
    });
    html += `</div>`;
  }
  if (item.witness_script_asm) {
    html += `<div style="margin-top:10px">
      <div class="dc-label">Witness Script ASM</div>
      <div style="font-family:var(--mono);font-size:0.68rem;color:var(--blue);background:var(--bg);border-radius:6px;padding:10px 12px;word-break:break-all;line-height:1.7;">${esc(item.witness_script_asm)}</div>
    </div>`;
  }

  // OP_RETURN data
  if (!isInput && item.script_type === "op_return") {
    html += `<div class="op-return-data">
      ${item.op_return_data_utf8 ? `<div style="color:var(--text);margin-bottom:4px;">"${esc(item.op_return_data_utf8)}"</div>` : ""}
      <div style="color:var(--muted);font-size:0.65rem;">${item.op_return_protocol !== "unknown" ? `Protocol: <strong style="color:var(--accent)">${item.op_return_protocol}</strong> · ` : ""}Hex: ${esc(item.op_return_data_hex) || "(empty)"}</div>
    </div>`;
  }

  // Hex details toggle
  const hasSigHex = isInput && item.script_sig_hex;
  const hasPkHex = !isInput && item.script_pubkey_hex;
  if (hasSigHex || hasPkHex) {
    const hexVal = hasSigHex ? item.script_sig_hex : item.script_pubkey_hex;
    const hexLabel = hasSigHex ? "ScriptSig Hex" : "ScriptPubKey Hex";
    const uid = `hex-${isInput ? "in" : "out"}-${i}`;
    html += `<button class="hex-toggle" onclick="toggleHex('${uid}')">🔍 Show ${hexLabel}</button>
    <div class="hex-block" id="${uid}">${esc(hexVal)}</div>`;
  }
  if (isInput && item.prevout?.script_pubkey_hex) {
    const uid2 = `hexpo-${i}`;
    html += `<button class="hex-toggle" onclick="toggleHex('${uid2}')">🔍 Show Prevout ScriptPubKey Hex</button>
    <div class="hex-block" id="${uid2}">${esc(item.prevout.script_pubkey_hex)}</div>`;
  }

  html += `</div></div>`;
  return html;
}

function dc(label, value) {
  return `<div class="detail-cell"><div class="dc-label">${esc(label)}</div><div class="dc-value">${esc(String(value ?? "—"))}</div></div>`;
}

function toggleIO(header) {
  const item = header.closest(".io-item");
  const body = item.querySelector(".io-body");
  const isOpen = body.classList.toggle("open");
  item.classList.toggle("open", isOpen);
}

function toggleHex(id) {
  const el = document.getElementById(id);
  el.style.display = el.style.display === "block" ? "none" : "block";
}

/* ════════════════════════════════════════════════════
   Render block results
════════════════════════════════════════════════════ */
function renderBlock(data) {
  const el = document.getElementById("results");

  // Handle multiple blocks in one file
  if (data.blocks && Array.isArray(data.blocks)) {
    let html = "";
    data.blocks.forEach((block, i) => {
      html +=
        `<div style="margin-bottom:32px;"><div class="section-title" style="margin-bottom:12px;">Block ${i + 1} of ${data.blocks.length}</div>` +
        buildBlockHTML(block) +
        "</div>";
    });
    el.innerHTML = html;
    el.style.display = "block";
    el.scrollIntoView({ behavior: "smooth", block: "start" });
    return;
  }

  // Single block
  el.innerHTML = buildBlockHTML(data);
  el.style.display = "block";
  el.scrollIntoView({ behavior: "smooth", block: "start" });
}

function buildBlockHTML(data) {
  // Guard — make sure it's actually a block response
  if (!data || !data.block_header) {
    return `<div class="error-box" style="display:block;">Invalid block response — missing block_header.</div>`;
  }

  const h = data.block_header;
  const bs = data.block_stats || {};
  const cb = data.coinbase || {};

  const typeColors = {
    p2wpkh: "#60a5fa",
    p2tr: "#a78bfa",
    p2pkh: "#fbbf24",
    p2sh: "#fbbf24",
    p2wsh: "#60a5fa",
    op_return: "#52525b",
    unknown: "#ef4444",
  };

  let html = `<div class="block-banner fade-in">
    <div class="section-title">Block Analysis</div>
    <div style="font-family:var(--mono);font-size:0.75rem;color:var(--accent);margin-bottom:4px;word-break:break-all;">${esc(h.block_hash)}</div>
    <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-top:6px;">
      <span class="badge ${h.merkle_root_valid ? "segwit" : "rbf"}">${h.merkle_root_valid ? "✓ Merkle Valid" : "✗ Merkle Invalid"}</span>
      <span style="font-family:var(--mono);font-size:0.7rem;color:var(--text2)">v${h.version} · nonce ${h.nonce} · bits ${esc(h.bits)}</span>
    </div>
    <div class="block-stat-row">
      ${bstat("Tx Count", data.tx_count)}
      ${bstat("BIP34 Height", cb.bip34_height ?? "—")}
      ${bstat("Total Fees", sats(bs.total_fees_sats))}
      ${bstat("Total Weight", (bs.total_weight || 0).toLocaleString() + " WU")}
      ${bstat("Avg Fee Rate", (bs.avg_fee_rate_sat_vb || 0) + " sat/vB")}
      ${bstat("Timestamp", h.timestamp ? new Date(h.timestamp * 1000).toUTCString() : "—")}
    </div>
  </div>`;

  // Script type summary
  if (bs.script_type_summary) {
    html += `<div class="flow-section fade-in">
      <h2>📊 Output Script Types</h2>
      <div class="script-type-chart">`;
    for (const [t, cnt] of Object.entries(bs.script_type_summary)) {
      html += `<div class="stc-item"><div class="stc-dot" style="background:${typeColors[t] || "#888"}"></div><span>${t}</span><span class="stc-count">${cnt}</span></div>`;
    }
    html += `</div></div>`;
  }

  // Transaction list
  html += `<div class="flow-section fade-in"><h2>📋 Transactions</h2>`;
  const txs = data.transactions || [];
  for (let i = 0; i < txs.length; i++) {
    const tx = txs[i];
    html += `<div class="tx-list-item" onclick="openTxModal(${i})">
      <span style="font-family:var(--mono);font-size:0.65rem;color:var(--muted);flex-shrink:0;">#${i}</span>
      <span class="tx-list-txid">${esc(tx.txid || "—")}</span>
      ${i === 0 ? `<span class="badge segwit">COINBASE</span>` : ""}
      <span class="tx-list-fee" style="color:var(--accent)">${sats(tx.fee_sats)}</span>
      <span style="font-family:var(--mono);font-size:0.65rem;color:var(--text2);">${tx.vbytes || "—"} vB</span>
      <span style="font-family:var(--mono);font-size:0.65rem;color:var(--muted);">▶</span>
    </div>`;
  }
  html += `</div>`;

  window._blockTxs = txs;
  return html;
}

function bstat(label, value) {
  return `<div class="block-stat"><div class="block-stat-lbl">${label}</div><div class="block-stat-val">${value ?? "—"}</div></div>`;
}

function openTxModal(idx) {
  const tx = window._blockTxs?.[idx];
  if (!tx) return;
  document.getElementById("txModalContent").innerHTML = buildTxHTML(tx);
  document.getElementById("txModal").classList.add("open");
}
function closeTxModal() {
  document.getElementById("txModal").classList.remove("open");
}

// Close modal on overlay click
document.getElementById("txModal").addEventListener("click", function (e) {
  if (e.target === this) closeTxModal();
});

/* ════════════════════════════════════════════════════
   Drag and drop for file inputs
════════════════════════════════════════════════════ */
["blkDrop", "revDrop", "xorDrop"].forEach((id) => {
  const el = document.getElementById(id);
  if (!el) return;
  el.addEventListener("dragover", (e) => {
    e.preventDefault();
    el.classList.add("dragover");
  });
  el.addEventListener("dragleave", () => el.classList.remove("dragover"));
  el.addEventListener("drop", (e) => {
    e.preventDefault();
    el.classList.remove("dragover");
    const input = el.querySelector('input[type="file"]');
    if (input && e.dataTransfer.files.length) {
      input.files = e.dataTransfer.files;
      document.getElementById(id.replace("Drop", "Name")).textContent =
        e.dataTransfer.files[0].name;
    }
  });
});
