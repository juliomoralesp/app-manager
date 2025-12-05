# Project Overview

This project provides a set of tools for managing applications on Debian-based Linux systems. It consists of two main components:

*   **`app-manager`:** A Terminal User Interface (TUI) application written in C. It allows users to view, search, select, and uninstall installed applications. It also identifies applications that can be updated.
*   **`app-uninstaller.py`:** A command-line script written in Python that lists all installed applications and allows the user to select one to uninstall.

The C application (`app-manager`) provides a more interactive and feature-rich experience, while the Python script (`app-uninstaller.py`) offers a simpler, more direct way to uninstall a specific application.

# Building and Running

## `app-manager` (C TUI)

To compile and run the `app-manager` application:

1.  **Compile the source code:**
    ```bash
    gcc -o app-manager app-manager.c
    ```

2.  **Run the application:**
    ```bash
    ./app-manager
    ```
    The application may require `sudo` privileges for uninstalling applications. The keybindings for the TUI are configurable in the `app-manager.conf` file.

## `app-uninstaller.py` (Python CLI)

To run the `app-uninstaller.py` script:

```bash
python3 app-uninstaller.py
```
The script will prompt for `sudo` password when uninstalling an application.

# Development Conventions

*   The project uses both C and Python. C is used for the more complex TUI, while Python is used for a simpler CLI tool.
*   The `app-manager.c` file implements a TUI using raw terminal I/O instead of a library like ncurses. It features searching, pagination, multi-select, and batch operations.
*   The C code uses `popen` and `execvp` to execute shell commands and capture their output.
*   The Python script uses the `subprocess` module to achieve the same.
*   Keybindings for the `app-manager` TUI are configurable via the `app-manager.conf` file.
