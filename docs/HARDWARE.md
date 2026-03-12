# Hardware Guide

## Bill of Materials

- ESP32-C3 SuperMini-style board
- WS2812B 8x8 LED matrix
- MPU6050 breakout
- external 5V supply sized for LED current
- 330-470 ohm resistor on LED data line
- 1000 uF or larger electrolytic capacitor across LED 5V and GND
- logic level shifter for LED data line

## GPIO Recommendations

- `GPIO7` -> WS2812B DIN
- `GPIO5` -> MPU6050 SDA
- `GPIO6` -> MPU6050 SCL

These pins are chosen to avoid the most common USB and boot-sensitive paths on ESP32-C3 boards. SuperMini clones vary, so verify the exact pin map on your board before soldering.

## Power Notes

- Do not power the 8x8 matrix from the ESP32-C3 onboard regulator.
- Tie the grounds of the ESP32, LED power supply, and MPU6050 together.
- Start with a conservative brightness limit during bring-up.
- Use a level shifter for reliable WS2812 signaling when the matrix is powered from 5V.
- If you are temporarily powering the matrix from the onboard regulator anyway, keep brightness very low. This firmware now hard-caps output to `16/255`.

Worst-case LED current for 64 WS2812B pixels at full white can exceed 3A. This firmware intentionally defaults to a low brightness cap to avoid brownouts and overheating during development.

## Suggested Wiring

```text
5V PSU + ----+-----------------> LED matrix 5V
            |
            +-----------------> level shifter HV

GND -------+------------------> LED matrix GND
           +------------------> ESP32 GND
           +------------------> MPU6050 GND
           +------------------> level shifter GND

ESP32 GPIO7 ----[330R]----> level shifter LV input -> HV output -> LED DIN
ESP32 GPIO5 ----------------> MPU6050 SDA
ESP32 GPIO6 ----------------> MPU6050 SCL
ESP32 3V3 ------------------> MPU6050 VCC
```

## Bring-Up Order

1. Validate ESP32-C3 flashing and serial output before attaching LEDs.
2. Validate I2C detection of the MPU6050.
3. Validate a low-brightness matrix test pattern.
4. Only then raise brightness or add external effects.
