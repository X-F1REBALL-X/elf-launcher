#!/usr/bin/env bash
# Cross-compile the Elf Launcher Windows app (Edge WebView2) from Linux.
set -euo pipefail
cd "$(dirname "$0")"
VER=$(sed -n 's/^\tappVersion *= "\(.*\)"/\1/p' main.go)
mkdir -p web dist
cp ../../index.html web/index.html
GOWINRES=${GOWINRES:-$(go env GOPATH)/bin/go-winres}
[ -x "$GOWINRES" ] || go install github.com/tc-hib/go-winres@latest
(cd winres && "$GOWINRES" make --in winres.json --arch amd64 --out ../rsrc)
GOOS=windows GOARCH=amd64 CGO_ENABLED=0 go build -trimpath -ldflags "-s -w -H=windowsgui" -o "dist/ElfLauncher-$VER.exe" .
ls -la "dist/ElfLauncher-$VER.exe"; sha256sum "dist/ElfLauncher-$VER.exe"
