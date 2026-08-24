# NexOS — System Specification

# Version 0.1

# alternitech.square.site

# github.com/DansDesigns/NexOS



━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## OVERVIEW

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

NexOS is a security-first, voice & touch focused operating
system built on a seL4 microkernel foundation. Linux,
Android, Windows, and retro gaming apps run in isolated
compatibility layers — no VM boot, no waiting.



━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 1\. KERNEL LAYER

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Microkernel:    seL4 (formally verified, capability-based)
Architecture:   x86\_64 primary | ARM64 secondary
Bootloader:     GRUB2 — UEFI + legacy BIOS fallback

seL4 responsibilities:
• Verified boot integrity (signature check before anything runs)
• Memory isolation — every process is isolated by proof
• Capability-based access control — no ambient authority
• Secure credential enclave — passphrases never leave seL4
• Hosts the Linux hardware domain as a contained VM

Linux VM (hardware domain):
• Boots inside seL4 VMM in approximately 300ms
• Provides: GPU, WiFi, USB, audio, NVMe, i2c drivers
• Exposes /dev, /proc, /sys to NexOS via VirtIO
• User never interacts with it — it is invisible infrastructure
• Kernel: Linux 6.x, minimal config, no desktop, no systemd

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 2\. INIT SYSTEM

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Init:           OpenRC — no systemd, ever
Base userland:  BusyBox (boot scripts and rescue shell only)
Login shell:    Bash (every user session)

/bin/sh  → BusyBox ash  (early boot, initramfs, scripts)
/bin/bash → Bash 5.x    (all interactive sessions)

Package management:
Backend:    apt + dpkg (Debian/Devuan compatible)
Frontend:   nala (faster, cleaner output than apt)
Alias:      apt → nala (system-wide, cannot be bypassed)
set in /emulation/linux/etc/profile.d/nexos.sh
Repos:      Devuan Excalibur (systemd-free Debian fork)
NexOS repository (nexos-specific packages)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 3\. FILESYSTEM LAYOUT

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

NexOS presents a clean, human-readable filesystem.
All legacy Linux internals live inside emulation/linux/.
The root directory contains only things a person can
understand without Unix archaeology.

