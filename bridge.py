#!/usr/bin/env python3
"""
ClawGlance Bridge — HTTP proxy for OpenClaw gateway data.
Runs on the same machine as OpenClaw, exposes REST endpoints for the ESP32.

Usage: python3 bridge.py
       Listens on 0.0.0.0:7001
"""
import json
import glob
import os
import re
import subprocess
import http.server
import time
import threading

PORT = 7001
REFRESH_INTERVAL = 3
OPENCLAW_DIR = os.path.expanduser("~/.openclaw")

_data = {
    "sessions": [],
    "costs": {"today": 0, "tokens": 0},
    "system": {"version": "", "status": "live"},
    "telemetry": {
        "model": "",
        "context_max": 0, "context_used": 0,
        "budget_pct": 0,
        "input_tokens": 0, "output_tokens": 0,
        "cache_read_tokens": 0, "cache_write_tokens": 0,
        "cache_hit_pct": 0,
        "sessions": [],
    },
    "activity": [],
    "transcript": [],
}
_lock = threading.Lock()

def run_cmd(args, timeout=10):
    try:
        r = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        return r.stdout.strip()
    except Exception as e:
        return f"ERROR: {e}"

def refresh_sessions():
    sessions = []
    agents_dir = os.path.join(OPENCLAW_DIR, "agents")
    if not os.path.isdir(agents_dir):
        return sessions
    for agent_id in os.listdir(agents_dir):
        sess_file = os.path.join(agents_dir, agent_id, "sessions", "sessions.json")
        if not os.path.isfile(sess_file):
            continue
        try:
            with open(sess_file) as f:
                data = json.load(f)
            for key, info in data.items():
                updated_at = info.get("updatedAt", 0)
                age_ms = (time.time() * 1000) - updated_at if updated_at else 999999999
                status = "active" if age_ms < 120000 else "idle"
                sessions.append({
                    "session_key": key,
                    "agent_id": agent_id,
                    "label": key.split(":")[-1] if ":" in key else key,
                    "status": status,
                    "tokens": 0,
                    "cost": 0,
                    "last_message": info.get("lastChannel", ""),
                })
        except Exception as e:
            print(f"[bridge] Error reading {sess_file}: {e}")
    return sessions

def refresh_costs():
    raw = run_cmd(["openclaw", "gateway", "usage-cost"], timeout=10)
    cost = 0.0
    tokens = 0
    for line in raw.split("\n"):
        if "Latest day" in line:
            try:
                cost = float(line.split("$")[1].split()[0])
                parts = line.split("\u00b7")
                for part in parts:
                    part = part.strip()
                    if "tokens" in part.lower():
                        tok_str = part.split()[0].strip()
                        if tok_str.lower().endswith("k"):
                            tokens = int(float(tok_str[:-1]) * 1000)
                        elif tok_str.lower().endswith("m"):
                            tokens = int(float(tok_str[:-1]) * 1000000)
                        else:
                            tokens = int(tok_str)
            except:
                pass
    return {"today": cost, "tokens": tokens}

def refresh_system():
    raw = run_cmd(["openclaw", "--version"], timeout=5)
    return {"version": raw.strip(), "status": "live"}

