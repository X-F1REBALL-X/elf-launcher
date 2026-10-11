#!/usr/bin/env python3
"""Refresh payloads.json items that have github_latest from GitHub releases/latest.

For every item with github_latest {repo, asset}, ask GitHub for the latest
(non-prerelease) release, pick the asset matching the regex and, when its sha256
digest differs from the catalog, update url, sha256, checksum, size and version.
Same rules as the app's applyRelease(), done once a day server-side so the app
barely needs the GitHub API.

Default is a dry run. --apply writes payloads.json.
Token: $GH_TOKEN or $GITHUB_TOKEN, else `gh auth token`, else anonymous.
Never touches the elf-launcher.elf entry (that one ships with the app release).
"""
import argparse, json, os, re, subprocess, sys, urllib.error, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CATALOG = os.path.join(HERE, "..", "payloads.json")
NEVER = {"elf-launcher.elf"}


def token():
    t = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if t:
        return t
    try:
        return subprocess.run(["gh", "auth", "token"], capture_output=True, text=True, check=True).stdout.strip()
    except Exception:
        return ""


def latest(repo, tok):
    req = urllib.request.Request("https://api.github.com/repos/%s/releases/latest" % repo,
                                 headers={"Accept": "application/vnd.github+json", "User-Agent": "elf-launcher-catalog-sync"})
    if tok:
        req.add_header("Authorization", "Bearer " + tok)
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--apply", action="store_true", help="write payloads.json")
    g.add_argument("--dry-run", action="store_true", help="only print (default)")
    ap.add_argument("--names-out", help="write changed item names here, one per line")
    args = ap.parse_args()

    cat = json.load(open(CATALOG, encoding="utf-8"))
    repos = {}
    for p in cat["payloads"]:
        gl = p.get("github_latest")
        if p["name"] in NEVER or not gl or not gl.get("repo") or not gl.get("asset"):
            continue
        repos.setdefault(gl["repo"], []).append(p)

    tok = token()
    changed, problems, ok = [], [], 0
    print("mode: %s, %d items in %d repos" % ("APPLY" if args.apply else "dry run",
                                               sum(len(v) for v in repos.values()), len(repos)))
    for repo in sorted(repos):
        try:
            rel = latest(repo, tok)
        except urllib.error.HTTPError as e:
            problems.append("%s: HTTP %d" % (repo, e.code))
            continue
        except Exception as e:
            problems.append("%s: %s" % (repo, e))
            continue
        tag = rel.get("tag_name") or ""
        for p in repos[repo]:
            rx = re.compile(p["github_latest"]["asset"])
            hits = [a for a in rel.get("assets", []) if rx.search(a.get("name") or "")]
            if len(hits) != 1:
                problems.append("%s: regex matched %d assets in %s %s" % (p["name"], len(hits), repo, tag))
                continue
            a = hits[0]
            sha = (a.get("digest") or "").replace("sha256:", "").lower()
            if len(sha) != 64:
                problems.append("%s: %s has no sha256 digest (%s)" % (p["name"], a["name"], tag))
                continue
            cur = str(p.get("checksum") or p.get("sha256") or "").lower()
            if sha == cur:
                ok += 1
                continue
            ver = tag
            if not str(p.get("version", "")).startswith("v"):
                ver = re.sub(r"^v(?=\d)", "", tag)  # keep the entry's own style
            print("  %-36s %s -> %s  size %s -> %s  sha %s -> %s" % (
                p["name"], p.get("version"), ver, p.get("size"), a["size"], cur[:12], sha[:12]))
            if p.get("source_direct") == p.get("url"):
                p["source_direct"] = a["browser_download_url"]
            p["url"] = a["browser_download_url"]
            p["sha256"] = sha
            p["checksum"] = sha
            p["size"] = a["size"]
            p["version"] = ver
            changed.append(p["name"])
    print("%d up to date, %d %s" % (ok, len(changed), "updated" if args.apply else "would change"))
    warn = "::warning::" if os.environ.get("GITHUB_ACTIONS") else "  problem: "
    for x in problems:
        print(warn + x)
    if args.apply and changed:
        with open(CATALOG, "w", encoding="utf-8") as f:
            f.write(json.dumps(cat, indent=2, ensure_ascii=False) + "\n")
    if args.names_out:
        with open(args.names_out, "w") as f:
            f.write("".join(n + "\n" for n in changed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
