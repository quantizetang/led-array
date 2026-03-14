# ESP32-C3 LED Array Platform

ESP-IDF firmware base for an ESP32-C3 SuperMini-style board driving:

- a WS2812B 8x8 LED matrix
- an MPU6050 accelerometer/gyro
- Wi-Fi onboarding through captive portal fallback

The project is structured as a reusable platform rather than a one-off sketch. Core behaviors are already wired together, and new effects or data providers can be added without changing the app loop.

## Features

- Modular components for control, drivers, effects, providers, and config
- Event-driven input handling for tilt, shake, and Wi-Fi events
- 8x8 matrix abstraction with serpentine and rotation-aware mapping
- Default effects:
  - static color
  - swirl
  - tilt-reactive gradient
  - motion sparkle
  - provider-reactive placeholder
- Wi-Fi station connect using saved credentials
- SoftAP fallback with a minimal captive portal and config form
- NVS-backed settings for Wi-Fi, display, and motion tuning
- Provider interface for future internet-backed inputs such as weather

## Recommended Wiring

Baseline GPIOs for an ESP32-C3 SuperMini-style board:

- WS2812B data in: `GPIO7`
- MPU6050 SDA: `GPIO5`
- MPU6050 SCL: `GPIO6`

See [docs/HARDWARE.md](/c:/workspaces/led-array/docs/HARDWARE.md) for power and safety requirements.

## Build

1. Install ESP-IDF 5.x and export the environment.
2. Set the target:

```bash
idf.py set-target esp32c3
```

3. Build and flash:

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

This workspace does not currently have `idf.py` installed in the shell used for implementation, so the code was prepared against standard ESP-IDF APIs but not compiled locally here.

## Runtime Behavior

- On boot, the device loads config from NVS.
- The current debug build forces `AP-only` mode so the captive portal hotspot always starts at boot.
- If stored Wi-Fi credentials are valid, it joins the configured network as a station.
- If connection fails or no credentials are stored, it starts a SoftAP and captive portal.
- Tilt and shake data influence effect parameters through the event bus.

## Captive Portal

When fallback AP mode is active, connect to the device AP and open any URL. The DNS responder points clients to the portal page served from the ESP32.

Default AP settings:

- SSID: `LED-Array-Setup`
- Password: `ledarray123`
- Portal address: `http://192.168.4.1/`

## Project Layout

- [main/app_main.c](/c:/workspaces/led-array/main/app_main.c)
- [components/app_core](/c:/workspaces/led-array/components/app_core)
- [components/config_store](/c:/workspaces/led-array/components/config_store)
- [components/drivers](/c:/workspaces/led-array/components/drivers)
- [components/effects](/c:/workspaces/led-array/components/effects)
- [components/providers](/c:/workspaces/led-array/components/providers)
- [docs/DEVELOPER_GUIDE.md](/c:/workspaces/led-array/docs/DEVELOPER_GUIDE.md)
- [docs/CONFIGURATION.md](/c:/workspaces/led-array/docs/CONFIGURATION.md)

## Extension Points

- Add a new LED effect by registering a new `app_effect_t`
- Add a new provider by implementing `app_provider_t`
- Add more config fields in `app_config_t` and persist them through `config_store`
- Add more event types if new sensors or automation rules are required