def refresh_telemetry():
    """Parse openclaw status --json for detailed telemetry."""
    raw = run_cmd(["openclaw", "status", "--json"], timeout=15)
    try:
        data = json.loads(raw)
    except:
        return _data["telemetry"]  # return last known

    sessions_data = data.get("sessions", {})
    recent = sessions_data.get("recent", [])
    defaults = sessions_data.get("defaults", {})

    # Aggregate tokens across all sessions
    input_tok = 0
    output_tok = 0
    cache_read = 0
    cache_write = 0
    context_max = 0
    context_used = 0
    model = defaults.get("model", "")
    sess_list = []
    active_session_label = ""
    active_session_age_s = 0

    # Sort by age (most recent first)
    recent_sorted = sorted(recent, key=lambda s: s.get("age", 999999999))

    for s in recent_sorted:
        input_tok += s.get("inputTokens", 0)
        output_tok += s.get("outputTokens", 0)
        cache_read += s.get("cacheRead", 0)
        cache_write += s.get("cacheWrite", 0)

        s_ctx = s.get("contextTokens", 0)
        s_total = s.get("totalTokens", 0)
        s_pct = s.get("percentUsed", 0)

        if s_ctx > context_max:
            context_max = s_ctx
        context_used += s_total

        if s.get("model"):
            model = s["model"]

        # Short label from key
        key = s.get("key", "")
        parts = key.split(":")
        label = parts[-1] if len(parts) > 2 else parts[-1] if parts else key
        if len(label) > 20:
            label = label[:20]

        age_ms = s.get("age", 999999999)
        is_active = age_ms < 120000  # active if updated < 2min ago

        # Track most recent active session
        if is_active and not active_session_label:
            active_session_label = label
            active_session_age_s = int(age_ms / 1000)

        sess_list.append({
            "label": label,
            "context_pct": s_pct,
            "status": "active" if is_active else "idle",
        })

    # Cache hit rate
    total_read = cache_read + input_tok
    cache_hit_pct = int(cache_read * 100 / total_read) if total_read > 0 else 0

    # Parse OpenAI usage budget from gateway log (free, no LLM call)
    budget_window_pct = 0
    budget_window_label = ""
    budget_week_pct = 0
    log_path = os.path.join(OPENCLAW_DIR, "logs", "gateway.log")
    if os.path.isfile(log_path):
        try:
            with open(log_path, "rb") as f:
                f.seek(0, 2)
                size = f.tell()
                f.seek(max(0, size - 8192))
                tail = f.read().decode("utf-8", errors="replace")
            for line in reversed(tail.split("\n")):
                if "Usage budget:" in line:
                    raw = line.split("budget:", 1)[1].strip().replace("**", "")
                    # Parse "5h window 75% left (13m) · week 92% left"
                    import re
                    # Window: "Xh window N% left"
                    m = re.search(r'(\d+h)\s+window\s+(\d+)%\s+left', raw)
                    if m:
                        budget_window_label = m.group(1)
                        budget_window_pct = int(m.group(2))
                    # Week: "week N% left"
                    m = re.search(r'week\s+(\d+)%\s+left', raw)
                    if m:
                        budget_week_pct = int(m.group(1))
                    break
        except:
            pass

    return {
        "model": model,
        "context_max": context_max,
        "context_used": context_used,
        "budget_window_pct": budget_window_pct,
        "budget_window_label": budget_window_label,
        "budget_week_pct": budget_week_pct,
        "input_tokens": input_tok,
        "output_tokens": output_tok,
        "cache_read_tokens": cache_read,
        "cache_write_tokens": cache_write,
        "cache_hit_pct": cache_hit_pct,
        "active_session_label": active_session_label,
        "active_session_age_s": active_session_age_s,
        "sessions": sess_list[:8],
    }

def log_follower():
    """Tail the gateway log file directly for real-time events."""
    NOISE = ["Usage cost", "Total:", "Latest day:", "console.log", "openclaw status"]
    LOG_DIR = "/tmp/openclaw"

    while True:
        try:
            # Find today's log file
            log_files = sorted(glob.glob(os.path.join(LOG_DIR, "openclaw-*.log")))
            if not log_files:
                time.sleep(5)
                continue
            log_path = log_files[-1]

            with open(log_path, "r") as f:
                # Read last 32KB for initial seed, then follow
                f.seek(0, 2)
                size = f.tell()
                f.seek(max(0, size - 32768))
                while True:
                    line = f.readline()
                    if not line:
                        time.sleep(0.5)
                        continue
                    line = line.strip()
                    if not line:
                        continue

                    # Parse JSON log line
                    try:
                        entry = json.loads(line)
                    except:
                        continue

                    # Extract message from openclaw's log format
                    msg = entry.get("1", entry.get("0", ""))
                    if isinstance(msg, dict):
                        msg = str(msg)
                    # Strip non-ASCII (LVGL fonts don't have Unicode symbols)
                    msg = msg.encode("ascii", errors="replace").decode("ascii").replace("?", "").strip()
                    level_name = entry.get("_meta", {}).get("logLevelName", "INFO").lower()
                    ts_str = entry.get("time", "")

                    # Filter noise
                    if any(n in msg for n in NOISE):
                        continue
                    if not msg or len(msg) < 5:
                        continue

                    # Short timestamp
                    short_ts = ""
                    if "T" in ts_str:
                        short_ts = ts_str.split("T")[1][:5]  # "13:41"

                    # Truncate
                    if len(msg) > 72:
                        msg = msg[:69] + "..."

                    event = {"ts": short_ts, "level": level_name, "msg": msg}
                    with _lock:
                        _data["activity"].append(event)
                        if len(_data["activity"]) > 20:
                            _data["activity"] = _data["activity"][-20:]

        except Exception as e:
            print(f"[bridge] Log follower error: {e}")
            time.sleep(5)

