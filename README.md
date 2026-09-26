# elf-launcher

A PS5 launcher for ELF payloads. Open it after the console is already exploited.

Page: https://x-f1reball-x.github.io/elf-launcher/

Release: https://github.com/X-F1REBALL-X/elf-launcher/releases/tag/v0.0.7

- [elf-launcher.elf](https://github.com/X-F1REBALL-X/elf-launcher/raw/main/launcher/elf-launcher.elf) installs the home tile.
- [elf-launcher-data.zip](https://github.com/X-F1REBALL-X/elf-launcher/releases/download/v0.0.7/elf-launcher-data.zip) is the payload folders. Copy its contents to `/data/elf-launcher` on the console.

## How it works

1. Jailbreak the console first.
2. Open the page in the PS5 browser. It starts elfldr on `127.0.0.1:9021` and shows **Elfldr Ready**.
3. Open a folder and press **Send**. The file comes from this repository and is handed to elfldr.
4. On a phone or a PC the same button is **Download**.
5. **Select** marks several files. **Multi load** sends them one after another. On a phone that button is **Multi download**.
6. **Auto** saves a list. The next time the page opens, those files send themselves after 3 seconds. **Cancel** stops that.
7. **Send** always re-sends the payload, even if the row shows **Active** or **Sent**.
8. **Kill** (PS5 only, next to Send) closes a running payload process. Requires **payload-manager.elf** running on the console (`127.0.0.1:8084`).

Circle goes back. Triangle refreshes the page. Cross confirms. Square closes the launcher.

## Downloads

Same files as the page.

### Launcher

| Payload | Version | What it does | Download |
|---|---|---|---|
| `elf-launcher.elf` | `0.0.7` | Installs the home screen tile and serves this page from the console. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/launcher/elf-launcher.elf) |

### Loader

| Payload | Version | What it does | Download |
|---|---|---|---|
| `elfldr-ps5.elf` | `v0.26` | Loads ELF payloads sent to port 9021. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/loader/elfldr-ps5.elf) |

### WebKit

| Payload | Version | What it does | Download |
|---|---|---|---|
| `webkit-autoloader-installer.elf` | `v0.4.0` | Installs the WK-AutoLoader tile on the home screen. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/webkit/webkit-autoloader-installer.elf) |
| `wk-autoloader.elf` | `v0.2.5` | Jailbreaks again from the home screen, with no browser host. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/webkit/wk-autoloader.elf) |
| `Goldengames-PS5-Jailbreak.elf` | `v1.1.1` | Home screen jailbreak, then starts the payload you picked. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/webkit/Goldengames-PS5-Jailbreak.elf) |

### HEN

| Payload | Version | What it does | Download |
|---|---|---|---|
| `etaHEN-2.5B.bin` | `2.5B` | HEN toolbox: kstuff, fPKG, plugins, cheats, and optional FTP. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/hen/etaHEN-2.5B.bin) |
| `onionHEN.elf` | `v0.0.13` | HEN stack: ShellUI toolbox, fPKG, plugins, and cheats. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/hen/onionHEN.elf) |

### Apps

| Payload | Version | What it does | Download |
|---|---|---|---|
| `PS5HostPayloads.elf` | `1.0` | Home screen menu of bundled payloads. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/apps/PS5HostPayloads.elf) |
| `kura-loader-ps5.elf` | `v1.6.50` | Console side of Kura. Receives installs from the Kura app. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/apps/kura-loader-ps5.elf) |
| `launchpad-install.elf` | `v0.2` | Installs the Launchpad homebrew menu. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/apps/launchpad-install.elf) |
| `payload-manager.elf` | `v0.5.1` | Home screen app that sends payloads to elfldr. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/apps/payload-manager.elf) |

### PKG

| Payload | Version | What it does | Download |
|---|---|---|---|
| `fetchpkg.elf` | `v0.3.3` | Downloads official game updates and merges split PKG parts. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/pkg/fetchpkg.elf) |
| `pkg-manager.elf` | `v1.2.4` | Installs, deletes, and manages PKGs on the console. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/pkg/pkg-manager.elf) |
| `pkg_install.elf` | `v0.34` | Installs a PKG that was staged by websrv. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/pkg/pkg_install.elf) |
| `ps5upload.elf` | `v5.33.2` | Fast LAN transfer, PKG install, image mount, files, and payload send. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/pkg/ps5upload.elf) |

### FTP

