"""Tiny client for Fusion's local MCP endpoint.

  fmcp.py script <file.py> [--ro]          run a Fusion API script
  fmcp.py shot <out.png> [direction] [w h]  save a viewport screenshot
  fmcp.py call <tool> '<json args>'         raw tool call
"""
import base64, json, sys, urllib.request

URL = "http://127.0.0.1:27182/mcp"
HDRS = {"Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
        "MCP-Protocol-Version": "2025-06-18"}


def post(body, sid=None, timeout=600):
    h = dict(HDRS)
    if sid:
        h["Mcp-Session-Id"] = sid
    req = urllib.request.Request(URL, json.dumps(body).encode(), h)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        sid = r.headers.get("MCP-Session-Id") or sid
        raw = r.read().decode("utf-8", "replace")
    if raw.lstrip().startswith("event:") or raw.lstrip().startswith("data:"):
        data = [l[5:].strip() for l in raw.splitlines() if l.startswith("data:")]
        raw = data[-1] if data else ""
    return (json.loads(raw) if raw.strip() else None), sid


def call(tool, args):
    _, sid = post({"jsonrpc": "2.0", "id": 0, "method": "initialize",
                   "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                              "clientInfo": {"name": "claude-code", "version": "0"}}})
    post({"jsonrpc": "2.0", "method": "notifications/initialized"}, sid)
    res, _ = post({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                   "params": {"name": tool, "arguments": args}}, sid)
    return res


def texts(res):
    if "error" in res:
        return [json.dumps(res["error"])]
    out = []
    for c in res["result"].get("content", []):
        if c.get("type") == "text":
            out.append(c["text"])
        else:
            out.append(c)
    return out


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "script":
        obj = {"script": open(sys.argv[2]).read()}
        if "--ro" in sys.argv:
            obj["readOnly"] = True
        res = call("fusion_mcp_execute", {"featureType": "script", "object": obj})
        if res.get("result", {}).get("isError"):
            print("ISERROR")
        for t in texts(res):
            print(t)
    elif cmd == "shot":
        args = {"queryType": "screenshot", "transparentBackground": False}
        if len(sys.argv) > 3:
            args["direction"] = sys.argv[3]
        if len(sys.argv) > 5:
            args["width"], args["height"] = int(sys.argv[4]), int(sys.argv[5])
        res = call("fusion_mcp_read", args)
        done = False
        for c in res.get("result", {}).get("content", []):
            b64 = c.get("data") or c.get("base64Data")
            if not b64 and c.get("type") == "text":
                try:
                    j = json.loads(c["text"])
                    b64 = j.get("base64Data") or j.get("data")
                except Exception:
                    pass
            if b64:
                open(sys.argv[2], "wb").write(base64.b64decode(b64))
                done = True
                print("saved", sys.argv[2])
                break
        if not done:
            print(json.dumps(res)[:2000])
    elif cmd == "call":
        res = call(sys.argv[2], json.loads(sys.argv[3]))
        for t in texts(res):
            print(t if isinstance(t, str) else json.dumps(t)[:2000])
