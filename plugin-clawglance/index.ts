/**
 * ClawGlance OpenClaw Plugin
 *
 * Replaces bridge.py by running inside the gateway process.
 * - Hooks capture transcript events in real time (push).
 * - An async setInterval polls session files, logs, and cost CLI for telemetry.
 * - HTTP routes on /api/clawglance/* serve JSON the ESP32 already understands.
 *
 * IMPORTANT: All I/O is async (fs.promises, child_process.exec) to avoid
 * blocking the gateway's event loop.
 */

import { definePluginEntry } from "openclaw/plugin-sdk/plugin-entry";
import * as fs from "node:fs";
import * as fsp from "node:fs/promises";
import * as path from "node:path";
import * as os from "node:os";
import { exec } from "node:child_process";
import { promisify } from "node:util";

const execAsync = promisify(exec);

// ── In-memory state ─────────────────────────────────────────────

interface TranscriptEntry { ts: string; type: string; text: string; }
interface ActivityEntry { ts: string; level: string; msg: string; }
interface SessionCtx { label: string; context_pct: number; }
interface SessionInfo {
  session_key: string; agent_id: string; label: string;
  status: string; tokens: number; cost: number; last_message: string;
}

const state = {
  sessions: [] as SessionInfo[],
  costs: { today: 0, tokens: 0 },
  system: { version: "", model: "", activeSessions: 0, totalSessions: 0 },
  telemetry: {
    model: "", context_max: 0, context_used: 0,
    budget_window_pct: 0, budget_window_label: "", budget_week_pct: 0,
    input_tokens: 0, output_tokens: 0,
    cache_read_tokens: 0, cache_write_tokens: 0, cache_hit_pct: 0,
    active_session_label: "", active_session_age_s: 0,
    sessions: [] as SessionCtx[],
  },
  activity: [] as ActivityEntry[],
  transcript: [] as TranscriptEntry[],
};

const OPENCLAW_DIR = path.join(os.homedir(), ".openclaw");
const MAX_TRANSCRIPT = 50;
const MAX_ACTIVITY = 20;

// ── Helpers ─────────────────────────────────────────────────────

function shortTs(): string {
  const d = new Date();
  return `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}`;
}

function toAscii(s: string, maxLen: number): string {
  let out = s.replace(/[^\x20-\x7E]/g, "").trim();
  if (out.length > maxLen) out = out.slice(0, maxLen - 3) + "...";
  return out;
}

async function runCmd(cmd: string, timeout = 10_000): Promise<string> {
  try {
    const { stdout } = await execAsync(cmd, { timeout });
    return stdout.trim();
  } catch {
    return "";
  }
}

function pushTranscript(entry: TranscriptEntry) {
  state.transcript.push(entry);
  if (state.transcript.length > MAX_TRANSCRIPT)
    state.transcript = state.transcript.slice(-MAX_TRANSCRIPT);
}

function pushActivity(entry: ActivityEntry) {
  state.activity.push(entry);
  if (state.activity.length > MAX_ACTIVITY)
    state.activity = state.activity.slice(-MAX_ACTIVITY);
}

// ── Async polling functions ─────────────────────────────────────

async function refreshSessions() {
  const sessions: SessionInfo[] = [];
  const agentsDir = path.join(OPENCLAW_DIR, "agents");
  try { await fsp.access(agentsDir); } catch { return; }

  for (const agentId of await fsp.readdir(agentsDir)) {
    const sessFile = path.join(agentsDir, agentId, "sessions", "sessions.json");
    try {
      const raw = await fsp.readFile(sessFile, "utf-8");
      const data = JSON.parse(raw);
      for (const [key, info] of Object.entries(data) as [string, any][]) {
        const updatedAt: number = info.updatedAt ?? 0;
        const ageMs = updatedAt ? Date.now() - updatedAt : 999_999_999;
        sessions.push({
          session_key: key, agent_id: agentId,
          label: key.includes(":") ? key.split(":").pop()! : key,
          status: ageMs < 120_000 ? "active" : "idle",
          tokens: 0, cost: 0, last_message: info.lastChannel ?? "",
        });
      }
    } catch { /* skip */ }
  }
  state.sessions = sessions;
}

