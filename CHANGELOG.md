**[README](README.md)** > **Changelog** · [Report an issue](../../issues/new)

# Changelog

All notable changes to this firmware. Versions follow [semver](https://semver.org/); the Matter software version integer for each release is noted since OTA uses it.

## v0.6.0 - Sensor fault reporting, wet-button guard & optional 2-slope curve

*Software version `0x00000600`*

- **Added:** Sensor Fault detection and Matter `Null` reporting. Probe readings below `SOIL_FAULT_MIN_MV` (default 100 mV) or failed ADC reads now report `Null` (`Unknown` in Home Assistant) instead of clamping to 100% or leaving stale values, and flash 3 fast red blinks on a manual press. Stored NVS calibration (`soilcal`) is also sanity-checked at boot.
- **Added:** CN4 wet-button guard. The LIT ICD wake and single-press sampling are each rate-limited by `SOIL_BUTTON_ICD_COOLDOWN_SECONDS` (default 5 s, matching the ICD active mode threshold), and triple-press calibration by `SOIL_BUTTON_CAL_COOLDOWN_SECONDS` (default 60 s), so a wet or chattering button cannot hold the Thread radio awake, resample with LED blinks, or restart calibration repeatedly. The 10 s factory-reset hold is not rate-limited.
- **Fixed:** battery reports no longer fire on every percent change. Steep chemistry curves (Lithium, NiMH) turned a few mV of ADC noise into percent changes that woke the radio almost every sample. Reports now trigger on a 20 mV voltage change or a charge level / replacement flag change, and the percent-based Warning (20%) and Critical (10%) levels only clear once the percent recovers 5 points above the threshold and the cell voltage rises 50 mV above its lowest reading since the level was entered (in practice, a cell swap), so temperature drift near a curve knee no longer toggles the level daily.
- **Added:** optional `SOIL_MOISTURE_CURVE_SEGMENTED` (2-slope, 850 permille -> 30%) in `menuconfig`, keeping `Linear` as the default so the 23% / 58% LED thresholds remain unchanged.

## v0.5.0 - Accuracy, power tuning & chemistry-aware battery

*Software version `0x00000500`*

- **Improved:** measurement accuracy. Both ADC channels now discard 2 dummy reads after switching channels and use an interquartile trimmed mean (16 reads for soil, 9 for battery) with half-up rounding. `soil_probe` holds `ESP_PM_APB_FREQ_MAX` and uses `LEDC_USE_PLL_DIV_CLK` so the 200 kHz excitation PWM stays frequency-locked under CPU DFS. `sample_task` now reads the rested battery voltage before exciting the soil probe, and calibration rejects unstable readings exceeding `SOIL_CAL_MAX_SPREAD_MV` (default 60 mV).
- **Added:** selectable `SOIL_BATTERY_CHEMISTRY` profiles (`Alkaline` default, `NiMH`, `Lithium` Li-FeS2 fitted to field data, or `Linear`). Note: switching the default from Linear to the Alkaline piecewise curve shifts the 10% Critical threshold from ~1.05 V to 1.10 V.
- **Changed:** default sample interval and LIT ICD idle mode interval raised from 900 s to 1800 s, default `SOIL_REPORT_DELTA_PERCENT` raised from 1% to 2% (manual single-press still reports any 1% change immediately), idle CPU DFS down to `CONFIG_XTAL_FREQ` (40 MHz) via `SOIL_CPU_DFS_IDLE`, and daily LED sag test skipped when battery is `<= 10%`.
- **Fixed:** single-press LED verdict now treats `<= 23%` as dry (red), matching Home Assistant and the stock firmware.

## v0.4.0 - Cell voltage reporting

*Software version `0x00000400`*

- **Added:** cell voltage over Matter. The Power Source cluster now carries `BatVoltage`, the resting measurement in millivolts, updated on a change of 20 mV or more so that ADC jitter does not wake the radio every sample. The battery percentage is a linear voltage map that pins at 100% on a lithium cell and compresses the alkaline curve, and the raw millivolts previously reached only the serial console, which a running battery unit cannot use because connecting USB disables the boost converter and takes the cell out of the load path.
- No change to sampling, power management, button handling, or calibration

## v0.3.0 - Identify fix & toolchain refresh

*Software version `0x00000300`*

- **Fixed:** the Identify button. The soil sensor endpoint advertised its identify type as None; controllers hid or ignored the button even though the LED blink logic was present. It now advertises the status LED as a visible indicator
- **Changed:** esp_matter moved from 1.5.1 to 1.6.0. The 1.6.0 component ships the Soil Sensor endpoint helper and the `SetSoilMoisture` glue, so the `soil_measurement_compat` shim that mirrored them is deleted
- **Changed:** builds pinned to ESP-IDF v5.5.5, up from v5.5.2, in both the dev container and the release workflow
- No change to sampling, power management, button handling, or battery reporting

## v0.2.0 - Battery health & diagnostics

*Software version `0x00000200`*

- **Fixed:** battery operation. On battery the device appeared to die after a couple of button presses. Cause: peripheral power-down during light sleep disabled the button's GPIO wake and the device slept through presses (USB masked the bug because the console blocks light sleep). The peripheral domain now stays powered in light sleep
- **Added:** battery health via sag measurement. The firmware loads the cell with its own LEDs (~daily) and uses voltage sag as an internal-resistance proxy; drives charge level Warning/Critical and `BatReplacementNeeded`
- **Added:** brownout diagnostics. Red ×5 blink at boot after a brownout reset, plus a lifetime counter in NVS printed on the serial console
- **Added:** Thread TX power capped at +10 dBm (configurable) to cut TX battery peaks ~3×

## v0.1.0 - Initial release

*Software version `0x00000100`*

- Matter over Thread firmware for the Seeed Studio XIAO Soil Moisture Sensor (XIAO ESP32-C6)
- Soil Sensor device type (Matter 1.5 SoilMeasurement cluster), sampled locally every 15 min, reported on ≥1% change
- Battery percentage via the Power Source cluster
- Sleepy end device (Thread MTD, LIT ICD), BLE commissioning, multi-fabric, Matter OTA (A/B partitions)
- Button: single press = sample + LED moisture verdict, triple press = LED-guided dry/wet calibration (persisted to NVS), 10 s hold = factory reset

## Related documentation

- [README](README.md) — project overview and quick start
- [Flashing Guide](docs/FLASHING.md) — flashing over USB-C and back to stock
- [Commissioning Guide](docs/COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Calibration Guide](docs/CALIBRATION.md) — LED-guided dry and wet calibration
- [Updating Guide](docs/UPDATING.md) — Matter OTA and USB updates that keep commissioning
- [Power & Battery](docs/POWER.md) — power design decisions and field data
- [Hardware Notes](docs/HARDWARE.md) — pin map, probe drive, battery sensing, antenna
- [Building from Source](docs/BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](docs/TROUBLESHOOTING.md) — LED reference and common issues
- [Roadmap](docs/ROADMAP.md) — known limitations and planned work
- [Contributing](docs/CONTRIBUTING.md) — commit, branch, and release conventions