/
├── applications/           Installed NexOS-native apps
│   ├── firefox/
│   ├── nexos-settings/
│   └── ...
│
├── connections/            Hardware connections as readable files
│   │                       Populated by hardware-detect at boot
│   ├── usb/
│   │   └── SanDisk-16GB/   Named from device descriptor
│   ├── nvme/
│   │   └── Samsung-EVO-256GB/
│   ├── sata/
│   ├── serial/
│   ├── uart/
│   └── i2c/
│
├── users/                  All user home directories
│   └── <username>/         Created by installer
│       ├── documents/
│       ├── downloads/
│       ├── music/
│       ├── pictures/
│       ├── 3D-files/
│       ├── desktop/
│       └── wallpapers/
│
├── hardware/               Live hardware info — files not dirs
│   │                       Populated by hardware-detect at boot
│   ├── cpu/
│   │   ├── model           "Intel Core i7-1165G7"
│   │   ├── cores           "4"
│   │   ├── temp            "42"
│   │   └── arch            "x86\_64"
│   ├── gpu/
│   │   ├── present         (exists = GPU detected)
│   │   ├── model           "Intel Iris Xe Graphics"
│   │   └── driver          "i915"
│   ├── ram/
│   │   ├── total\_gb        "8"
│   │   └── available\_gb    "5.8"
│   ├── battery/            (absent on desktops)
│   │   ├── level           "87"
│   │   └── status          "Charging"
│   ├── network/
│   │   └── wlan0/
│   │       ├── present
│   │       ├── connected   "1"
│   │       └── ssid        "MyNetwork"
│   ├── tier                "T4"
│   └── tier\_desc           "laptop — full stack"
│
├── storage/                Drive mount points
│   ├── internal/           Primary NVMe/SATA
│   └── usb-1/              Removable (auto-mounted, auto-named)
│
├── system/                 NexOS OS files
│   ├── config/             System configuration (replaces /etc)
│   │   ├── network/
│   │   ├── users/          Per-user app config (replaces \~/.config)
│   │   ├── services/       OpenRC service config
│   │   └── nexos/          NexOS-specific config
│   │       └── theme       "dark" or "light"
│   ├── services/           OpenRC service definitions (.initd files)
│   ├── logs/               System logs (replaces /var/log)
│   ├── fonts/
│   ├── themes/
│   │   ├── dark/
│   │   └── light/
│   └── nexos/              NexOS core binaries
│       ├── bin/            nexos-shell, hardware-detect, etc.
│       ├── lib/
│       └── share/
│
├── kernel/                 Boot and kernel files
│   ├── boot/               GRUB2 config, vmlinuz, initramfs
│   ├── modules/            Linux kernel modules (.ko)
│   ├── seL4/               seL4 binaries + verified boot keys
│   └── proc → /proc        Symlink to live kernel interface
│
├── update/                 Update staging area
│   ├── pending/            Updates waiting to apply
│   └── downloaded/         Downloaded, not yet staged
│
└── emulation/              App compatibility layers
├── linux/              Full FHS — Linux apps live here
│   ├── usr/
│   │   ├── bin/        All Linux binaries (Firefox, VLC, etc.)
│   │   ├── lib/
│   │   ├── share/
│   │   └── local/
│   ├── lib/
│   ├── lib64 → lib
│   ├── etc/            Linux app config files
│   ├── var/
│   ├── opt/
│   ├── bin → usr/bin   FHS compatibility symlink
│   └── sbin → usr/bin  FHS compatibility symlink
│
├── android/            Waydroid Android runtime
│   ├── system/         AOSP system image
│   ├── data/           App data
│   └── sdcard → /users/<username>/downloads/android/
│
├── windows/            Wine + Proton prefix
│   ├── drive\_c/        C:\\ equivalent
│   │   ├── Program Files/
│   │   ├── Windows/
│   │   └── Users/
│   └── proton/         Proton GE runtime
│
└── retro/              RetroArch
├── cores/          Emulator core libraries (.so)
│   ├── snes9x\_libretro.so
│   ├── mgba\_libretro.so
│   ├── pcsx\_rearmed\_libretro.so
│   ├── mupen64plus\_libretro.so
│   └── ... (40+ cores available)
├── roms/
│   ├── snes/
│   ├── gba/
│   ├── gbc/
│   ├── gb/
│   ├── nes/
│   ├── genesis/
│   ├── mastersystem/
│   ├── ps1/
│   ├── n64/
│   ├── atari/
│   ├── amiga/
│   ├── dos/
│   └── arcade/
├── saves/
└── config/

FHS compatibility symlinks (for apt and legacy tools):
/home    → /users
/etc     → /system/config
/var/log → /system/logs
/opt     → /applications
/usr     → /emulation/linux/usr
/lib     → /emulation/linux/lib
/lib64   → /emulation/linux/lib64

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 4\. HARDWARE TIERS

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Detected by installer hardware-detect.sh.
Written to /hardware/tier and /hardware/tier\_desc.
Determines which packages are installed and which
services run. Lower-tier devices are valid NexOS devices
with a reduced feature set — not degraded versions.

T5 — Server / Headless
RAM: any | GPU: absent | Display: absent
Use: servers, build machines, mesh compute nodes
Has: CLI, SSH, apt/nala, intent engine (text only)
No:  GUI, STT/TTS, Waydroid, Wine, RetroArch, N-ring

T4 — Desktop / Laptop  ← primary development target
RAM: 4GB+ | GPU: present | Display: present
Use: daily driver workstation, gaming, creativity
Has: full stack — everything
Examples: Dell 3420, Lenovo Miix 520, custom desktops

T3 — SBC / Phone
RAM: 1-4GB | GPU: optional | Display: optional
Use: portable devices, single-board computers
Has: lightweight GUI, STT (small model), basic apps
No:  Waydroid, Wine, RetroArch
Examples: Raspberry Pi 4, OSM Phone

T2 — Embedded
RAM: 256MB-1GB | GPU: no | Display: no
Use: industrial, IoT, dedicated appliances
Has: CLI only, minimal BusyBox, SSH
No:  STT/TTS, GUI, compat layers