| Payload | Version | What it does | Download |
|---|---|---|---|
| `ftpsrv-drakmor.elf` | `1.16-ng-stable` | FTP server on port 2121. Drakmor build, can remount system as writable. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/ftp/ftpsrv-drakmor.elf) |
| `ftpsrv-ps5-install.elf` | `v0.21.1` | Installs ftpsrv so it launches from the home screen. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/ftp/ftpsrv-ps5-install.elf) |
| `ftpsrv-ps5.elf` | `v0.21.1` | FTP server on port 2121, with SELF decrypt and system remount. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/ftp/ftpsrv-ps5.elf) |
| `ps5_ftp_server.elf` | `v2.0` | High-speed FTP server. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/ftp/ps5_ftp_server.elf) |
| `zftpd-ps5-zhttp.elf` | `v1.5.0` | Zero-copy FTP server plus an HTTP file server. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/ftp/zftpd-ps5-zhttp.elf) |
| `zftpd-ps5.elf` | `v1.5.0` | Zero-copy FTP server. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/ftp/zftpd-ps5.elf) |

### Files

| Payload | Version | What it does | Download |
|---|---|---|---|
| `BFpilot.elf` | `v0.4.4` | File manager that opens in the browser. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/files/BFpilot.elf) |
| `web-file-mgr.elf` | `v1.9` | File manager with its own web page. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/files/web-file-mgr.elf) |
| `wfm-7zip-helper.elf` | `v1.9` | Packs and unpacks archives for the web file manager. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/files/wfm-7zip-helper.elf) |

### Network

| Payload | Version | What it does | Download |
|---|---|---|---|
| `nanodns.elf` | `0.4` | DNS proxy. Can block Sony update servers. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/network/nanodns.elf) |
| `websrv-ps5.elf` | `v0.34` | Web server: launch homebrew, browse files, and read SMB shares. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/network/websrv-ps5.elf) |

### Debug

| Payload | Version | What it does | Download |
|---|---|---|---|
| `gdbsrv-ps5.elf` | `v0.9` | GDB server for debugging a process. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/debug/gdbsrv-ps5.elf) |
| `klogsrv-ps5.elf` | `v0.9` | Streams the kernel log to a PC. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/debug/klogsrv-ps5.elf) |
| `notify_test.elf` | `1.0` | Shows one test notification. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/debug/notify_test.elf) |
| `ps5debug-NG.elf` | `1.3.2` | Debugger that runs inside SceShellCore. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/debug/ps5debug-NG.elf) |
| `ps5debug.elf` | `0.0.1` | Older PS5 debugger. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/debug/ps5debug.elf) |
| `shsrv-ps5.elf` | `v0.20` | Command shell on port 2323. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/debug/shsrv-ps5.elf) |

### Storage

| Payload | Version | What it does | Download |
|---|---|---|---|
| `SMPlusGui.elf` | `v1.0.1` | Web page that controls ShadowMountPlus. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/SMPlusGui.elf) |
| `dump_installer.elf` | `1.07` | Installs a dumped game. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/dump_installer.elf) |
| `dump_runner.elf` | `v1.02` | Runs a dumped game. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/dump_runner.elf) |
| `game-compressor.elf` | `v1.0.4` | Compresses or repairs games for ShadowMountPlus. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/game-compressor.elf) |
| `garlic-savemgr-v1.13.1.elf` | `v1.13.1` | Save manager, mirror build v1.13.1. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/garlic-savemgr-v1.13.1.elf) |
| `garlic-savemgr.elf` | `v1.7` | Copies and restores game saves. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/garlic-savemgr.elf) |
| `ps5-app-dumper.elf` | `v1.11` | Dumps installed games to files. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/ps5-app-dumper.elf) |
| `shadowmountplus.elf` | `1.7beta1` | Mounts dumped games from internal storage or USB. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/storage/shadowmountplus.elf) |

### Power

| Payload | Version | What it does | Download |
|---|---|---|---|
| `SystemStateManager.elf` | `1.0.0` | Reboot, rest mode, shutdown, or eject, controlled over TCP. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/SystemStateManager.elf) |
| `TM44.elf` | `0.1` | Puts the console into rest mode. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/TM44.elf) |
| `poweroff.elf` | `1.0` | Powers the console off. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/poweroff.elf) |
| `reboot-masterps0.elf` | `1.0` | Reboots the console. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/reboot-masterps0.elf) |
| `reboot.elf` | `own-built` | Reboots the console. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/reboot.elf) |
| `reboot-own.elf` | `own-built` | Same build as `reboot.elf`. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/reboot-own.elf) |
| `suspend-masterps0.elf` | `1.0` | Puts the console into rest mode. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/suspend-masterps0.elf) |
| `suspend.elf` | `own-built` | Puts the console into rest mode. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/suspend.elf) |
| `suspend-own.elf` | `own-built` | Same build as `suspend.elf`. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/system/suspend-own.elf) |

