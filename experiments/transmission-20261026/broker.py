#!/usr/bin/env python3
"""File bridge, not an organism. Single hardcoded HTTPS origin, no redirects.

Runs outside the networkless C engine. It cannot read the hidden document,
checkpoint, or mailbox: only bridge/ is mounted in this container.
"""
import argparse
import hashlib
import html.parser
import json
import os
from pathlib import Path
import re
import ssl
import time
import urllib.error
import urllib.request

MAX_RAW = 2 * 1024 * 1024
MAX_TEXT = 65536
CACHE_LIMIT = 8 * 1024 * 1024
REQUESTS_PER_WINDOW = 64
WINDOW = 3 * 3600
SLUG = re.compile(r"[A-Za-z0-9_()\-]{1,96}\Z")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError("redirect forbidden")


class Text(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.hidden = 0
        self.parts = []

    def handle_starttag(self, tag, attrs):
        if tag in ("script", "style", "svg"):
            self.hidden += 1
        if tag in ("p", "h1", "h2", "h3", "li", "br", "div") and not self.hidden:
            self.parts.append("\n")

    def handle_endtag(self, tag):
        if tag in ("script", "style", "svg") and self.hidden:
            self.hidden -= 1

    def handle_data(self, data):
        if not self.hidden:
            self.parts.append(data)


def atomic(path, data):
    temp = path.with_suffix(path.suffix + ".tmp")
    with temp.open("wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)
    fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
    os.fsync(fd)
    os.close(fd)


def get_article(slug):
    if not SLUG.fullmatch(slug):
        raise ValueError("invalid slug; URLs, slash, percent and queries forbidden")
    url = "https://grokipedia.com/page/" + slug
    opener = urllib.request.build_opener(
        urllib.request.ProxyHandler({}), NoRedirect(),
        urllib.request.HTTPSHandler(context=ssl.create_default_context()))
    request = urllib.request.Request(url, headers={"User-Agent": "Octet-Transmission/1 (bounded research library)"})
    with opener.open(request, timeout=20) as response:
        if response.url != url or response.status != 200:
            raise ValueError("origin/status mismatch")
        if "text/html" not in response.headers.get("Content-Type", ""):
            raise ValueError("non HTML response")
        raw = response.read(MAX_RAW + 1)
    if len(raw) > MAX_RAW:
        raise ValueError("HTML exceeds 2 MiB")
    parser = Text()
    parser.feed(raw.decode("utf-8", errors="replace"))
    text = re.sub(r"[ \t]+", " ", "".join(parser.parts))
    text = re.sub(r"\n[ \t\n]+", "\n", text).strip()
    body = text.encode("utf-8")[:MAX_TEXT]
    if not body:
        raise ValueError("empty article")
    return body, {"url": url, "raw_sha256": hashlib.sha256(raw).hexdigest(),
                  "raw_bytes": len(raw), "bytes": len(body),
                  "sha256": hashlib.sha256(body).hexdigest(), "truncated": len(text.encode("utf-8")) > MAX_TEXT}


def process(root):
    state_path = root / "broker-state.json"
    state = json.loads(state_path.read_text()) if state_path.exists() else {"window": 0, "attempts": 0, "seen": {}, "accepted": 0, "rejected": 0}
    now = int(time.time())
    if now - state["window"] >= WINDOW:
        state["window"], state["attempts"] = now, 0
    cache = root / "cache"
    cache.mkdir(exist_ok=True)
    changed = False
    for slot in range(8):
        req = root / f"{slot}.req"
        if not req.exists():
            continue
        payload = req.read_bytes()[:512]
        key = hashlib.sha256(payload).hexdigest()
        if state["seen"].get(str(slot)) == key:
            continue
        # Don't consume a request when the network budget is exhausted.
        if state["attempts"] >= REQUESTS_PER_WINDOW:
            continue
        state["seen"][str(slot)] = key
        state["attempts"] += 1
        changed = True
        result = {"time": now, "slot": slot, "request_sha256": key}
        try:
            tick, organism, seq, slug = payload.decode("ascii").rstrip("\n").split("\t")
            if not all(x.isdecimal() for x in (tick, organism, seq)) or not SLUG.fullmatch(slug):
                raise ValueError("invalid request")
            result.update(tick=int(tick), organism=int(organism), sequence=int(seq), slug=slug)
            cache_key = hashlib.sha256(slug.encode()).hexdigest()
            saved = cache / (cache_key + ".book")
            info = cache / (cache_key + ".json")
            if saved.exists() and info.exists():
                body = saved.read_bytes()
                provenance = json.loads(info.read_text())
                result["cache_hit"] = True
            else:
                usage = sum(p.stat().st_size for p in cache.iterdir() if p.is_file())
                if usage + MAX_TEXT + 2048 > CACHE_LIMIT:
                    raise ValueError("library cache full; no eviction")
                body, provenance = get_article(slug)
                provenance["retrieved_at"] = int(time.time())
                atomic(saved, body)
                atomic(info, json.dumps(provenance, ensure_ascii=False).encode())
                result["cache_hit"] = False
            if len(body) > MAX_TEXT or hashlib.sha256(body).hexdigest() != provenance["sha256"]:
                raise ValueError("cache corrupt")
            atomic(root / f"{slot}.book", body)
            result.update(provenance, status="ok")
            state["accepted"] += 1
        except (OSError, ValueError, urllib.error.URLError) as error:
            result.update(status="rejected", error=str(error)[:300])
            state["rejected"] += 1
        atomic(root / f"{slot}.result.json", json.dumps(result, ensure_ascii=False).encode())
        print(json.dumps(result, ensure_ascii=False), flush=True)
    if changed:
        atomic(state_path, json.dumps(state).encode())
    return state


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bridge", type=Path)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    while True:
        process(args.bridge)
        if args.once:
            return
        time.sleep(2)


if __name__ == "__main__":
    main()