T1 — 



━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 5\. INSTALLER

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Installer sequence:

1. Boot installer ISO
→ NexOS ASCII logo + version
→ "NexOS Installer"
2. Language and keyboard layout selection
3. User creation
→ "Your name:" (display name, e.g. Daniel)
→ "Choose a username:" (lowercase, no spaces)
→ "Choose a password:" (hidden, confirmed)
→ "Confirm password:"
4. Disk selection
→ List available disks with sizes and labels
→ Confirm: "Install NexOS on /dev/sda? (all data will be lost)"
→ Partitioning:
/dev/sda1  512MB   EFI partition (FAT32)
/dev/sda2  remainder  NexOS root (ext4 or btrfs)
5. Hardware scan
→ Runs hardware-detect.sh with live output
→ Determines tier (T2-T5)
→ Shows: CPU, RAM, GPU, battery, network adapters found
→ "Hardware tier: T4 — full stack"
6. Base system installation
→ Writes NexOS filesystem layout to disk
→ Installs base packages (bash, busybox, openrc, nala)
→ Creates /users/<username>/ with standard folders
→ Writes /hardware/ from scan results
7. Tier package installation
→ Downloads and installs tier-appropriate packages
→ Shows progress per package group
→ T4 example:
\[✓] Core system
\[✓] Network tools
\[✓] Audio (PipeWire)
\[✓] Display (XLibre + Qtile)
\[✓] Voice (Vosk + Piper)
\[✓] Wine / Proton
\[ ] Waydroid (optional, ask user)
\[ ] RetroArch (optional, ask user)
8. Bootloader
→ GRUB2 written to disk
→ EFI entry created
→ Legacy BIOS fallback written if no EFI
9. Complete
→ "NexOS installed successfully."
→ "Remove installation media and press Enter to reboot."
→ Reboot → boots installed NexOS

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 6\. INSTALLED BOOT SEQUENCE

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

GRUB2
→ NexOS boot splash (minimal, fast)
→ seL4 microkernel loads
• Capability tree initialised
• Verified boot check (signature validation)
• Spawns Linux hardware VM
→ Linux VM boots (\~300ms)
• Minimal kernel, no desktop, no systemd
• OpenRC starts hardware services
• Drivers load: GPU, WiFi, audio, storage
• /dev, /proc, /sys become available
→ NexOS filesystem mounted
• /hardware/ refreshed by nexos-hardware-detect
• /connections/ populated from /dev
• FHS symlinks verified
→ OpenRC starts NexOS services
• nexos-network (NetworkManager)
• nexos-audio (PipeWire)
• nexos-display (XLibre + Qtile, T4 only)
• nexos-wakeword (T4 only)
→ Login prompt (or auto-login if configured)
→ Bash shell — NexOS prompt
dan@nexos \~ $

Total boot time target: under 8 seconds to prompt (T4)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 7\. COMPATIBILITY LAYERS

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

The RetroArch model: load a core, run the app.
No OS boots. No waiting. Switch between environments
the way RetroArch switches between SNES and PS1.

LINUX APPS
Method:   Linux mount namespace (unshare --mount)
Root:     /emulation/linux/
Launch:   \~0.2s — no boot, just namespace setup
Perf:     Native — no emulation, x86 on x86
Isolation: Mount namespace only. Cannot see /users/
by default. Explicit permission required.
Install:  nala install <package>
→ installs to /emulation/linux/
Display:  XLibre/Wayland → appears as NexOS window
Examples: Firefox, VLC, GIMP, Spotify, VSCode

WINDOWS APPS
Method:   Wine 9.x + Proton GE
Prefix:   /emulation/windows/
Launch:   \~1s first run (Wine init), fast after
Perf:     Native x86 execution — not emulation
Win32 API calls translated to POSIX
DXVK translates DirectX to Vulkan
Isolation: Sandboxed in /emulation/windows/
No access to /users/ without permission
Install:  nexos-install <setup.exe>
or: Steam with Proton (recommended for games)
Examples: Half Life 2 (\~5s to in-game), MS Office,
Photoshop, any Steam game with Proton rating

