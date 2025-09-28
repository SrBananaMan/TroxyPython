
![Terraria Proxy Running in a-Shell](ios.png)
*Terraria Proxy running in a-Shell on iOS, showing server status and broadcast logs.*

# Terraria Proxy

A lightweight Python script to proxy Terraria server connections, enabling Xbox consoles to discover and join a public or local Terraria server as if it were on the local network. This allows cross-platform play (Xbox with PC/mobile) and allows the Xbox player to exit sessions without disconnecting others. Forked from Troxy, it runs on PC, iOS (via a-Shell), or Android (via Pydroid 3) with minimal setup.

## Features

- **Local Server Spoofing:** Broadcasts UDP packets on port 8888 to make a remote server (e.g., `t.dark-gaming.com:7777`) appear local to Xbox.
- **Cross-Platform Play:** Xbox joins via local discovery; PC/mobile players join via public IP.
- **No Computer Required:** Runs on iOS (a-Shell with one-tap Shortcut) or Android (Pydroid 3).

## Requirements

- **Python 3.8+:** Uses built-in modules only (`socket`, `threading`, `time`, `json`).
- **Supported Platforms:**
  - PC: Windows, Linux, macOS
  - iOS: a-Shell (free, App Store)
  - Android: Pydroid 3 (free, Google Play)
- **Network:** Proxy device must be on the same Wi-Fi as the Xbox (e.g., `192.168.1.x`).

## Installation

### Download the Script
1. Clone this repository or download `Troxy.py`.
2. Save to a local directory (e.g., `~/Documents` for a-Shell).

## Usage

### Running on PC
1. Install Python 3.8+ from [python.org](https://www.python.org).
2. Open a terminal in the script’s directory.
3. Run:
   ```bash
   python Troxy.py
   ```
4. If using prompts, enter:
   - Server IP (e.g., `t.dark-gaming.com`)
   - Port (e.g., `7777`)

### Running on iOS (a-Shell)
1. Install **a-Shell** (free, [App Store](https://apps.apple.com/us/app/a-shell/id1476949236)).
2. Copy `Troxy.py` to a-Shell’s `~/Documents`:
   - In a-Shell, type `open .` to access Files app, then copy the script to Documents.
   - Or create: `nano Troxy.py`, paste script, save (`Ctrl+X`, `Y`, Enter).
3. Navigate to the script:
   ```bash
   cd ~/Documents
   ```
4. Set permissions:
   ```bash
   chmod +x Troxy.py
   ```
5. Run:
   ```bash
   python Troxy.py
   ```
6. Keep a-Shell open to maintain the proxy.

### Running on Android (Pydroid 3)
1. Install **Pydroid 3** (free, [Google Play](https://play.google.com/store/apps/details?id=ru.iiec.pydroid3)).
2. Create or save `Troxy.py` in Pydroid’s editor.
3. Tap the Run button (▶️).
4. Keep Pydroid open or use “Run in Terminal” for semi-background mode.

### Testing the Proxy
- **Xbox:** Open Terraria > Multiplayer > Join Game. Select “MyWorld” (or your custom name), enter password.
- **Remote Players:** Join via server IP (e.g., `t.dark-gaming.com:7777`) on PC/mobile.

## Troubleshooting

### Xbox Doesn’t See Server
- Ensure proxy device is on the same Wi-Fi as Xbox (`192.168.1.x`).
- Check logs for “Broadcast error” or “Server unreachable.”
- Run proxy on a separate device if server is on the same PC (port 7777 conflict).

### Port Conflict
- If logs show “Cannot bind TCP port 7777”:
  - Edit `CONFIG['port_7777'] = 7778` or use a different device.
- Separate device recommended for best reliability.

### iOS Proxy Pauses
- Keep a-Shell open to avoid iOS pausing the app.
- Enable Guided Access (Settings > Accessibility > Guided Access) to lock a-Shell.

### Friend Can’t Join
- Test server reachability: `telnet t.dark-gaming.com 7777`.
- Ensure both platforms are on the same version

## Limitations

- **Nintendo Switch:** Incompatible due to Nintendo Switch Online (NSO) restrictions, which prevent local proxy discovery. Use mobile-hosted crossplay instead.
- **iOS Background:** a-Shell pauses if minimized; keep open or use Guided Access.

## Acknowledgments

- Inspired by **Troxy** and community ESP32-based proxies.
- Built on **TShock** and Terraria server protocols.
