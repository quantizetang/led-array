# Configuration Guide

## Persisted Settings

The `app_config_t` structure stores:

- Wi-Fi SSID and password
- fallback AP SSID and password
- LED brightness limit
- matrix rotation
- matrix serpentine mode
- startup effect
- tilt threshold and shake threshold
- accelerometer offsets
- provider enable flag, endpoint placeholder, and API key placeholder

Settings are saved to NVS under the `appcfg` namespace.

## Captive Portal Flow

1. Device boots and tries the saved station credentials.
2. If credentials are missing or the connection attempt fails, SoftAP mode starts.
3. The DNS responder directs requests to `192.168.4.1`.
4. The portal serves a simple HTML form for Wi-Fi credentials.
5. Submitted credentials are saved to NVS and a reconnect attempt begins.

## Manual Recovery

- Erase flash if config becomes unrecoverable:

```bash
idf.py erase-flash
```

## Configuration Defaults

- AP SSID: `LED-Array-Setup`
- AP password: `ledarray123`
- brightness limit: `5 / 255`
- matrix mode: `straight rows`
- startup effect: `swirl`
- tilt threshold: `0.30g`
- shake threshold delta: `0.90g`