async function refreshCosts() {
  const raw = await runCmd("openclaw gateway usage-cost");
  let cost = 0, tokens = 0;
  for (const line of raw.split("\n")) {
    if (!line.includes("Latest day")) continue;
    try {
      const dollarPart = line.split("$")[1];
      if (dollarPart) cost = parseFloat(dollarPart.split(/\s/)[0]);
      for (const part of line.split("\u00b7")) {
        const p = part.trim().toLowerCase();
        if (!p.includes("token")) continue;
        const tokStr = p.split(/\s/)[0];
        if (tokStr.endsWith("k")) tokens = Math.round(parseFloat(tokStr) * 1_000);
        else if (tokStr.endsWith("m")) tokens = Math.round(parseFloat(tokStr) * 1_000_000);
        else tokens = parseInt(tokStr, 10) || 0;
      }
    } catch { /* best effort */ }
  }
  state.costs = { today: cost, tokens };
}

async function refreshSystem() {
  const raw = await runCmd("openclaw --version", 5_000);
  state.system.version = raw;
  state.system.activeSessions = state.sessions.filter((s) => s.status === "active").length;
  state.system.totalSessions = state.sessions.length;
}

async function refreshTelemetry() {
  const raw = await runCmd("openclaw status --json", 15_000);
  let data: any;
  try { data = JSON.parse(raw); } catch { return; }

  const sessionsData = data.sessions ?? {};
  const recent: any[] = sessionsData.recent ?? [];
  const defaults = sessionsData.defaults ?? {};

  let inputTok = 0, outputTok = 0, cacheRead = 0, cacheWrite = 0;
  let contextMax = 0, contextUsed = 0;
  let model = defaults.model ?? "";
  const sessList: SessionCtx[] = [];
  let activeLabel = "", activeAgeS = 0;

  const sorted = [...recent].sort((a, b) => (a.age ?? 999_999_999) - (b.age ?? 999_999_999));
  if (sorted[0]?.model) model = sorted[0].model;
  for (const s of sorted) {
    inputTok += s.inputTokens ?? 0;
    outputTok += s.outputTokens ?? 0;
    cacheRead += s.cacheRead ?? 0;
    cacheWrite += s.cacheWrite ?? 0;
    if ((s.contextTokens ?? 0) > contextMax) contextMax = s.contextTokens ?? 0;
    contextUsed += s.totalTokens ?? 0;

    const key: string = s.key ?? "";
    const parts = key.split(":");
    let label = parts[parts.length - 1] ?? key;
    if (label.length > 20) label = label.slice(0, 20);

    const ageMs: number = s.age ?? 999_999_999;
    const isActive = ageMs < 120_000;
    if (isActive && !activeLabel) {
      activeLabel = label;
      activeAgeS = Math.floor(ageMs / 1_000);
    }
    sessList.push({ label, context_pct: s.percentUsed ?? 0 });
  }

  const totalRead = cacheRead + inputTok;
  const cacheHitPct = totalRead > 0 ? Math.floor((cacheRead * 100) / totalRead) : 0;

  // Budget from gateway log — read larger tail since budget lines may be sparse.
  // Handles both provider formats:
  //   OpenAI:    "5h window 79% left · week 93% left"   (remaining)
  //   Anthropic: "5h window 21% used · week 7% used"    (used)
  // Always output USED percentage so the ESP32 display is consistent.
  let budgetWindowUsed = 0, budgetWindowLabel = "", budgetWeekUsed = 0;
  const logPath = path.join(OPENCLAW_DIR, "logs", "gateway.log");
  try {
    const handle = await fsp.open(logPath, "r");
    const stat = await handle.stat();
    const readSize = Math.min(65536, stat.size); // read up to 64KB
    const buf = Buffer.alloc(readSize);
    await handle.read(buf, 0, readSize, Math.max(0, stat.size - readSize));
    await handle.close();
    for (const line of buf.toString("utf-8").split("\n").reverse()) {
      if (!line.includes("Usage budget:")) continue;
      const budgetRaw = line.split("budget:")[1]?.replace(/\*\*/g, "").trim() ?? "";

      // Window budget — try "left" (remaining) then "used"
      const wmLeft = budgetRaw.match(/(\d+h)\s+window\s+(\d+)%\s+left/);
      const wmUsed = budgetRaw.match(/(\d+h)\s+window\s+(\d+)%\s+used/);
      if (wmLeft) {
        budgetWindowLabel = wmLeft[1];
        budgetWindowUsed = 100 - parseInt(wmLeft[2], 10);
      } else if (wmUsed) {
        budgetWindowLabel = wmUsed[1];
        budgetWindowUsed = parseInt(wmUsed[2], 10);
      }

      // Week budget — try "left" then "used"
      const weekLeft = budgetRaw.match(/week\s+(\d+)%\s+left/);
      const weekUsed = budgetRaw.match(/week\s+(\d+)%\s+used/);
      if (weekLeft) budgetWeekUsed = 100 - parseInt(weekLeft[1], 10);
      else if (weekUsed) budgetWeekUsed = parseInt(weekUsed[1], 10);

      break;
    }
  } catch { /* best effort */ }

  state.system.model = model;
  state.telemetry = {
    model, context_max: contextMax, context_used: contextUsed,
    budget_window_pct: budgetWindowUsed, budget_window_label: budgetWindowLabel,
    budget_week_pct: budgetWeekUsed,
    input_tokens: inputTok, output_tokens: outputTok,
    cache_read_tokens: cacheRead, cache_write_tokens: cacheWrite,
    cache_hit_pct: cacheHitPct,
    active_session_label: activeLabel, active_session_age_s: activeAgeS,
    sessions: sessList.slice(0, 8),
  };
}

