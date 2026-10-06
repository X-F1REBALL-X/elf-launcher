# Elf Launcher

Home-screen and browser ELF payload launcher for PS5.

![Elf Launcher 1.1.8](docs/screenshots/home.png)

**Version 1.1.8**

- Live page: https://x-f1reball-x.github.io/elf-launcher/
- Release: https://github.com/X-F1REBALL-X/elf-launcher/releases/tag/1.1.8

## Downloads

- [elf-launcher.elf](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/1.1.8/elf-launcher.elf) - launcher payload
- [elf-launcher-install.elf](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/1.1.8/elf-launcher-install.elf) - home-screen installer
- [SHA256SUMS.txt](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/1.1.8/SHA256SUMS.txt)

## Features

- Send / download ELF payloads (elfldr on `9021`, UI on `:1000`)
- **AutoPayload** - run marked ELFs once after each jailbreak (disk `auto.list`)
- **Open browser** and **Leave closed** both run the same disk Auto
- **10 languages:** en, ar, es, fr, de, pt, ru, ja, zh, it (picker; shared `ps5elfs-lang` with WK)
- Self-update + home-screen icon install

## Usage

1. Jailbreak and start elfldr on `9021`.
2. Send `elf-launcher-install.elf` once, or open via the live page / WK Hybrid.
3. Mark Auto payloads; pick Open browser or Leave closed.
4. Use **Update** when a newer build is available.

## Layout

- `docs/` - GitHub Pages
- `launcher/` - built ELF + sha256
- `host/hbinstall/` - installer source

Developed by [X-F1REBALL-X](https://github.com/X-F1REBALL-X).
