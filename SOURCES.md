# SOURCES

PS5 ELF/bin collection assembled from public GitHub releases and local own-built payloads.
Prefer official release assets. Hashes in MANIFEST.csv / MANIFEST.md.

## Duplicates (same sha256)

- `306aad2e95abd93e767ada2aba3e7b54c59d312977eda344ddb717405a3e1017`: `system/reboot-own.elf`, `system/reboot.elf`
- `2934a7adbf50a8e6931ce4ec2374bcd556b441b69d61954e2879a8beb15dc681`: `system/suspend-own.elf`, `system/suspend.elf`

- Note: EchoStretch `dump_runner_toggle_kstuff.elf` matched `dump_runner.elf` sha256; toggle copy removed.

## Per-file sources

| file | source | tag / date | license | purpose |
|---|---|---|---|---|
| `debug/gdbsrv-ps5.elf` | https://github.com/ps5-payload-dev/gdbsrv | v0.9 / 2026-08-02 | GPL-3.0 | GDB stub server for PS5 |
| `debug/klogsrv-ps5.elf` | https://github.com/ps5-payload-dev/klogsrv | v0.9 / 2026-08-02 | GPL-3.0 | Kernel log server |
| `debug/notify_test.elf` | https://github.com/MasterPS0/PS5-Power-Payloads-Project | 1.0 / 2026-07-06 | NOASSERTION | Notification test payload |
| `debug/ps5debug-NG.elf` | https://github.com/Pharaoh2k/ps5debug-NG | 1.3.2 / 2026-09-23 | GPL-3.0 | PS5 debugger (userland TCP in SceShellCore) |
| `debug/ps5debug.elf` | https://github.com/someguythatmods/ps5debug | 0.0.1 / 2025-02 | unknown | Classic PS5Debug ELF (older) |
| `cheats/CheatRunner.elf` | https://github.com/notmaj0r/CheatRunner | v0.17 / 2026-07-30 | unknown | Cheat trainer homebrew |
| `apps/PS5HostPayloads.elf` | https://github.com/MasterPS0/PS5HostPayloads | 1.0 / 2026 | NOASSERTION | Host payloads bundle ELF |
| `pkg/fetchpkg.elf` | https://github.com/ps5-payload-dev/fetchpkg | v0.3.3 / 2026-04-03 | unknown | PKG fetch helper for PS5 |
| `apps/kura-loader-ps5.elf` | https://github.com/NookieAI/kura | v1.6.50 / 2026 | unknown | Kura console-side loader payload |
| `apps/launchpad-install.elf` | https://github.com/ps5-payload-dev/launchpad | v0.2 / 2026-08-31 | GPL-3.0 | Launchpad installer |
| `apps/payload-manager.elf` | https://github.com/itsPLK/ps5-payload-manager | v0.5.1 / 2026-08-02 | GPL-3.0 | Payload Manager UI |
| `pkg/pkg-manager.elf` | https://github.com/itsPLK/ps5-pkg-manager | v1.2.4 / 2026-09-23 | unknown | On-console PKG manager |
| `pkg/pkg_install.elf` | https://github.com/ps5-payload-dev/websrv | v0.34 / 2026-08-02 | GPL-3.0 | PKG install helper (from websrv PKGInstall.zip) |
| `webkit/wk-autoloader.elf` | https://github.com/X-F1REBALL-X/WK-AutoLoader | v0.2.5 / 2026-09-22 | unknown | WK-AutoLoader |
| `webkit/webkit-autoloader-installer.elf` | https://github.com/itsPLK/ps5-webkit-autoloader | v0.4.0 / 2026-08-24 | GPL-3.0 | WebKit autoloader installer |
| `launcher/elf-launcher.elf` | https://github.com/X-F1REBALL-X/elf-launcher | 0.0.6 / 2026-09-25 | own | Installs the home tile and serves the launcher page from the console |
| `kernel/kexp.bin` | local-sjb-work | local / 2026-09-22 | unknown | Kernel exploit helper binary (not ELF) - Seed from /workspace/sjb-work/payloads |
| `kernel/kstuff-lite.elf` | https://github.com/EchoStretch/kstuff-lite | v1.11 / 2026-09-20 | NOASSERTION | kstuff-lite kernel patches (EchoStretch) |
| `kernel/kstuff-ps5-payload-dev.elf` | https://github.com/ps5-payload-dev/kstuff | v2025-01-26 / 2025-01-25 | NOASSERTION | Classic kstuff kernel helper (org release) |
| `kernel/kstuff-toggle-1.elf` | https://github.com/EchoStretch/kstuff-toggle | 0.6 / 2026-05-07 | NOASSERTION | Toggle kstuff on/off variant 1 |
| `kernel/kstuff-toggle-2.elf` | https://github.com/EchoStretch/kstuff-toggle | 0.6 / 2026-05-07 | NOASSERTION | Toggle kstuff on/off variant 2 |
| `kernel/kstuff-toggle-3.elf` | https://github.com/EchoStretch/kstuff-toggle | 0.6 / 2026-05-07 | NOASSERTION | Toggle kstuff on/off variant 3 |
| `kernel/kstuff-v1.6.7.elf` | mirror:itsPLK/ps5-payloads-mirror | v1.6.7 / mirror | unknown | kstuff build v1.6.7 from community mirror |
| `kernel/ps5-linux-loader.elf` | https://github.com/ps5-linux/ps5-linux-loader | v2.5 / 2026-09-05 | unknown | Linux loader (rest-mode path) |
| `loader/elfldr-ps5.elf` | https://github.com/ps5-payload-dev/elfldr | v0.26 + local build / 2026-09-24 | GPL-3.0 | ELF loader daemon (port 9021) - local build, serves the `?uri=` and `/ip` endpoints the page calls, so it does not match the upstream release asset |
| `loader/already-active.elf` | https://github.com/X-F1REBALL-X/elf-launcher | own-built / 2026-09-24 | own | Notification sent when the picked payload is already running |
| `media/plutotv-install.elf` | https://github.com/ps5-payload-dev/plutotv | latest / 2026 | GPL-3.0 | Pluto TV media app installer |
| `media/svtplay-install.elf` | https://github.com/ps5-payload-dev/svtplay | latest / 2026 | GPL-3.0 | SVT Play media app installer |
| `media/tv4play-install.elf` | https://github.com/ps5-payload-dev/tv4play | latest / 2026 | GPL-3.0 | TV4 Play media app installer |
| `files/BFpilot.elf` | https://github.com/ItsBlurf/BFpilot | v0.4.4 / 2026-08-11 | unknown | Browser-based lightweight file manager |
| `ftp/ftpsrv-drakmor.elf` | https://github.com/Drakmor/ftpsrv | 1.16-ng-stable / 2026-09-20 | unknown | Drakmor ftpsrv fork (NG stable) |
| `ftp/ftpsrv-ps5-install.elf` | https://github.com/ps5-payload-dev/ftpsrv | v0.21.1 / 2026-08-20 | GPL-3.0 | ftpsrv installer ELF |
| `ftp/ftpsrv-ps5.elf` | https://github.com/ps5-payload-dev/ftpsrv | v0.21.1 / 2026-08-20 | GPL-3.0 | FTP server for jailbroken PS5 |
| `network/nanodns.elf` | https://github.com/drakmor/nanoDNS | 0.4 / 2026-08-04 | unknown | PS4/PS5 DNS proxy |
| `ftp/ps5_ftp_server.elf` | https://github.com/manos555555/ps5-ftp-server | v2.0 / 2026 | unknown | High-speed FTP server |
| `pkg/ps5upload.elf` | https://github.com/phantomptr/ps5upload | v5.33.2 / 2026-09-23 | unknown | Fast PKG/homebrew transfer payload |
| `files/web-file-mgr.elf` | https://github.com/owendswang/ps5-web-file-manager | v1.9 / 2026-09-16 | unknown | Web UI file manager |
| `network/websrv-ps5.elf` | https://github.com/ps5-payload-dev/websrv | v0.34 / 2026-08-02 | GPL-3.0 | HTTP homebrew launcher / websrv |
| `ftp/zftpd-ps5-zhttp.elf` | https://github.com/seregonwar/zftpd | v1.5.0 / 2026-06-14 | unknown | zftpd with HTTP (PS5) |
| `ftp/zftpd-ps5.elf` | https://github.com/seregonwar/zftpd | v1.5.0 / 2026-06-14 | unknown | Zero-copy FTP daemon (PS5) |
| `webkit/Goldengames-PS5-Jailbreak.elf` | https://github.com/robin2kk/Goldengames-PS5-Autoloader | v1.1.1 / 2026 | unknown | Autoloader jailbreak bundle - OPTIONAL (large) |
| `utils/Lapy-JB-Daemon.elf` | https://github.com/ArkSama/PS5-Lapy-JB-Daemon | v1.2 / mirror | unknown | Standalone JB daemon (etaHEN-like API) - OPTIONAL - Via itsPLK/ps5-payloads-mirror; no GitHub release on origin |
| `hen/etaHEN-2.5B.bin` | https://github.com/etaHEN/etaHEN | 2.5B / 2025-12-25 | GPL-3.0 | etaHEN AIO HEN payload (.bin not ELF) - OPTIONAL |
| `hen/onionHEN.elf` | https://github.com/aydencharles/onionHEN | v0.0.13 / 2026-09-11 | unknown | Alternate HEN enabler (optional) |
| `system/reboot-own.elf` | own-built:/workspace/ps5-reboot | own-built / 2026-09-23 | own | Same reboot.elf (user-built copy) |
| `system/suspend-own.elf` | own-built:/workspace/ps5-suspend | own-built / 2026-09-23 | own | Same suspend.elf (user-built copy) |
| `storage/SMPlusGui.elf` | https://github.com/KarnerF/SMPlusGui | v1.0.1 / 2026-09-21 | unknown | Web GUI for ShadowMountPlus |
| `storage/dump_installer.elf` | https://github.com/EchoStretch/dump_installer | 1.07 / 2026-07-12 | NOASSERTION | Install dumped apps |
| `storage/dump_runner.elf` | https://github.com/EchoStretch/dump_runner | v1.02 / 2026-03-29 | NOASSERTION | Runner for dump workflow |
| `storage/game-compressor.elf` | https://github.com/juma-sayeh/PS5-Game-Compressor | v1.0.4 / 2026-08-21 | NOASSERTION | Compress/repair ShadowMountPlus games |
| `storage/garlic-savemgr-v1.13.1.elf` | https://github.com/earthonion/garlic-savemgr | v1.13.1 / mirror | unknown | Garlic save mgr alternate build from mirror - Via itsPLK/ps5-payloads-mirror |
| `storage/garlic-savemgr.elf` | https://github.com/earthonion/garlic-savemgr | v1.7 / 2026-03-16 | unknown | Local save manager (prefer author v1.7 over older mirror) |
| `storage/ps5-app-dumper.elf` | https://github.com/EchoStretch/ps5-app-dumper | v1.11 / 2026-07-28 | NOASSERTION | Dump installed PS5 apps |
| `storage/shadowmountplus.elf` | https://github.com/drakmor/ShadowMountPlus | 1.7beta1 / 2026-09-20 | unknown | ShadowMountPlus game mount helper |
| `system/SystemStateManager.elf` | https://github.com/StonedModder/PS5-SystemStateManager | 1.0.0 / 2026 | NOASSERTION | TCP control for reboot/rest/shutdown/eject |
| `system/TM44.elf` | https://github.com/StonedModder/TM44 | 0.1 / 2026-06-30 | NOASSERTION | Minimal rest-mode / EnterStandby payload |
| `system/poweroff.elf` | https://github.com/MasterPS0/PS5-Power-Payloads-Project | 1.0 / 2026-07-06 | NOASSERTION | Power off PS5 |
| `system/reboot-masterps0.elf` | https://github.com/MasterPS0/PS5-Power-Payloads-Project | 1.0 / 2026-07-06 | NOASSERTION | Reboot PS5 (MasterPS0) |
| `system/reboot.elf` | own-built:/workspace/ps5-reboot | own-built / 2026-09-23 | own | Userspace reboot via reboot(RB_AUTOBOOT) |
| `system/suspend-masterps0.elf` | https://github.com/MasterPS0/PS5-Power-Payloads-Project | 1.0 / 2026-07-06 | NOASSERTION | Rest mode template (MasterPS0) |
| `system/suspend.elf` | own-built:/workspace/ps5-suspend | own-built / 2026-09-23 | own | Rest mode via sceSystemStateMgrEnterStandby |
| `utils/BackPork.elf` | https://github.com/BestPig/BackPork | 0.1 / 2026-04-30 | unknown | Sideload system libraries into games |
| `cheats/Common_FPS_PS5.elf` | https://github.com/porhe911/Common-FPS-for-PS5 | v1.1.0 / 2026-09-05 | GPL-3.0 | Universal FPS overlay |
| `utils/LinkDev.elf` | https://github.com/ps5-payload-dev/linkdev | v0.4.2 / 2025-06-28 | GPL-3.0 | Remote Play / link device helper |
| `utils/OffAct.elf` | https://github.com/ps5-payload-dev/offact | v0.4.2 / 2025-06-28 | GPL-3.0 | Offline account activator |
| `utils/ps5-self-pager-full-system.elf` | https://github.com/idlesauce/ps5-self-pager | v1.2 / 2026-06-09 | Unlicense | SELF pager / decrypt helper variant |
| `utils/ps5-self-pager-game.elf` | https://github.com/idlesauce/ps5-self-pager | v1.2 / 2026-06-09 | Unlicense | SELF pager / decrypt helper variant |
| `utils/ps5-self-pager-shellcore.elf` | https://github.com/idlesauce/ps5-self-pager | v1.2 / 2026-06-09 | Unlicense | SELF pager / decrypt helper variant |
| `utils/ps5-self-pager-system-common-lib.elf` | https://github.com/idlesauce/ps5-self-pager | v1.2 / 2026-06-09 | Unlicense | SELF pager / decrypt helper variant |
| `utils/rp-get-pin.elf` | https://github.com/idlesauce/ps5-remoteplay-get-pin | v0.1.1 / 2025-03-09 | NOASSERTION | Get Remote Play pairing PIN |
| `debug/shsrv-ps5.elf` | https://github.com/ps5-payload-dev/shsrv | v0.20 / 2026-08-02 | GPL-3.0 | Telnet-like shell server (port 2323) |
| `files/wfm-7zip-helper.elf` | https://github.com/owendswang/ps5-web-file-manager | v1.9 / 2026-09-16 | unknown | 7zip helper for web file manager |
| `archive/elf-launcher-install.elf` | https://github.com/X-F1REBALL-X/elf-launcher | own-built / 2026-09-24 | own | Superseded elf-launcher installer build |
| `archive/ps5-elfs-host.elf` | https://github.com/X-F1REBALL-X/elf-launcher | own-built / 2026-09-24 | own | Superseded ps5-elfs host |
| `archive/ps5-elfs-host-installer.elf` | https://github.com/X-F1REBALL-X/elf-launcher | own-built / 2026-09-24 | own | Superseded ps5-elfs host installer |
| `archive/ps5-elfs-install.elf` | https://github.com/X-F1REBALL-X/elf-launcher | own-built / 2026-09-24 | own | Superseded ps5-elfs installer |