### Media

| Payload | Version | What it does | Download |
|---|---|---|---|
| `plutotv-install.elf` | `latest` | Installs the Pluto TV app. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/media/plutotv-install.elf) |
| `svtplay-install.elf` | `latest` | Installs the SVT Play app. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/media/svtplay-install.elf) |
| `tv4play-install.elf` | `latest` | Installs the TV4 Play app. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/media/tv4play-install.elf) |

### Cheats

| Payload | Version | What it does | Download |
|---|---|---|---|
| `CheatRunner.elf` | `v0.17` | Loads cheats into a running game. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/cheats/CheatRunner.elf) |
| `Common_FPS_PS5.elf` | `v1.1.0` | Draws an FPS counter over the game. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/cheats/Common_FPS_PS5.elf) |

### Tools

| Payload | Version | What it does | Download |
|---|---|---|---|
| `BackPork.elf` | `0.1` | Mounts replacement libraries into a game when it launches. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/BackPork.elf) |
| `LinkDev.elf` | `v0.4.2` | Pairs Remote Play on a jailbroken console. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/LinkDev.elf) |
| `OffAct.elf` | `v0.4.2` | Activates a local account without PSN. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/OffAct.elf) |
| `ps5-self-pager-full-system.elf` | `v1.2` | Decrypts /system and /system_ex to USB or /data. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/ps5-self-pager-full-system.elf) |
| `ps5-self-pager-game.elf` | `v1.2` | Decrypts the game app0 and patch0 to USB or /data. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/ps5-self-pager-game.elf) |
| `ps5-self-pager-shellcore.elf` | `v1.2` | Decrypts SceShellCore.elf to USB or /data. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/ps5-self-pager-shellcore.elf) |
| `ps5-self-pager-system-common-lib.elf` | `v1.2` | Decrypts /system/common/lib to USB or /data. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/ps5-self-pager-system-common-lib.elf) |
| `rp-get-pin.elf` | `v0.1.1` | Shows the Remote Play pairing PIN. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/rp-get-pin.elf) |
| `Lapy-JB-Daemon.elf` | `v1.2` | Unsandboxes homebrew apps so they run without etaHEN. | [Download](https://raw.githubusercontent.com/X-F1REBALL-X/elf-launcher/main/utils/Lapy-JB-Daemon.elf) |

## Credits

- **X-F1REBALL-X** - elf-launcher, WK-AutoLoader, `reboot.elf`, `suspend.elf`

Upstream authors (specific work, not generic support):

- **ps5-payload-dev** - elfldr, ftpsrv, shsrv, websrv, gdbsrv, klogsrv, offact, fetchpkg, linkdev, launchpad, media installers, classic kstuff
- **EchoStretch** - kstuff-lite, kstuff-toggle, ps5-app-dumper, dump_installer, dump_runner
- **itsPLK** - Payload Manager, WebKit Autoloader installer, PKG Manager; also ps5-payloads-mirror
- **idlesauce** - rp-get-pin, ps5-self-pager
- **Pharaoh2k** - ps5debug-NG
- **someguythatmods** - classic ps5debug
- **Drakmor** - ftpsrv NG fork, nanoDNS
- **seregonwar** - zftpd
- **owendswang** - ps5-web-file-manager (+ 7zip helper)
- **ItsBlurf** - BFpilot
- **manos555555** - ps5-ftp-server
- **phantomptr** - ps5upload
- **earthonion** - garlic-savemgr
- **juma-sayeh** - PS5-Game-Compressor
- **KarnerF** - SMPlusGui
- **notmaj0r** - CheatRunner
- **BestPig** - BackPork
- **porhe911** - Common FPS for PS5
- **MasterPS0** - Power payloads, PS5HostPayloads
- **StonedModder** - TM44, SystemStateManager
- **aydencharles** - onionHEN
- **etaHEN** - etaHEN (optional .bin)
- **ArkSama** - Lapy-JB-Daemon (via mirror)
- **robin2kk** - Goldengames PS5 Autoloader (optional)
- **ps5-linux** - ps5-linux-loader
- **NookieAI** - kura-loader-ps5

All rights remain with original authors. See `SOURCES.md` for licenses and provenance.

## Docs

- `SOURCES.md` - per-file provenance
- `MANIFEST.csv` / `MANIFEST.md` - path, size, sha256, source

Folder browsing still works if you prefer navigating the tree.