# Persistent transcript state
_transcript_buf = []       # accumulated events
_transcript_file = ""      # current session file being tracked
_transcript_pos = 0        # file read position

def _parse_transcript_line(line):
    """Parse one JSONL line into transcript events."""
    events = []
    try:
        d = json.loads(line)
    except:
        return events

    msg = d.get("message", {})
    role = msg.get("role", "")
    content = msg.get("content", "")
    ts = d.get("timestamp", "")
    short_ts = ts.split("T")[1][:5] if "T" in ts else ""

    if role == "user" and isinstance(content, str):
        lines = content.split("\n")
        capture = False
        for l in lines:
            if "GMT" in l and l.strip().startswith("["):
                capture = True
                continue
            if capture and l.strip() and not l.strip().startswith("```"):
                text = l.strip()
                if len(text) > 60:
                    text = text[:57] + "..."
                text = text.encode("ascii", errors="replace").decode("ascii").replace("?", "")
                if text:
                    events.append({"ts": short_ts, "type": "user", "text": text})
                break

    elif role == "assistant" and isinstance(content, list):
        for c in content:
            if not isinstance(c, dict):
                continue
            if c.get("type") == "toolCall":
                name = c.get("name", "")
                args = c.get("arguments", {})
                detail = args.get("command", args.get("query", args.get("path", args.get("url", ""))))
                if isinstance(detail, str):
                    if len(detail) > 40:
                        detail = detail[:37] + "..."
                    detail = detail.encode("ascii", errors="replace").decode("ascii").replace("?", "")
                text = f"{name}: {detail}" if detail else name
                events.append({"ts": short_ts, "type": "tool", "text": text})
            elif c.get("type") == "text":
                text = c.get("text", "")
                if text.startswith("[[reply"):
                    text = text.split("]] ", 1)[-1]
                text = text.split("\n")[0].strip()
                if text and len(text) > 5:
                    if len(text) > 60:
                        text = text[:57] + "..."
                    text = text.encode("ascii", errors="replace").decode("ascii").replace("?", "")
                    events.append({"ts": short_ts, "type": "reply", "text": text})

    return events

def refresh_transcript():
    """Incrementally read new lines from the active session JSONL. Accumulates in buffer."""
    global _transcript_buf, _transcript_file, _transcript_pos

    sess_index_path = os.path.join(OPENCLAW_DIR, "agents", "main", "sessions", "sessions.json")
    if not os.path.isfile(sess_index_path):
        return _transcript_buf[-20:]

    try:
        with open(sess_index_path) as f:
            index = json.load(f)
    except:
        return _transcript_buf[-20:]

    # Find most recent session
    best_key = None
    best_time = 0
    for key, info in index.items():
        t = info.get("updatedAt", 0)
        if t > best_time:
            best_time = t
            best_key = key

    if not best_key:
        return _transcript_buf[-20:]

    sess_file = index[best_key].get("sessionFile", "")
    if not os.path.isfile(sess_file):
        return _transcript_buf[-20:]

    # If session file changed, reset and do initial seed
    if sess_file != _transcript_file:
        _transcript_file = sess_file
        _transcript_buf = []
        _transcript_pos = 0
        # Seed with last 16KB
        try:
            with open(sess_file, "rb") as f:
                f.seek(0, 2)
                size = f.tell()
                f.seek(max(0, size - 16384))
                tail = f.read().decode("utf-8", errors="replace")
                _transcript_pos = size  # start tracking from end
            for line in tail.split("\n"):
                if line.strip():
                    _transcript_buf.extend(_parse_transcript_line(line))
        except:
            pass
        return _transcript_buf[-20:]

    # Incremental read — only new bytes since last position
    try:
        file_size = os.path.getsize(sess_file)
        if file_size <= _transcript_pos:
            return _transcript_buf[-20:]  # no new data

        with open(sess_file, "r") as f:
            f.seek(_transcript_pos)
            new_data = f.read()
            _transcript_pos = f.tell()

        for line in new_data.split("\n"):
            if line.strip():
                _transcript_buf.extend(_parse_transcript_line(line))

        # Keep buffer capped at 50
        if len(_transcript_buf) > 50:
            _transcript_buf = _transcript_buf[-50:]

    except:
        pass

    return _transcript_buf[-20:]