// Log follower state
let logTailPos = 0;
let logTailFile = "";

async function refreshActivityFromLog() {
  const LOG_DIR = "/tmp/openclaw";
  const NOISE = ["Usage cost", "Total:", "Latest day:", "console.log", "openclaw status"];
  try {
    const files = (await fsp.readdir(LOG_DIR))
      .filter((f) => f.startsWith("openclaw-") && f.endsWith(".log")).sort();
    if (files.length === 0) return;
    const logPath = path.join(LOG_DIR, files[files.length - 1]);

    if (logPath !== logTailFile) {
      logTailFile = logPath;
      const stat = await fsp.stat(logPath);
      logTailPos = Math.max(0, stat.size - 32_768);
    }

    const stat = await fsp.stat(logPath);
    if (stat.size <= logTailPos) return;

    const handle = await fsp.open(logPath, "r");
    const readSize = stat.size - logTailPos;
    const buf = Buffer.alloc(readSize);
    await handle.read(buf, 0, readSize, logTailPos);
    await handle.close();
    logTailPos = stat.size;

    for (const line of buf.toString("utf-8").split("\n")) {
      if (!line.trim()) continue;
      let entry: any;
      try { entry = JSON.parse(line); } catch { continue; }
      let msg: string = entry["1"] ?? entry["0"] ?? "";
      if (typeof msg === "object") msg = JSON.stringify(msg);
      msg = msg.replace(/[^\x20-\x7E]/g, "").trim();
      if (NOISE.some((n) => msg.includes(n)) || msg.length < 5) continue;
      const tsStr: string = entry.time ?? "";
      const shortTime = tsStr.includes("T") ? tsStr.split("T")[1]?.slice(0, 5) ?? "" : "";
      if (msg.length > 72) msg = msg.slice(0, 69) + "...";
      pushActivity({ ts: shortTime, level: (entry._meta?.logLevelName ?? "INFO").toLowerCase(), msg });
    }
  } catch { /* best effort */ }
}

// Transcript follower state (reads active session JSONL)
let transcriptFile = "";
let transcriptPos = 0;

