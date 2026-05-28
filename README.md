<p align="center">
  <img src="program_info/prismonline.svg" height="128" alt="PrismOnline Logo"/>
</p>

<h1 align="center">PrismOnline</h1>

<p align="center">
  <b>A Prism Launcher fork with built-in modpack sharing</b><br/>
  Share your Minecraft modpacks with friends using a simple key system — no file hosting needed.
</p>

<p align="center">
  <a href="https://github.com/renatskayt/PrismOnline/releases"><img src="https://img.shields.io/github/v/release/renatskayt/PrismOnline?style=flat-square" alt="Release"/></a>
  <a href="https://github.com/renatskayt/PrismOnline/blob/main/LICENSE"><img src="https://img.shields.io/github/license/renatskayt/PrismOnline?style=flat-square" alt="License"/></a>
</p>

---

## What is PrismOnline?

PrismOnline is a fork of [Prism Launcher](https://prismlauncher.org/) that adds a **pack sharing system**. It lets you:

- **Share** any Minecraft instance as a modpack with one click
- **Receive** a unique key (e.g. `SKAYT-A1B2-C3D4`) to send to friends  
- **Import** shared packs by entering the key
- **Push updates** — modify your modpack and push changes to the same key
- **Auto-update** — players with the key get notified about updates

No need for CurseForge, Modrinth uploads, or file sharing services. Just share the key.

## Features

### 🔗 Pack Sharing System
| Feature | Description |
|---------|-------------|
| **Share Pack** | Export and upload your instance to the PrismOnline server |
| **Download Pack** | Import a shared pack by entering its key |
| **Push Update** | Update an existing shared pack on the server |
| **Auto-Update** | Right-click → "Update Pack" to pull the latest version |
| **PrismOnline Key** | Each shared instance gets a persistent key stored in `instance.cfg` |

### 🎮 Everything from Prism Launcher
- Microsoft & offline account support
- Modrinth and CurseForge modpack support  
- Fabric, Forge, Quilt, NeoForge loader support
- Mod, resource pack, and shader management
- Java auto-detection and management

## How It Works

### Sharing a Pack
1. Open **PrismOnline → Pack Sharing** from the toolbar
2. Select your instance and click **Share Pack**
3. Copy the generated key and send it to friends

### Importing a Pack
1. Open **PrismOnline → Pack Sharing**
2. Enter the key in the download field
3. Click **Download & Import**

### Updating a Shared Pack
1. Modify your instance (add/remove mods, change configs)
2. Open **Pack Sharing** and click **Push Update to Server**
3. Friends right-click the instance → **🔄 Update Pack** to get changes

## Building from Source

### Requirements
- Qt 6.x
- CMake 3.16+
- A C++17 compiler
- Java (for running Minecraft)

### Linux
```bash
git clone https://github.com/renatskayt/PrismOnline.git
cd PrismOnline
mkdir build-linux && cd build-linux
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
```

### Windows (cross-compile with MinGW)
```bash
mkdir build-win && cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake
make -j$(nproc)
```

## Configuration

In the launcher settings, you can configure:
- **Share Server URL** — the PrismOnline sharing server address.


## License

PrismOnline is licensed under the **GPL-3.0** license, same as Prism Launcher.

This project is a fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher), which itself is a fork of MultiMC.

## Credits

- [Prism Launcher](https://prismlauncher.org/) — the base launcher
- [MultiMC](https://multimc.org/) — the original project
- [Modrinth](https://modrinth.com/) — mod platform API
- [CurseForge](https://curseforge.com/) — mod platform API