ANDROID APPS
Method:   Waydroid (AOSP in Linux container)
Root:     /emulation/android/
Launch:   \~2s cold container start, then instant
Perf:     Near-native — uses host Linux kernel
via Android kernel interface shim
Isolation: Waydroid container. No host access.
Install:  waydroid app install <app.apk>
or via Aurora Store (Google Play frontend)
Examples: Any Android app or game

RETRO GAMES
Method:   RetroArch + libretro cores
Cores:    /emulation/retro/cores/ (.so libraries)
ROMs:     /emulation/retro/roms/<system>/
Launch:   Instant — core is loaded as a library
Switching systems: 1-2 seconds
Systems:  SNES, GBA, GBC, GB, NES, Genesis,
Master System, PS1, N64, Atari 2600/7800,
Amiga, DOS (DOSBox), Arcade (MAME/FBNeo)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 8\. INTENT ENGINE

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Voice, touch, keyboard, and gaze all feed into one
intent pipeline. The OS figures out what you mean.

Activation triggers (any of these wakes the N-ring):
• Wake word: configurable, default off
• Hotkey:    Super+Space (configurable)
• Touch:     Long press or swipe up on N-ring pip
• Physical:  Hardware button (OSM Phone side button)
• Gaze:      800ms dwell on N-ring pip (when enabled)

Intent pipeline:
Trigger
→ N-ring appears (instant — already in memory)
→ STT loads (Vosk, on demand, \~1s first time)
→ User speaks / types / touches
→ Local rule resolver (instant, handles \~90% of cases)
→ Ollama LLM (on demand, for ambiguous input only)
→ Action dispatched
→ N-ring dismisses
→ STT + LLM unload after 30s idle

STT: Vosk (offline, single model for wake word + transcription)
Small model: vosk-model-small-en (\~50MB) — T3/T4 base
Large model: vosk-model-en (\~1.8GB) — T4 optional

TTS: Piper (offline, natural voice)
Model: en\_US-lessac-medium (\~63MB)
Greeting: "Good morning, <name>. NexOS is ready."

Intent resolution:
Layer 1: Rule-based regex (instant, no model required)
Handles: play, open, connect, volume, navigation
Layer 2: Ollama phi3:mini (2.3GB, on demand)
Handles: complex, ambiguous, conversational

RAM usage:
Idle:   \~2MB (wake word detector only)
Active: \~800MB (STT + LLM loaded)
After:  \~2MB (models evicted after 30s idle)

Examples:
"play foo fighters outside"
→ media\_play {artist: "Foo Fighters", track: "Outside"}
→ searches /users/<username>/music/ → plays file

"connect to MyNetwork"
→ sys\_wifi {ssid: "MyNetwork", action: "connect"}
→ nmcli connection up MyNetwork
→ if new: TTS asks for password → onscreen keyboard

"i want to play half life 2"
→ game\_launch {title: "Half Life 2"}
→ checks /applications/ → checks Steam + Proton
→ launches via Proton

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 9\. N-RING

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

The single interaction point for NexOS.
Always centred on screen. Activates from any input type.

States:
IDLE      — small N pip, minimal CPU, waits for trigger
ACTIVE    — expanding ring animation
LISTENING — mic active, spoken words appear inside ring
radial quick-action menu visible around ring
THINKING  — spinner, intent being resolved
RESULT    — icon + status text, auto-dismisses
INPUT     — keyboard typing mode, text shown in ring

Inside the ring:
• Spoken or typed words appear word by word
• Text wraps to fill the circle
• Words fade from teal → dim → invisible over \~3 seconds
• Newest words always brightest

Radial menu (6 items around the ring):
🎵 Music    📁 Files    🌐 Browser
⚙  Settings  🎮 Games    🔍 Search

Implementation: Qt5, Python, \~600 lines
Runs on: Linux (X11/Wayland), Windows, macOS

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 10\. SECURITY MODEL

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

seL4 capabilities:
Every process has explicit capabilities for every resource
it may access. No capability = no access, provably.
A compromised process cannot escalate beyond its
capability set. This is mathematically verified.

