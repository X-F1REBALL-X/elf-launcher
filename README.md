# Elf Launcher

Home-screen and browser ELF payload launcher for PS5.

![Elf Launcher 1.1.6](docs/screenshots/home.png)

**Version 1.1.6** - Open browser runs disk AutoPayload like Leave closed; stays silent when nothing to do.

- Live page: https://x-f1reball-x.github.io/elf-launcher/
- Release: https://github.com/X-F1REBALL-X/elf-launcher/releases/tag/1.1.6

## Downloads

- [elf-launcher.elf](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/1.1.6/elf-launcher.elf) - launcher payload
- [elf-launcher-install.elf](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/1.1.6/elf-launcher-install.elf) - home-screen installer
- [SHA256SUMS.txt](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/1.1.6/SHA256SUMS.txt)

## Usage

1. Jailbreak the console and start elfldr (port 9021).
2. Send `elf-launcher-install.elf` once to add Elf Launcher to the home screen, **or** open the live page in the console browser.
3. Open **Elf Files** - *Download* saves an ELF, *Payload* sends it to elfldr.
4. Mark payloads **Auto** in *AutoPayload* to run them once after each jailbreak.
5. **Update** pulls the latest Elf Launcher from this repo.

## Repo layout

- `docs/` - web UI for GitHub Pages (`index.html`, `payloads.json`, `icons/`, `screenshots/`)
- `launcher/` - built `elf-launcher.elf` + sha256 (self-update source)
- `host/hbinstall/` - home-screen installer source
- Root `index.html` / `payloads.json` - same UI + catalog on `main` (raw)

Created by X-F1REBALL-X.
