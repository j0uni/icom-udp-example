# ICOM UDP example

Minimal standalone C++ program that demonstrates the ICOM Wi‑Fi remote protocol over UDP.

The client:

1. Opens **three UDP sockets** (control, CI‑V, audio)
2. Completes **control login** (discover → login → token → connection info → status)
3. Opens **CI‑V** and **audio** streams on ports returned by the radio
4. Prints **operating frequency** (CI‑V read `0x03`) and **audio RX statistics**

## Requirements

- C++17 compiler (clang or g++)
- CMake 3.14+
- ICOM radio with Wi‑Fi remote enabled (e.g. IC‑705, IC‑7300 with WLAN)
- Computer on the same network as the radio

## Build

```bash
git clone <repository-url>
cd icom_udp_example
cmake -B build
cmake --build build
```

## Run

```bash
./build/icom_udp_example <radio_ip> [control_port] [user] [password]
```

Examples:

```bash
./build/icom_udp_example 192.168.1.50
./build/icom_udp_example 192.168.1.50 50001 icom yourpassword
```

Defaults: control port **50001**, user/password **icom** / **icom** (change to match the radio’s Remote Settings).

The program runs for up to **90 seconds**, polling frequency every second and printing audio packet counts every 5 seconds once streams are up.

## Expected output (success)

```
ICOM UDP example
  Radio: 192.168.1.50:50001
  Local ports: control=xxxxx civ=xxxxx audio=xxxxx
[control] I_AM_HERE remoteId=0x...
[control] I_AM_READY
[control] Sent login (0x80)
[control] Login OK, rigToken=0x...
[control] Sent token confirm + renewal
[control] Sent connection request (0x90)
[control] ConnInfo rig='IC-705' busy=0
[control] Status OK — rig CI-V port=xxxxx audio port=xxxxx
[civ] I_AM_READY
[civ] Open CI-V stream
[audio] I_AM_READY (via common handler)
[civ] Set frequency to 3.699 MHz (3699000 Hz)
[civ] Frequency: 3.699 MHz (3699000 Hz)
[status] freq=3.699 MHz (3699000 Hz) | audio RX packets=... bytes=...
```

Exit code **0** = handshake completed; **2** = timeout (no radio or wrong credentials).

## Notes

- Educational minimal example, not a supported product. No TX audio, no reconnection UI.
- CI‑V address defaults to **0xA4** until a **0xA8** capabilities packet updates it.
- Retransmit handling is simplified (small sequence cache only).
- Linux and macOS only (`udp_socket.hpp` uses BSD sockets).

## Files

| File | Purpose |
|------|---------|
| `main.cpp` | State machine and poll loop |
| `icom_protocol.hpp` | Packet build/parse helpers |
| `udp_socket.hpp` | UDP bind/send/recv |
| `CMakeLists.txt` | Build configuration |

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE). Copyright © 2026 **Jouni OH3CUF**.

Portions of the ICOM Wi‑Fi protocol handling are derived from [FT8CN](https://github.com/N0BOY/FT8CN) (`IComPacketTypes` and related code), Copyright © 2023 **BG7YOZ**, MIT License. The full third-party notice is in [NOTICE](NOTICE).