Passphrase system:
Two levels: session passphrase + system passphrase
Format: word-number-word-number
Example: tiger4cloud7 (digits 0-9 only)
Storage: seL4 secure enclave — never in Linux VM
Never written to disk in plaintext

Compat layer isolation:
Linux apps:   mount namespace — cannot see /users/
by default. Access is explicitly granted.
Windows apps: Wine prefix sandbox in /emulation/windows/
Android apps: Waydroid container — no host filesystem
All layers:   seL4 capability boundary between them
A compromised Wine process cannot touch
Waydroid data or NexOS system files

User data:
/users/<username>/ is never accessible to compat
layers without explicit user permission (like Android
storage permissions but for every app type)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 11\. CORE SERVICES

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

All managed by OpenRC. Service files in /system/services/

Service                  Tier   Description
───────────────────────────────────────────────────────
nexos-hardware-detect    all    Runs at boot, populates
/hardware/ and /connections/

nexos-network            all    NetworkManager wrapper
WiFi, ethernet, VPN

nexos-audio              T3+    PipeWire audio server
Replaces PulseAudio

nexos-display            T4+    XLibre + Qtile WM
Starts after login

nexos-wakeword           T4+    Always-on wake word
\~2MB RAM, \~1% CPU

nexos-intent             T4+    Intent engine daemon
Loads models on demand
Unloads after 30s idle

nexos-tts                T3+    Piper TTS
Speaks on first activation

nexos-waydroid           T4     Android container
Started on first Android
app launch, not at boot

nexos-update             all    Background update checker
Downloads to /update/pending/
Applies on next boot

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 12\. PACKAGE MANIFESTS

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Packages installed per tier by the installer.
All packages go into /emulation/linux/ via apt/nala.

BASE (installed on all tiers):
bash busybox openrc nala apt dpkg
curl wget git openssh-server
htop tmux nano
networkmanager network-manager-tui
ca-certificates gnupg2

T5 adds:
fail2ban ufw
build-essential gcc g++ make
python3 python3-pip

T4 adds:

# Display

xlibre-server xinit
python3-pyqt5 python3-qtile

# Audio

pipewire pipewire-pulse wireplumber
alsa-utils

# Voice

vosk-model-small-en
piper piper-en-lessac-medium
python3-vosk python3-sounddevice

# Compat layers

wine wine64
proton-ge
waydroid              (optional — installer asks)
retroarch             (optional — installer asks)

# NexOS apps

nexos-nring
nexos-settings
nexos-shell

T3 adds:
pipewire pipewire-pulse
python3-vosk vosk-model-small-en
piper piper-en-lessac-medium
lightweight compositor (labwc or openbox)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 13\. REPO STRUCTURE

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

github.com/DansDesigns/NexOS (monorepo)

