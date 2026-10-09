"""Render PLAN.md into site/index.html (git-ignored). Re-run after editing PLAN.md or after a session.

The Markdown is embedded in the page and rendered in the browser with marked (cdnjs), so the page
opens straight from disk (file://) with no server. A `## User to-do` section in PLAN.md is shown first.

Token usage comes from the Claude Code transcripts of this project
(~/.claude/projects/<repo path with :\\/ -> ->/*.jsonl, subagent transcripts included). Each session is
assigned to a subtask by an `S<n>.<m>` / `S<n>F.<m>` id in its first prompt, or by its first prompt matching the
subtask's text in PLAN.md.
"""
import collections
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
md = (ROOT / "PLAN.md").read_text(encoding="utf-8")
out = ROOT / "site" / "index.html"
out.parent.mkdir(exist_ok=True)

TRANSCRIPTS = pathlib.Path.home() / ".claude" / "projects" / re.sub(r"[:\\/ ]", "-", str(ROOT))
MODEL_TAG = {"haiku": "H", "sonnet": "S", "opus": "O"}
FIELDS = ("input_tokens", "cache_creation_input_tokens", "cache_read_input_tokens", "output_tokens")


def norm(s):
    return re.sub(r"[^a-z0-9]+", " ", s.lower()).strip()


TITLE = re.sub(r"\s+—.*", "", (re.search(r"^# (.+)$", md, re.M) or [None, "Port"])[1]) + " — Plan"
SUBTASKS = {m[1]: m[2].strip() for m in re.finditer(r"^\|\s*(S\d+[A-Z]?\.\d+)\s*\|\s*([^|]+)\|", md, re.M)}


# session id prefix -> subtask, for sessions from before PLAN.md (their prompts name no subtask)
OVERRIDES = {"89df5c6f": "S0.1"}


def subtask_of(prompt, sid=""):
    if sid[:8] in OVERRIDES:
        return OVERRIDES[sid[:8]]
    # also typed by hand: "s3.1", "s3f.1", "S3f.4", "s3f3"
    for m in re.finditer(r"\bs(\d+)(?:(f)\.?|\.)(\d+)\b", prompt[:200], re.I):
        sub = f"S{m[1]}{'F' if m[2] else ''}.{m[3]}"
        if sub in SUBTASKS:
            return sub
    p = norm(prompt)[:60]
    for sid, text in SUBTASKS.items():
        if p and (p in norm(text) or norm(text)[:60] in p):
            return sid
    return ""


def first_prompt(path):
    for line in path.open(encoding="utf-8"):
        o = json.loads(line)
        c = o.get("message", {}).get("content") if o.get("type") == "user" else None
        if isinstance(c, list):
            c = " ".join(b.get("text", "") for b in c if isinstance(b, dict) and b.get("type") == "text")
        if c and not c.lstrip().startswith("<"):
            return c
    return ""


def usage():
    sessions = []
    for main in sorted(TRANSCRIPTS.glob("*.jsonl")):
        sid = main.stem
        # custom-title.json is not reliable (the same title shows up on several sessions): first prompt wins
        title_file = TRANSCRIPTS / sid / "custom-title.json"
        title = json.loads(title_file.read_text(encoding="utf-8")).get("customTitle", "") if title_file.exists() else ""
        prompt = first_prompt(main)
        msgs = {}  # message id -> (model, usage, timestamp); a message is logged once per content block
        for f in [main, *(TRANSCRIPTS / sid).rglob("*.jsonl")]:
            for line in f.open(encoding="utf-8"):
                o = json.loads(line)
                m = o.get("message") if o.get("type") == "assistant" else None
                if m and m.get("usage") and m.get("model", "").startswith("claude"):
                    msgs[m.get("id") or o.get("uuid")] = (m["model"], m["usage"], o.get("timestamp", ""))
        if not msgs:
            continue
        by_model = collections.defaultdict(lambda: dict.fromkeys(FIELDS, 0) | {"requests": 0})
        for model, u, _ in msgs.values():
            tag = next((t for k, t in MODEL_TAG.items() if k in model), model)
            row = by_model[tag]
            row["requests"] += 1
            for k in FIELDS:
                row[k] += u.get(k) or 0
        stamps = sorted(t for _, _, t in msgs.values() if t)
        sessions.append({"id": sid[:8], "subtask": subtask_of(prompt, sid), "title": prompt[:70] or title,
                         "start": stamps[0][:16].replace("T", " ") if stamps else "", "models": by_model})
    return sessions

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<style>
:root { --bg:#f7f7f5; --fg:#1d1d1b; --muted:#6b6b66; --card:#fff; --line:#deded8; --accent:#2b5fab;
        --code:#efefea; --h:#3d7a3d; --s:#2b5fab; --o:#8a4bb0; --done:#3d7a3d; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg:#141413; --fg:#e8e8e3; --muted:#9a9a93; --card:#1d1d1b; --line:#33332f; --accent:#7aa7e8;
  --code:#262624; --h:#7cc07c; --s:#7aa7e8; --o:#c49ae0; --done:#7cc07c; } }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--fg);
       font:15px/1.55 system-ui, -apple-system, "Segoe UI", sans-serif; }