def background_refresh():
    while True:
        try:
            sessions = refresh_sessions()
            costs = refresh_costs()
            system = refresh_system()
            telemetry = refresh_telemetry()
            transcript = refresh_transcript()
            with _lock:
                _data["sessions"] = sessions
                _data["costs"] = costs
                _data["system"] = system
                _data["telemetry"] = telemetry
                _data["transcript"] = transcript
        except Exception as e:
            print(f"[bridge] Refresh error: {e}")
        time.sleep(REFRESH_INTERVAL)

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.rstrip("/")
        with _lock:
            if path in ("/health", "/api/health"):
                self.json_response({"ok": True, "status": "live"})
            elif path in ("/sessions", "/api/sessions"):
                self.json_response({"success": True, "data": _data["sessions"]})
            elif path in ("/costs", "/api/costs"):
                self.json_response({"success": True, "data": _data["costs"]})
            elif path in ("/system", "/api/system"):
                self.json_response({"success": True, "data": _data["system"]})
            elif path in ("/telemetry", "/api/telemetry"):
                self.json_response({"success": True, "data": _data["telemetry"]})
            elif path in ("/activity", "/api/activity"):
                self.json_response({"success": True, "data": _data["activity"][-8:]})
            elif path in ("/transcript", "/api/transcript"):
                self.json_response({"success": True, "data": _data["transcript"]})
            else:
                self.send_error(404)

    def do_POST(self):
        path = self.path.rstrip("/")
        if path in ("/v1/chat/completions", "/chat", "/api/chat"):
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode() if length else "{}"
            try:
                data = json.loads(body)
                msg = ""
                if "messages" in data:
                    for m in data["messages"]:
                        if m.get("role") == "user":
                            msg = m.get("content", "")
                elif "message" in data:
                    msg = data["message"]
                if not msg:
                    self.json_response({"error": "no message"})
                    return

                if msg == "/restart-gateway":
                    print("[bridge] Restarting gateway...")
                    result = run_cmd(["openclaw", "gateway", "restart"], timeout=30)
                    content = "Gateway restart: " + (result if result else "sent")
                else:
                    print(f"[bridge] Sending to agent: {msg}")
                    result = run_cmd(["openclaw", "agent", "--agent", "main", "--message", msg], timeout=60)
                    try:
                        resp = json.loads(result)
                        content = resp.get("reply", resp.get("content", result))
                    except:
                        content = result

                self.json_response({
                    "choices": [{"message": {"role": "assistant", "content": content}}]
                })
            except Exception as e:
                self.json_response({"error": str(e)})
        else:
            self.send_error(404)

    def json_response(self, data):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        pass

if __name__ == "__main__":
    print(f"ClawGlance Bridge listening on 0.0.0.0:{PORT}")
    print(f"  Refreshing every {REFRESH_INTERVAL}s in background")

    t = threading.Thread(target=background_refresh, daemon=True)
    t.start()
    t2 = threading.Thread(target=log_follower, daemon=True)
    t2.start()
    time.sleep(0.5)

    server = http.server.HTTPServer(("0.0.0.0", PORT), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
