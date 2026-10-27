# Autodesk Fusion MCP server connection

A Claude Code chat from **Oct 24–26, 2026**, exported from session `session_01GQbYa5MzvLXhbdwHHtsDcC`.
It ran in the CLI on Michael's Mac and was driven through Remote Control.

- **Full conversation:** [`transcript.md`](transcript.md)
- **CAD renders from the chat:** [`images/`](images/)
- **Scripts and notes Claude wrote, rebuilt in their final form:** [`files/`](files/)
- **The rising-knob panel it designed**, now regenerated in this repo: [`../../rising-knob-panel/`](../../rising-knob-panel/)

## What happened

| When (PDT) | Topic | Outcome |
|---|---|---|
| Sat Oct 24, 12:06 AM | Fusion extension log showed "Server transport closed unexpectedly" | The Claude extension is an `mcp-remote` bridge to `http://127.0.0.1:27182/mcp` inside Fusion. Fusion wasn't running and its **Preferences → General → API → Fusion MCP Server** setting was off. After Fusion was launched and the setting turned on, the port answered `405` (healthy). |
| Sat Oct 24, 12:17 AM | "Build me a benchy, but make it wild" | Built a 33-body **Wild Benchy** (hydrofoil rocket boat, ~99 × 77 × 57 mm) in Fusion by sending Python scripts to Fusion's MCP endpoint. |
| Sat Oct 24, 9:03 PM | Save it as "Wild Benchy" | Saving through the API hung. Claude saved it through Fusion's own Save dialog instead, to Default Project on the Team hub. |
| Sat Oct 24, 9:31 PM | Same log error again | Fusion's MCP server was stuck (empty replies). Quitting and relaunching Fusion fixed it. |
| Sun Oct 25, 9:29 PM | Rising panel for the VIEWE UEDX46460015-MD50ET knob display | Designed the **rising knob** module: an MG90S servo turns a crank and link, and the carriage slides on Ø4 mm rods in LM4UU bearings. It gives 12 mm of travel and eases in and out at both ends. Fusion's MCP was jammed again, so the model was built in CadQuery. A clash check at 13 positions through the stroke found nothing, and STLs, a STEP and renders were exported. |
| Mon Oct 26, 7:42 AM | "What's wrong with it now" | Root cause found: Fusion leaks a socket to PubNub about every 90 s. After roughly a day, all 1,024 low file descriptors are used, and Autodesk's cpp-httplib MCP server drops every new connection. Workaround: restart Fusion daily. Possible permanent fix (untested): exclude `*.pubnub.net` from the Cloudflare WARP tunnel. |
| Mon Oct 26, 7:47 AM | Is there a better way to connect to Fusion? | Compared third-party servers and recommended **[faust-machines/fusion360-mcp-server](https://github.com/faust-machines/fusion360-mcp-server)** (MIT). It runs commands on Fusion's main thread through custom events, has a 30 s timeout, uses plain sockets with no 1,024 limit, and offers 93 tools. |
| Mon Oct 26, 7:56 AM | Install it | Installed the add-in at commit `19b9f1e`, set it to run on Fusion startup (port 9876), and registered it in Claude Code as `fusion360`. Tested end to end: 93 tools listed, `ping` → `pong`, and `execute_code` saw the imported rising-knob model. |

## Notes on this export

- The transcript keeps every user message, every Claude reply and every tool call, with tool calls collapsed.
  Long tool outputs are cut at 2,500 characters. Claude's private reasoning is not included.
- Screenshots of the Mac desktop are left out of the repo, since it is public. Only the CAD renders are kept.
- The STL/STEP zip Claude sent in the chat isn't stored in the session record, so the design was regenerated from
  its source in [`rising-knob-panel/`](../../rising-knob-panel/).
- The `files/fusion/` scripts expect Fusion's built-in MCP endpoint on port 27182 (`fmcp.py` is the small client).
  `files/memory/fusion-mcp-quirks.md` is the memory note Claude saved about Fusion's quirks.
