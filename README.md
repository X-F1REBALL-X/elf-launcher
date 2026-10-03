# Elf Launcher

PS5 launcher for ELF payloads. Use it only after the console is already jailbroken.

**Elf Launcher 1.0.48-test**

- Page: https://x-f1reball-x.gitlab.io/ElfLauncher
- Release: https://gitlab.com/X-F1REBALL-X/ElfLauncher/-/releases/v1.0.48-test
- ELF: [launcher/elf-launcher.elf](launcher/elf-launcher.elf)

Send `launcher/elf-launcher.elf` to elfldr. It installs the home-screen tile and serves the UI on port 1000. The same page is hosted on GitLab Pages.

## Usage

Elf Launcher sends ELF payloads to the PS5 loader on port 9021.

Download is green and only appears when the file is missing. Payload is orange, and turns blue after it loads.

Downloaded lists files already on the console. The Auto mark is only there. Auto runs once after elfldr is listening, with a 3 second cancel.

Open browser opens the page after WK. Leave closed keeps the page closed but still loads Auto files.

A manual send of `elf-launcher.elf` always installs and opens the page.

The side items are Home, Elf Files, Downloaded, and AutoPayload. There is no Close button and no Kill.
