# Payload icons

Original icons from each project, converted to PNG so the launcher can show them.
Nothing here is redrawn.

| File | Source |
|---|---|
| Goldengames-PS5-Jailbreak.png | icon0 embedded in Goldengames-PS5-Jailbreak-v1.1.1.elf (robin2kk/Goldengames-PS5-Autoloader), 512 to 256 |
| webkit-autoloader-installer.png | home tile icon embedded in webkit-autoloader-installer_v0.6.1.elf (itsPLK/ps5-webkit-autoloader), 512 to 256 |
| dump_installer.png | EchoStretch/dump_installer sce_sys/icon0.png @413b647, 1.4 MB to 256 |
| bfplayer-standalone.png | ItsBlurf/BFplayer assets/icon0.png @ad3bdc9, 1254 to 256 |
| kura-loader-ps5.png | Kura logo embedded in the web UI of kura-loader-ps5.elf v1.6.50 (NookieAI/kura), 96x96 as shipped |

## X-F1REBALL-X's own payloads

reboot.png and suspend.png use the ELF Launcher icon style (icons/icon.svg).
Sources are in src/, built by icons/categories/src/make_icons.py.

## Folder icons

Payloads with no original icon anywhere use the shared icon of their folder,
icons/categories/<folder>.png, built by icons/categories/src/make_icons.py.
| ps5-linux-loader.png | logo.webp from ps5-linux/ps5-linux-loader @ 64637c177eaed277fb6ccce394cbb29dd047b893, WebP to PNG, 1254 to 256 |
| librespot-ps5.png | player/assets/icon.png from stupiditytries/librespot-ps5 @480e140 |
| ps5lm.png | brand/web/icon-512.png from cobanov/PS5LM @e2a8cf4 |
| airpsx.png | pkgroot/sce_sys/icon0.png from barisyild/airpsx @aed9f7b |
| dump_runner | uses the same dump_installer.png (EchoStretch/dump_installer sce_sys/icon0) |

## GitHub avatars (2.0.36)

Payloads that still had only a folder icon now use the public GitHub avatar of the repo owner
(or the known author, for mirrored files), for example https://avatars.githubusercontent.com/u/<id>?v=4&s=256.
Left on the folder icon because there is no public GitHub owner:
kstuff1.13-fpkg-dr-test8-rest.elf, kstuff1.13-fpkg-dr-test8-rest-fast.elf, a53_ppr_install_fast.elf.
