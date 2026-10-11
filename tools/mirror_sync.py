#!/usr/bin/env python3
"""Keep payload-mirror and non-GitHub catalog items in sync with upstream.

Some payloads can't use the app's github_latest auto-update: upstream ships
them only inside a ZIP, or hosts them off GitHub (Gitea/Forgejo, no sha256
digests). For each one this script finds the newest upstream release,
downloads the asset (pulls the ELF out of the ZIP if needed) and compares
its sha256 with payloads.json.

Default is a dry run (--dry-run): it only prints what would change.
  --apply   upload changed files to the payload-mirror release
            (gh release upload --clobber) and update payloads.json
            url/size/checksum/sha256/version.

Needs: gh (logged in), python3.
"""
import argparse, hashlib, io, json, os, re, subprocess, sys, tempfile, urllib.request, zipfile

OWNER_REPO = "X-F1REBALL-X/elf-launcher"
MIRROR_TAG = "payload-mirror"
MIRROR_URL = "https://github.com/%s/releases/download/%s/" % (OWNER_REPO, MIRROR_TAG)
HERE = os.path.dirname(os.path.abspath(__file__))
CATALOG = os.path.join(HERE, "..", "payloads.json")

# catalog name -> upstream
#   host:   "github" or a Gitea/Forgejo base URL
#   repo:   owner/name
#   asset:  regex for the release asset
#   member: regex for the ELF inside the asset when it is a ZIP
#   skip:   reason; the item is left alone
UPSTREAM = {
    # skip: listed for reference but never synced (the reason is printed).
    "elfldr-ps5.elf":          {"host": "github", "repo": "ps5-payload-dev/elfldr",   "asset": r"^elfldr-ps5\.elf$",
                                "skip": "pinned: mirror copy = loader/elfldr-ps5.elf (448952 B, ?uri= fallback), "
                                        "not byte-identical to upstream v0.26 asset (394480 B)"},
    "fetchpkg.elf":            {"host": "github", "repo": "ps5-payload-dev/fetchpkg", "asset": r"^PS5\.zip$", "member": r"(^|/)fetchpkg\.elf$"},
    "pkg_install.elf":         {"host": "github", "repo": "ps5-payload-dev/websrv",   "asset": r"^PKGInstall\.zip$", "member": r"(^|/)pkg_install\.elf$"},
    "dump_installer.elf":      {"host": "github", "repo": "EchoStretch/dump_installer", "asset": r"^dump_installer\.zip$", "member": r"(^|/)dump_installer\.elf$"},
    "dump_runner.elf":         {"host": "github", "repo": "EchoStretch/dump_runner",  "asset": r"^dump_runner\.zip$", "member": r"(^|/)dump_runner\.elf$"},
    "mounter.elf":             {"host": "github", "repo": "n0llptr/Playstation-5-Save-Mounter", "asset": r"^save-mounter\.zip$", "member": r"(^|/)mounter\.elf$"},
    "actremotelink_agent.elf": {"host": "github", "repo": "francoataffarel/ActRemoteLink", "asset": r"^ActRemoteLink\.zip$", "member": r"(^|/)actremotelink_agent\.elf$"},
    "LinkDev.elf":             {"host": "github", "repo": "ps5-payload-dev/linkdev",  "asset": r"^LinkDev\.zip$", "member": r"(^|/)LinkDev\.elf$"},
    "OffAct.elf":              {"host": "github", "repo": "ps5-payload-dev/offact",   "asset": r"^OffAct\.zip$", "member": r"(^|/)OffAct\.elf$"},
    # off GitHub (no digests, so the app can't auto-update these itself)
    "garlic-savemgr-v1.13.1.elf": {"host": "https://git.etawen.dev", "repo": "earthonion/garlic-savemgr", "asset": r"^garlic-savemgr\.elf$"},
    "garlic-worker-ps5.elf":   {"host": "https://git.etawen.dev", "repo": "earthonion/garlic-worker", "asset": r"^garlic-worker-ps5\.elf$"},
    "elfldr-autoldr.elf":      {"host": "https://git.etawen.dev", "repo": "earthonion/elfldr-autoldr", "asset": r"^elfldr-ps5\.elf$"},
    "AnyPad-PS5.elf":          {"host": "https://codeberg.org", "repo": "elmonomalva0/Any-Pad-ps5", "asset": r"^AnyPad-PS5-[0-9][0-9A-Za-z.-]*\.elf$"},
}
# Built by us, no upstream.
OWN_BUILT = {"reboot.elf", "suspend.elf"}


def sh(*a):
    return subprocess.run(a, check=True, capture_output=True, text=True).stdout