function parseTranscriptLine(line: string): TranscriptEntry[] {
  const events: TranscriptEntry[] = [];
  let d: any;
  try { d = JSON.parse(line); } catch { return events; }

  const msg = d.message ?? {};
  const role: string = msg.role ?? "";
  const content = msg.content;
  const ts: string = d.timestamp ?? "";
  const shortTime = ts.includes("T") ? ts.split("T")[1]?.slice(0, 5) ?? "" : "";

  if (role === "assistant" && Array.isArray(content)) {
    for (const c of content) {
      if (typeof c !== "object" || !c) continue;
      if (c.type === "toolCall") {
        const name: string = c.name ?? "";
        const args = c.arguments ?? {};
        let detail: string = args.command ?? args.query ?? args.path ?? args.url ?? "";
        if (typeof detail === "string" && detail.length > 40) detail = detail.slice(0, 37) + "...";
        if (typeof detail === "string") detail = detail.replace(/[^\x20-\x7E]/g, "");
        const text = detail ? `${name}: ${detail}` : name;
        events.push({ ts: shortTime, type: "tool", text: text.slice(0, 60) });
      } else if (c.type === "text") {
        let text: string = c.text ?? "";
        if (text.startsWith("[[reply")) text = text.split("]] ").pop() ?? text;
        text = text.split("\n")[0].trim();
        if (text.length > 5) {
          text = text.replace(/[^\x20-\x7E]/g, "").slice(0, 60);
          if (text) events.push({ ts: shortTime, type: "reply", text });
        }
      }
    }
  } else if (role === "user" && typeof content === "string") {
    const lines = content.split("\n");
    let capture = false;
    for (const l of lines) {
      if (l.includes("GMT") && l.trim().startsWith("[")) { capture = true; continue; }
      if (capture && l.trim() && !l.trim().startsWith("```")) {
        let text = l.trim().replace(/[^\x20-\x7E]/g, "").slice(0, 60);
        if (text) events.push({ ts: shortTime, type: "user", text });
        break;
      }
    }
  }
  return events;
}

async function refreshTranscript() {
  const sessIndexPath = path.join(OPENCLAW_DIR, "agents", "main", "sessions", "sessions.json");
  try { await fsp.access(sessIndexPath); } catch { return; }

  let index: any;
  try { index = JSON.parse(await fsp.readFile(sessIndexPath, "utf-8")); } catch { return; }

  // Find most recent session
  let bestKey = "", bestTime = 0;
  for (const [key, info] of Object.entries(index) as [string, any][]) {
    const t: number = info.updatedAt ?? 0;
    if (t > bestTime) { bestTime = t; bestKey = key; }
  }
  if (!bestKey) return;

  const sessFile: string = index[bestKey]?.sessionFile ?? "";
  if (!sessFile) return;
  try { await fsp.access(sessFile); } catch { return; }

  // If session file changed, reset and seed with last 16KB
  if (sessFile !== transcriptFile) {
    transcriptFile = sessFile;
    state.transcript = [];
    const stat = await fsp.stat(sessFile);
    transcriptPos = Math.max(0, stat.size - 16384);
    const handle = await fsp.open(sessFile, "r");
    const seedSize = stat.size - transcriptPos;
    const buf = Buffer.alloc(seedSize);
    await handle.read(buf, 0, seedSize, transcriptPos);
    await handle.close();
    transcriptPos = stat.size;
    for (const line of buf.toString("utf-8").split("\n")) {
      if (line.trim()) {
        for (const evt of parseTranscriptLine(line)) pushTranscript(evt);
      }
    }
    return;
  }

  // Incremental read
  const stat = await fsp.stat(sessFile);
  if (stat.size <= transcriptPos) return;

  const handle = await fsp.open(sessFile, "r");
  const readSize = stat.size - transcriptPos;
  const buf = Buffer.alloc(readSize);
  await handle.read(buf, 0, readSize, transcriptPos);
  await handle.close();
  transcriptPos = stat.size;

  for (const line of buf.toString("utf-8").split("\n")) {
    if (line.trim()) {
      for (const evt of parseTranscriptLine(line)) pushTranscript(evt);
    }
  }
}

