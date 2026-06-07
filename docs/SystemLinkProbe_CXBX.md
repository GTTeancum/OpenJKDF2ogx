# OpenJKDF2 Xbox System Link Probe

This is a lightweight LAN discovery probe, not gameplay networking. It answers
one question: can two Xbox/CXBX-R instances see each other over UDP broadcast?

## Behavior

- Opens a UDP socket when entering `Multiplayer -> System Link`.
- Binds the first available port from `9777` through `9780`.
- Broadcasts `JKXSL1|<local-id>|<local-port>|<counter>` once per second to all
  four probe ports.
- Lists other instances that send the same probe packet.
- Stops and closes the socket when backing out of the screen.

## Log Markers

The Xbox log at `D:\debug_openjkdf2.txt` should show:

```text
XSL probe net init xnet=0 wsa=0 ready=1
XSL probe started id=0x12345678 port=9777
XSL peer discovered id=0x87654321 addr=192.168.x.x port=9778
XSL probe status id=0x12345678 port=9777 peers=1 sent=10 lastErr=0
XSL probe stopped
```

## CXBX-R Setup

Use two separate game folders, not the exact same folder twice. Launch two
emulator instances, enter `Multiplayer -> System Link` in both, and expect each
screen to show one peer with an increasing packet count.

This intentionally bypasses Xbox secure networking. Real System Link gameplay
still needs the secure-session path and integration with Jedi Knight's network
session flow.