.wrap { display:grid; grid-template-columns:240px minmax(0,1fr); gap:32px; max-width:1240px;
        margin:0 auto; padding:24px 16px 64px; }
nav { position:sticky; top:16px; align-self:start; max-height:calc(100vh - 32px); overflow:auto;
      font-size:13px; }
nav a { display:block; color:var(--muted); text-decoration:none; padding:3px 0; }
nav a.h3 { padding-left:12px; }
nav a:hover { color:var(--accent); }
main { min-width:0; }
h1 { font-size:26px; margin:0 0 12px; }
h2 { font-size:20px; margin:36px 0 10px; padding-bottom:6px; border-bottom:1px solid var(--line); }
h3 { font-size:16px; margin:24px 0 8px; }
a { color:var(--accent); }
code { background:var(--code); padding:1px 5px; border-radius:4px; font-size:13px; }
pre { background:var(--code); padding:12px 14px; border-radius:8px; overflow:auto; }
pre code { padding:0; background:none; }
.tbl { overflow-x:auto; margin:10px 0 16px; }
table { border-collapse:collapse; width:100%; background:var(--card); font-size:14px; }
th, td { border:1px solid var(--line); padding:6px 9px; text-align:left; vertical-align:top; }
th { background:var(--code); font-weight:600; }
.b { display:inline-block; min-width:22px; text-align:center; padding:0 6px; border-radius:10px;
     font-weight:700; font-size:12px; color:#fff; }
.b.H { background:var(--h); } .b.S { background:var(--s); } .b.O { background:var(--o); }
td.done { color:var(--done); font-weight:600; }
td.dropped { color:var(--muted); text-decoration:line-through; }
.meta { color:var(--muted); font-size:12px; margin-bottom:20px; }
.cards { display:grid; grid-template-columns:repeat(auto-fit, minmax(150px, 1fr)); gap:10px; margin:12px 0 18px; }
.card { background:var(--card); border:1px solid var(--line); border-radius:8px; padding:10px 12px; }
.card .v { font-size:20px; font-weight:700; font-variant-numeric:tabular-nums; }
.card .k { color:var(--muted); font-size:12px; }
td.n, th.n { text-align:right; font-variant-numeric:tabular-nums; white-space:nowrap; }
.note { color:var(--muted); font-size:13px; }
@media (max-width: 800px) { .wrap { grid-template-columns:1fr; } nav { position:static; max-height:none; } }
</style>
</head>
<body>
<div class="wrap">
<nav id="toc"></nav>
<main>
<div class="meta">Generated from PLAN.md by tools/plan_site.py</div>
<div id="doc"></div>
<section id="usage"></section>
</main>
</div>
<script type="text/markdown" id="src">__MD__</script>
<script type="application/json" id="usage-data">__USAGE__</script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/marked/12.0.2/marked.min.js"></script>
<script>
const src = document.getElementById('src').textContent;
const doc = document.getElementById('doc');
doc.innerHTML = window.marked ? marked.parse(src) : '<pre>' + src.replace(/[<&]/g, c => c === '<' ? '&lt;' : '&amp;') + '</pre>';
doc.querySelectorAll('table').forEach(t => {
  const w = document.createElement('div'); w.className = 'tbl'; t.replaceWith(w); w.appendChild(t);
});
doc.querySelectorAll('td').forEach(td => {
  const t = td.textContent.trim();
  if (/^[HSO]$/.test(t)) td.innerHTML = '<span class="b ' + t + '">' + t + '</span>';
  else if (/^done/.test(t)) td.classList.add('done');
  else if (/^(dropped|moved)/.test(t)) td.classList.add('dropped');
});
// "User to-do" is the first table: move that section under the title, and add a row for every
// subtask whose status cell says "U<n> pending" but which the to-do section does not mention yet.
const todoH = [...doc.querySelectorAll('h2')].find(h => /^user to-do/i.test(h.textContent.trim()));
if (todoH) {
  const part = [todoH];
  for (let n = todoH.nextElementSibling; n && n.tagName !== 'H2'; n = n.nextElementSibling) part.push(n);
  const todo = part.map(e => e.tagName === 'TABLE' ? e : e.querySelector('table')).find(Boolean);
  const listed = part.map(e => e.textContent).join(' ');
  if (todo) doc.querySelectorAll('tr').forEach(tr => {
    if (todo.contains(tr) || tr.cells.length < 3) return;
    const id = tr.cells[0].textContent.trim(), m = tr.cells[tr.cells.length - 1].textContent.match(/\\b(U\\d+) pending/);
    if (!m || !/^S\\d+[A-Z]?\\.\\d+$/.test(id) || listed.includes(id)) return;
    const row = todo.insertRow(-1), cols = todo.rows[0].cells.length;
    const text = tr.cells[1].textContent.trim();
    [m[1], 'Check the result of ' + id + ': ' + (text.length > 90 ? text.slice(0, 90) + '…' : text) + ' (auto-added)', id]
      .concat(Array(Math.max(0, cols - 3)).fill('')).slice(0, cols).forEach(v => row.insertCell().textContent = v);
  });
  const h1 = doc.querySelector('h1');
  h1 ? h1.after(...part) : doc.prepend(...part);
}

// Token usage
const sessions = JSON.parse(document.getElementById('usage-data').textContent);
const F = ['input_tokens', 'cache_creation_input_tokens', 'cache_read_input_tokens', 'output_tokens'];
const fmt = n => n >= 1e6 ? (n / 1e6).toFixed(2) + ' M' : n >= 1e3 ? (n / 1e3).toFixed(1) + ' k' : String(n);
const esc = s => String(s).replace(/[<&"]/g, c => ({'<': '&lt;', '&': '&amp;', '"': '&quot;'})[c]);
const badge = t => /^[HSO]$/.test(t) ? '<span class="b ' + t + '">' + t + '</span>' : esc(t);
const zero = () => Object.fromEntries(F.concat('requests').map(k => [k, 0]));
const add = (a, b) => { for (const k in a) a[k] += b[k] || 0; return a; };
const total = zero(), perModel = {}, perTask = {};
for (const s of sessions) {
  const key = s.subtask || 'session ' + s.id;
  const t = perTask[key] ??= {subtask: s.subtask, title: s.subtask ? '' : s.title, sessions: 0, models: new Set(), u: zero(), start: s.start};
  t.sessions++;
  for (const [m, u] of Object.entries(s.models)) {
    add(total, u); add(perModel[m] ??= zero(), u); add(t.u, u); t.models.add(m);
  }
}
const nums = u => F.map(k => '<td class="n">' + fmt(u[k]) + '</td>').join('');
const head = first => '<tr>' + first + '<th class="n">Input</th><th class="n">Cache write</th><th class="n">Cache read</th><th class="n">Output</th></tr>';
const taskRows = Object.values(perTask).sort((a, b) => (a.subtask || '~' + a.start).localeCompare(b.subtask || '~' + b.start, undefined, {numeric: true}))
  .map(t => '<tr><td>' + esc(t.subtask || '—') + '</td><td>' + esc(t.title) + '</td><td class="n">' + t.sessions + '</td><td>' +
    [...t.models].map(badge).join(' ') + '</td>' + nums(t.u) + '</tr>').join('');
const modelRows = Object.entries(perModel).map(([m, u]) => '<tr><td>' + badge(m) + '</td><td class="n">' + u.requests + '</td>' + nums(u) + '</tr>').join('');
const sessionRows = sessions.slice().sort((a, b) => a.start.localeCompare(b.start)).map(s => {
  const u = Object.values(s.models).reduce(add, zero());
  return '<tr><td>' + esc(s.start) + '</td><td>' + esc(s.subtask || '—') + '</td><td>' + esc(s.title) + '</td><td>' +
    Object.keys(s.models).map(badge).join(' ') + '</td>' + nums(u) + '</tr>';
}).join('');
const card = (v, k) => '<div class="card"><div class="v">' + v + '</div><div class="k">' + k + '</div></div>';
document.getElementById('usage').innerHTML = sessions.length ? (
  '<h2>Token usage</h2><div class="cards">' + card(fmt(total.input_tokens), 'Input (uncached)') +
  card(fmt(total.cache_creation_input_tokens), 'Cache write') + card(fmt(total.cache_read_input_tokens), 'Cache read') +
  card(fmt(total.output_tokens), 'Output') + card(total.requests, 'Requests') + card(sessions.length, 'Sessions') + '</div>' +
  '<p class="note">From the Claude Code transcripts of this project, subagents included. Sessions are matched to a subtask by an S-id or the subtask text in their first prompt. Totals include the session that is still running only up to the last page rebuild.</p>' +
  '<h3>By subtask</h3><div class="tbl"><table>' + head('<th>Subtask</th><th>Session</th><th class="n">Sessions</th><th>Models</th>') + taskRows + '</table></div>' +
  '<h3>By model</h3><div class="tbl"><table>' + head('<th>Model</th><th class="n">Requests</th>') + modelRows + '</table></div>' +
  '<h3>Sessions</h3><div class="tbl"><table>' + head('<th>Start (UTC)</th><th>Subtask</th><th>Title</th><th>Models</th>') + sessionRows + '</table></div>'
) : '';

const toc = document.getElementById('toc');
document.querySelectorAll('main h2, main h3').forEach((h, i) => {
  h.id = 'h' + i;
  const a = document.createElement('a');
  a.href = '#' + h.id; a.textContent = h.textContent; a.className = h.tagName.toLowerCase();
  toc.appendChild(a);
});
</script>
</body>
</html>
"""

# <script> content is raw text: only a closing tag inside it needs breaking up.
data = json.dumps(usage()).replace("</", "<\\/")
out.write_text(PAGE.replace("__TITLE__", TITLE).replace("__MD__", md.replace("</script", "<\\/script")).replace("__USAGE__", data),
               encoding="utf-8")
print(f"wrote {out.relative_to(ROOT)}")