let _refreshing = false;

async function doRefresh() {
  if (_refreshing) return; // skip if previous cycle still running
  _refreshing = true;
  try {
    await refreshSessions();
    await refreshCosts();
    await refreshSystem();
    await refreshTelemetry();
    await refreshActivityFromLog();
    await refreshTranscript();
  } catch (e) {
    console.error("[clawglance] Refresh error:", e);
  } finally {
    _refreshing = false;
  }
}

// ── Plugin entry ────────────────────────────────────────────────

let _registered = false;
let _refreshTimer: ReturnType<typeof setInterval> | null = null;

export default definePluginEntry({
  id: "clawglance",
  name: "ClawGlance",
  description: "Telemetry REST endpoints for ClawGlance ESP32 dashboard",

  register(api) {
    if (_registered) return;
    _registered = true;

    const refreshMs = api.config?.refreshIntervalMs ?? 3_000;

    // ── HTTP routes ──────────────────────────────────────────

    const routes = [
      { path: "/api/clawglance/sessions", handler: () => ({ success: true, data: state.sessions }) },
      { path: "/api/clawglance/costs", handler: () => ({ success: true, data: state.costs }) },
      { path: "/api/clawglance/system", handler: () => ({ success: true, data: state.system }) },
      { path: "/api/clawglance/telemetry", handler: () => ({ success: true, data: state.telemetry }) },
      { path: "/api/clawglance/activity", handler: () => ({ success: true, data: state.activity.slice(-8) }) },
      { path: "/api/clawglance/transcript", handler: () => ({ success: true, data: state.transcript.slice(-20) }) },
    ];

    for (const route of routes) {
      api.registerHttpRoute({
        method: "GET",
        path: route.path,
        auth: "gateway",
        handler: async (req: any, res: any) => {
          const body = JSON.stringify(route.handler());
          res.statusCode = 200;
          res.setHeader("Content-Type", "application/json");
          res.end(body);
          return true;
        },
      });
    }

    // ── Chat POST route (sends via agent CLI so it writes to session) ──

    api.registerHttpRoute({
      method: "POST",
      path: "/api/clawglance/chat",
      auth: "gateway",
      handler: async (req: any, res: any) => {
        try {
          // Read request body
          const chunks: Buffer[] = [];
          for await (const chunk of req) chunks.push(chunk);
          const body = JSON.parse(Buffer.concat(chunks).toString());

          let msg = "";
          if (body.messages) {
            for (const m of body.messages) {
              if (m.role === "user") msg = m.content ?? "";
            }
          } else if (body.message) {
            msg = body.message;
          }

          if (!msg) {
            res.statusCode = 400;
            res.setHeader("Content-Type", "application/json");
            res.end(JSON.stringify({ error: "no message" }));
            return true;
          }

          // Route through openclaw agent CLI so it writes to the session
          const result = await runCmd(
            `openclaw agent --agent main --message ${JSON.stringify(msg)}`,
            60_000
          );

          let content = result;
          try {
            const parsed = JSON.parse(result);
            content = parsed.reply ?? parsed.content ?? result;
          } catch { /* use raw */ }

          res.statusCode = 200;
          res.setHeader("Content-Type", "application/json");
          res.end(JSON.stringify({
            choices: [{ message: { role: "assistant", content } }],
          }));
        } catch (e: any) {
          res.statusCode = 500;
          res.setHeader("Content-Type", "application/json");
          res.end(JSON.stringify({ error: String(e) }));
        }
        return true;
      },
    });

    // ── Start async polling ──────────────────────────────────

    setTimeout(() => doRefresh(), 10_000);
    if (_refreshTimer) clearInterval(_refreshTimer);
    _refreshTimer = setInterval(() => doRefresh(), refreshMs);

    console.log(`[clawglance] Plugin loaded — polling every ${refreshMs}ms, serving /api/clawglance/*`);
  },
});
