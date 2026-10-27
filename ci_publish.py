#!/usr/bin/env python3
"""
把指定目录通过 GitHub Git Data API 发布到指定分支。
用法: ci_publish.py <branch> <dir>
不依赖 git push（容器内 push 不稳定），直接操作 blob/tree/commit/ref。
"""
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.request

TOKEN = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
REPO = os.environ["GITHUB_REPOSITORY"]   # owner/name
API = "https://api.github.com"


def req(method, path, body=None, retries=4):
    data = json.dumps(body).encode() if body is not None else None
    headers = {
        "Authorization": "Bearer " + TOKEN,
        "Accept": "application/vnd.github+json",
        "User-Agent": "ci-publish",
    }
    if data:
        headers["Content-Type"] = "application/json"
    last = None
    for i in range(retries):
        try:
            r = urllib.request.Request(API + path, data=data, headers=headers, method=method)
            with urllib.request.urlopen(r, timeout=120) as resp:
                raw = resp.read()
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as e:
            last = f"HTTP {e.code}: {e.read().decode(errors='replace')[:300]}"
            if e.code in (500, 502, 503, 504):
                time.sleep(2 + i * 2)
                continue
            raise SystemExit(f"{method} {path} -> {last}")
        except Exception as e:
            last = str(e)
            time.sleep(2 + i * 2)
    raise SystemExit(f"{method} {path} failed: {last}")


def main():
    if len(sys.argv) < 3:
        raise SystemExit("usage: ci_publish.py <branch> <dir>")
    branch, d = sys.argv[1], sys.argv[2]
    if not os.path.isdir(d):
        raise SystemExit(f"dir not found: {d}")

    files = []
    for root, _, fns in os.walk(d):
        for fn in fns:
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, d)
            files.append((rel, full))
    files.sort()
    print(f"[ci_publish] branch={branch} dir={d} files={len(files)}")

    tree = []
    for rel, full in files:
        size = os.path.getsize(full)
        with open(full, "rb") as f:
            b64 = base64.b64encode(f.read()).decode()
        blob = req("POST", f"/repos/{REPO}/git/blobs",
                   {"content": b64, "encoding": "base64"})
        tree.append({"path": rel.replace(os.sep, "/"),
                     "mode": "100644", "type": "blob", "sha": blob["sha"]})
        print(f"  blob {rel} ({size} B) -> {blob['sha'][:8]}")

    tree_obj = req("POST", f"/repos/{REPO}/git/trees", {"tree": tree})
    print("  tree ->", tree_obj["sha"][:8])

    # 父提交（分支已存在时）
    parent = None
    try:
        ref = req("GET", f"/repos/{REPO}/git/ref/heads/{branch}")
        parent = ref["object"]["sha"]
        print("  existing ref ->", parent[:8])
    except SystemExit:
        print("  branch does not exist yet -> creating root commit")

    body = {"message": f"ci publish {branch}", "tree": tree_obj["sha"]}
    if parent:
        body["parents"] = [parent]
    commit = req("POST", f"/repos/{REPO}/git/commits", body)
    print("  commit ->", commit["sha"][:8])

    if parent:
        req("PATCH", f"/repos/{REPO}/git/refs/heads/{branch}",
            {"sha": commit["sha"], "force": True})
        print("  ref updated (force)")
    else:
        req("POST", f"/repos/{REPO}/git/refs",
            {"ref": f"refs/heads/{branch}", "sha": commit["sha"]})
        print("  ref created")

    print(f"[ci_publish] OK -> https://github.com/{REPO}/tree/{branch}")


if __name__ == "__main__":
    main()