## Developers / orgs covered

- ArkSama (via itsPLK mirror)
- BestPig
- Drakmor / drakmor
- EchoStretch
- ItsBlurf
- KarnerF
- MasterPS0
- NookieAI
- Pharaoh2k
- StonedModder
- X-F1REBALL-X - elf-launcher, WK-AutoLoader, reboot.elf, suspend.elf
- aydencharles
- earthonion
- etaHEN
- idlesauce
- itsPLK
- juma-sayeh
- local sjb-work seed (kexp.bin, shadowmountplus match)
- manos555555
- notmaj0r
- owendswang
- phantomptr
- porhe911
- ps5-linux
- ps5-payload-dev (john-tornblom et al.)
- robin2kk
- seregonwar
- someguythatmods

## Failed / skipped

- Cryptogenic/PS5-SELF-Decrypter: no GitHub release assets
- PS5Dev/Byepervisor, ctn123/Byepervisor, ctn123/PS5Debug: no usable PS5 ELF releases (ctn123/Binary-Releases is PS4 Save Mounter)
- Scene-Collective: PS4-focused payload repos; skipped
- LightningMods/Itemzflow, Al-Azif/psfree-lapse: releases without standalone PS5 .elf assets
- Master-s/PS5-Payloads: release tag with empty assets
- ArkSama/PS5-Lapy-JB-Daemon: no origin release; used itsPLK mirror copy
- Huge websrv homebrew zips (RetroArch, ScummVM, emu packs): skipped (not standalone utility ELFs)
- GoldHEN: PS4 .7z only

## Notes

- `hen/` holds the etaHEN .bin and onionHEN; marked optional (user does not use etaHEN day-to-day). The autoloaders live under `webkit/` and the JB daemon under `utils/`, matching the folders on the page.
- `kexp.bin` is not an ELF; kept under kernel/ for completeness.
- Garlic save manager: both author `v1.7` and mirror `v1.13.1` kept (different builds).
- Multiple kstuff builds kept (EchoStretch lite, org classic, mirror v1.6.7) because hashes differ.
- Collection date (Asia/Jerusalem): 2026-09-23.
