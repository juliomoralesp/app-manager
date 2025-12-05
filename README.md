# app-manager

Terminal app manager for Debian-based systems

This repository contains two tools to manage installed packages on Debian-based systems:

- `app-manager` — a C-based TUI (terminal user interface) that lists installed packages, shows upgradable packages, and supports searching, multi-select, batch update/uninstall (uses `apt-get`).
- `app-uninstaller.py` — a simple Python CLI helper that lists installed packages and uninstalls one selected package.

## Building

To compile the TUI C program:

```bash
gcc -o app-manager app-manager.c
```

Then run:

```bash
./app-manager
```

Note: uninstall/update actions may require `sudo` privileges.

## Files in this repository

- `app-manager.c` — main TUI source
- `app-manager.conf` — default keybindings
- `app-uninstaller.py` — Python uninstall helper
- `GEMINI.md` — project notes
- `LICENSE` — GPL-3.0 license

## Configuration

Edit `app-manager.conf` to change keybindings. The default configuration is included.

## License

This project is licensed under the GNU General Public License v3.0. See `LICENSE` for details.
