# Developer Guide

## Architecture

The firmware is split into five layers:

- `app_core`: shared types, state, event processing, and mode control
- `config_store`: persisted settings in NVS
- `drivers`: MPU6050, Wi-Fi portal, and LED matrix access
- `effects`: LED animation registry and render implementations
- `providers`: external context providers for future internet-backed effects

The main loop creates a shared queue of `app_event_t`. Drivers push events into this queue, and the controller consumes them to update `app_state_t`. Rendering reads a snapshot of that state and asks the active effect to populate the framebuffer.

## Add a New Effect

1. Implement the callbacks described by `app_effect_t` in [components/effects/include/effects.h](/c:/workspaces/led-array/components/effects/include/effects.h).
2. Append the new effect to the registry in [components/effects/effects.c](/c:/workspaces/led-array/components/effects/effects.c).
3. Use `effect_update()` to render pixels into `rgb_pixel_t framebuffer[64]`.
4. Read live values from `app_state_t` instead of talking to drivers directly.
5. If the effect needs one-shot triggers, react in `effect_on_event()`.

## Add a New Provider

1. Implement the `app_provider_t` interface in [components/providers/include/providers.h](/c:/workspaces/led-array/components/providers/include/providers.h).
2. Register the provider in `provider_manager_init()`.
3. Store fetched data in `provider_snapshot_t`.
4. Emit `APP_EVENT_PROVIDER_UPDATED` or `APP_EVENT_PROVIDER_ERROR` when the snapshot changes or fails.

The base provider is intentionally local and deterministic so the rest of the system can be developed without internet dependencies.

## Event Flow

- MPU6050 task emits tilt changes and shake events.
- Wi-Fi manager emits connection state changes.
- Provider manager emits update and error events.
- Controller consumes those events and updates mode, connectivity flags, and sensor state.
- Effects consume the current state every render tick and may also receive direct events for transient behavior.

## Matrix Mapping

Use logical coordinates `(x, y)` with origin at the top left. Physical LED addressing is abstracted by `matrix_map_xy()`, which applies rotation, serpentine layout transform, and bounds checks.

## Calibration

The MPU6050 driver applies configured accelerometer offsets and threshold values from `app_config_t`. Future contributors can extend this to interactive calibration without changing the higher-level event model.
