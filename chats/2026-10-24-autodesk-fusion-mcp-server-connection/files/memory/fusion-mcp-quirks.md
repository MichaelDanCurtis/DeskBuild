---
name: fusion-mcp-quirks
description: Autodesk Fusion MCP (port 27182) wedges easily; how it was driven and the CadQuery fallback that worked
metadata:
  type: reference
---

Fusion's local MCP server (http://127.0.0.1:27182/mcp, Claude extension `ant.dir.gh.autodesk.fusion-mcp` = mcp-remote bridge) was used Oct 24–25 2026 for a "Wild Benchy" and a rising-knob panel design.

- Bridge exits instantly (log: "transport closed unexpectedly") whenever Fusion isn't listening (ECONNREFUSED) or its MCP server is stuck (empty reply / ECONNRESET).
- ROOT CAUSE of the "stuck" state (diagnosed Oct 26 2026): Fusion leaks a TCP socket roughly every 90 s to PubNub (204.57.240.0/21, *.pubnub.net), routed through Cloudflare WARP (utun0, 172.16.0.2); they pile up in CLOSED state. Once all fds below 1024 are used (~a day of uptime), its MCP server (NsMCP10.dylib, cpp-httplib using select()) drops every new connection unanswered. Check: `lsof -nP -p <fusion pid> | wc -l` (>1100 = broken). Fix: restart Fusion. Untested prevention: exclude PubNub from WARP split tunnel.
- INSTALLED Oct 26 2026: faust-machines/fusion360-mcp-server at commit 19b9f1e (MIT, beta). Add-in in `~/Library/Application Support/Autodesk/Autodesk Fusion 360/API/AddIns/Fusion360MCP` with runOnStartup=true, listening on TCP 9876 with plain Python sockets (no 1024 limit). Server: `uv tool install git+…@19b9f1e` → `~/.local/bin/fusion360-mcp-server --mode socket`, registered user-scope as `fusion360` in ~/.claude.json. It has 93 tools incl. execute_code and ping; log at ~/fusion360mcp.log. Prefer it over Autodesk's 27182 server. Gotcha: an add-in started from a one-off script loses its CustomEvent handler unless `adsk.autoTerminate(False)` is called.
- Fusion build 2705.x: Part Design docs allow only one component (build bodies in root); root can't be renamed; default design is Y-up.
- Saving via API hung; saving via Fusion's Save dialog (Cmd+S + typed name) worked.
- Reliable fallback: CadQuery 2.8 in a uv Python 3.12 venv (`uv venv --python 3.12`), OCC booleans for interference checks, VTK offscreen renders, STEP for import into Fusion.

Related: [[translate-planning-status]] (unrelated main project in this directory).