nexos/
├── build/                  ISO builder
│   ├── config/             live-build configuration tree
│   │   ├── archives/       apt sources for installer
│   │   ├── hooks/          pre/post install scripts
│   │   ├── includes.chroot/ files copied into ISO root
│   │   ├── package-lists/  per-tier package manifests
│   │   └── preseed/        automated install answers
│   ├── Makefile            make iso|qemu|clean|burn
│   └── README.md
│
├── installer/              Installer scripts (run once)
│   ├── install.sh          Main installer entrypoint
│   ├── hardware-detect.sh  Hardware scanner + tier detect
│   ├── first-run-setup.sh  User creation + disk format
│   ├── package-lists/      Tier package manifests
│   │   ├── base.list
│   │   ├── t4.list
│   │   ├── t3.list
│   │   └── t5.list
│   └── README.md
│
├── init/                   Boot and init files
│   ├── services/           OpenRC .initd service files
│   │   ├── nexos-hardware-detect
│   │   ├── nexos-network
│   │   ├── nexos-audio
│   │   ├── nexos-display
│   │   ├── nexos-wakeword
│   │   └── nexos-intent
│   ├── profile.d/          Shell profile additions
│   │   └── nexos.sh        PATH, aliases (apt→nala)
│   └── README.md
│
├── system/                 NexOS system files
│   ├── filesystem/         Directory structure creator
│   │   └── create-layout.sh
│   ├── hardware-detect.sh  Hardware scanner (also in installer)
│   └── README.md
│
├── shell/                  NexOS C++ CLI shell
│   ├── src/
│   ├── include/
│   ├── Makefile
│   └── README.md
│
├── nring/                  N-ring Qt5 overlay
│   ├── nring.py
│   ├── vosk\_stt.py
│   ├── intent\_client.py
│   ├── nexos\_config.py
│   ├── setup.py
│   └── README.md
│
├── intent/                 Intent engine
│   ├── engine/
│   │   ├── intents.py
│   │   ├── intent\_engine.py
│   │   └── actions.py
│   ├── wakeword/
│   │   └── wakeword.py
│   └── README.md
│
├── apps/                   NexOS native applications
│   ├── nexos-settings/     Settings app
│   ├── nexos-music/        Music player
│   └── nexos-files/        File browser
│
├── branding/               Logos, wallpapers, themes
│   ├── logo/
│   ├── wallpapers/
│   └── themes/
│       ├── dark/
│       └── light/
│
├── docs/                   Documentation
│   ├── SPEC.md             This document
│   ├── BUILDING.md         How to build the ISO
│   ├── INSTALLING.md       How to install NexOS
│   └── CONTRIBUTING.md
│
└── README.md               Project overview

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 14\. BUILD PHASES

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Phase 1 — Foundation CLI                        ← CURRENT
Bootable ISO with NexOS filesystem + Bash CLI
seL4 boot, Linux VM, OpenRC, BusyBox + Bash
Hardware detection, NexOS filesystem layout
Installer: user creation, disk write, packages
Target: boots to Bash prompt on real hardware
Test: QEMU first, then Miix 520 + Dell 3420

Phase 2 — Compatibility Layers
Wine + Proton (Windows apps)
Linux namespace isolation (Linux apps)
Waydroid (Android apps)
RetroArch (retro gaming)
Target: launch apps from all four layers

Phase 3 — GUI + Voice
XLibre + Qtile
N-ring overlay
Intent engine (voice + text)
STT/TTS (Vosk + Piper)
Target: full voice-controlled desktop

Phase 4 — seL4 Hardening
seL4 VMM hosting Linux hardware domain
Verified boot with seL4 signature check
Capability isolation between compat layers
Secure credential enclave
Target: production security model

Phase 5 — Polish + Ports
ARM64 (Raspberry Pi, OSM Phone)
NexOS native apps (settings, music, files)
Eye tracking (MediaPipe gaze estimation)
NexOS app store
Target: daily driver for real users

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 15\. DESIGN PRINCIPLES

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Security is non-negotiable
seL4's formal verification is the foundation.
Every design decision respects its capability model.
2. The filesystem is for humans
/users/dan/music not /home/dan/.local/share/rhythmbox/music
A person who has never used Linux can navigate it.
3. One interaction point
Voice, touch, keyboard, gaze — all go through the N-ring.
Not separate systems bolted together. One pipeline.
4. Nothing boots if you don't need it
Intent engine loads on demand. Waydroid starts on first
Android app. Models unload after idle. Battery matters.
5. Compat layers are not second-class
Windows apps, Android apps, and Linux apps are all
first-class. The user should not need to know or care
which layer an app uses.
6. Developer and noob friendly — same OS
The Bash CLI is the foundation. The GUI is a layer on top.
A developer can ignore the GUI entirely. A noob never
needs to touch the CLI. Both are valid uses.
7. Modular
Every component can be replaced or removed.
OpenRC → replace init. Qtile → replace WM.
Vosk → replace STT. Nothing is load-bearing except seL4.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 16\. BRANDING

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Name:       NexOS
Tagline:    "One OS. Any machine. Your rules."
Owner:      DansDesigns / AlterniTech
Website:    alternitech.square.site
Repo:       github.com/DansDesigns/NexOS

Palette:
Night:    #0C1210    primary background
Teal:     #3EE8C4    accent, interactive, voice
Copper:   #B87040    labels, secondary
Gold:     #D4904A    highlights
White:    #D4EDE8    primary text

Themes:
Dark:   Night background, teal accents, copper labels
Light:  Cream (#F5F0E8) background, dark text,
amber accents — matches GUI prototype

