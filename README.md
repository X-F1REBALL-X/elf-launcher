# Elf Launcher

Home screen and browser ELF payload launcher for a jailbroken PS5.

![Elf Launcher 2.0.0](docs/screenshots/home.png)

**Version 2.0.0**, a full overhaul. See the [guide and what's new](https://x-f1reball-x.github.io/elf-launcher/guide/).

- Guide: https://x-f1reball-x.github.io/elf-launcher/guide/
- Live page: https://x-f1reball-x.github.io/elf-launcher/
- Release: https://github.com/X-F1REBALL-X/elf-launcher/releases/tag/2.0.0

## Downloads

- [elf-launcher.elf](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/2.0.0/elf-launcher.elf) - launcher payload
- [elf-launcher-install.elf](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/2.0.0/elf-launcher-install.elf) - home screen installer
- [SHA256SUMS.txt](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/2.0.0/SHA256SUMS.txt)

## Quick start

1. Jailbreak and start elfldr on port `9021`.
2. Send `elf-launcher.elf` once. It serves the page on port `1000` and installs the home screen tile.
3. Open the tile, or `http://<PS5 IP>:1000` from your phone or PC.
4. Pick your AutoPayload list and you are done.

## Layout

- `docs/` - GitHub Pages (live page, guide, catalog)
- `launcher/` - built ELF + sha256
- `host/hbinstall/` - launcher source and web UI

Developed by [X-F1REBALL-X](https://github.com/X-F1REBALL-X).