def latest_release(up):
    if up["host"] == "github":
        return json.loads(sh("gh", "api", "repos/%s/releases/latest" % up["repo"]))
    url = "%s/api/v1/repos/%s/releases/latest" % (up["host"], up["repo"])
    with urllib.request.urlopen(url, timeout=30) as r:
        return json.load(r)


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "elf-launcher-mirror-sync"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read()


def upstream_bytes(up, rel):
    assets = [a for a in rel.get("assets", []) if re.search(up["asset"], a["name"])]
    if len(assets) != 1:
        raise RuntimeError("asset regex matched %d assets in %s" % (len(assets), rel.get("tag_name")))
    a = assets[0]
    data = fetch(a["browser_download_url"])
    dig = (a.get("digest") or "").replace("sha256:", "")
    if dig and hashlib.sha256(data).hexdigest() != dig:
        raise RuntimeError("download does not match GitHub digest for " + a["name"])
    if up.get("member"):
        z = zipfile.ZipFile(io.BytesIO(data))
        names = [n for n in z.namelist() if re.search(up["member"], n)]
        if len(names) != 1:
            raise RuntimeError("member regex matched %d files in %s" % (len(names), a["name"]))
        data = z.read(names[0])
        src = "%s!%s" % (a["name"], names[0])
    else:
        src = a["name"]
    if data[:4] != b"\x7fELF":
        raise RuntimeError(src + " is not an ELF")
    return data, src, a["browser_download_url"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--apply", action="store_true", help="upload and write payloads.json")
    g.add_argument("--dry-run", action="store_true", help="only print (default)")
    ap.add_argument("--names-out", help="write changed item names here, one per line")
    ap.add_argument("--only", nargs="*", help="limit to these catalog names")
    args = ap.parse_args()

    cat = json.load(open(CATALOG, encoding="utf-8"))
    byname = {p["name"]: p for p in cat["payloads"]}
    changed, problems = [], []
    print("mode: %s" % ("APPLY" if args.apply else "dry run (nothing uploaded or written)"))
    for p in cat["payloads"]:
        if "payload-mirror" in p.get("url", "") and p["name"] not in UPSTREAM and p["name"] not in OWN_BUILT:
            problems.append((p["name"], "on payload-mirror but no upstream in UPSTREAM map"))
    for name in sorted(OWN_BUILT):
        if name in byname:
            print("  %-28s skip (own-built)" % name)
    tmp = tempfile.mkdtemp(prefix="mirror_sync_")
    for name, up in UPSTREAM.items():
        if args.only and name not in args.only:
            continue
        if up.get("skip"):
            print("  %-28s skip (%s)" % (name, up["skip"]))
            continue
        e = byname.get(name)
        if not e:
            problems.append((name, "not in payloads.json"))
            continue
        try:
            rel = latest_release(up)
            data, src, upurl = upstream_bytes(up, rel)
        except Exception as ex:
            problems.append((name, "%s/%s: %s" % (up["host"], up["repo"], ex)))
            print("  %-28s ERROR %s" % (name, ex))
            continue
        sha = hashlib.sha256(data).hexdigest()
        tag = rel.get("tag_name", "")
        cur = str(e.get("checksum") or e.get("sha256") or "").lower()
        if sha == cur:
            print("  %-28s up to date  %-16s %s (%s)" % (name, tag, sha[:12], src))
            continue
        new_url = MIRROR_URL + e["filename"]
        print("  %-28s CHANGE  %s -> %s  size %s -> %d  sha %s -> %s  (%s %s)" % (
            name, e.get("version"), tag, e.get("size"), len(data), cur[:12], sha[:12], up["repo"], src))
        if e.get("url") != new_url:
            print("  %-28s         url %s -> %s" % ("", e.get("url"), new_url))
        if args.apply:
            f = os.path.join(tmp, e["filename"])
            open(f, "wb").write(data)
            try:
                sh("gh", "release", "upload", MIRROR_TAG, f, "--clobber", "-R", OWNER_REPO)
            except subprocess.CalledProcessError as ex:
                problems.append((name, "upload failed: %s" % (ex.stderr or ex).strip()))
                continue
            e["url"] = new_url
            e["size"] = len(data)
            e["checksum"] = sha
            e["sha256"] = sha
            e["version"] = tag
            if up["host"] != "github":
                e["source_direct"] = upurl
        changed.append(name)
    if args.apply and changed:
        open(CATALOG, "w", encoding="utf-8").write(json.dumps(cat, indent=2, ensure_ascii=False) + "\n")
        print("payloads.json updated (%d)" % len(changed))
    print("%d would change%s" % (len(changed), "" if not args.apply else " (applied)"))
    if args.names_out:
        with open(args.names_out, "w") as f:
            f.write("".join(n + "\n" for n in changed))
    warn = "::warning::" if os.environ.get("GITHUB_ACTIONS") else "  problem: "
    for n, why in problems:
        print("%s%s: %s" % (warn, n, why))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
