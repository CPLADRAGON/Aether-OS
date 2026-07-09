// Shared lux-level qualitative labeling. `lux_value` is computed on the
// ESP32 (see firmware/src/main.cpp's ldrRawToLux()) from an estimated
// generic-CdS-photoresistor approximation -- not a calibrated lux meter.

// Coarse 3-band tag, matching the firmware's lightLevelTag() thresholds
// exactly (400/30 lux) so a reading reads consistently between the OLED,
// Telegram bot, and any place on the web that wants the terse version.
export function lightLevelTag(lux: number): 'Bright' | 'Dim' | 'Dark' {
  if (lux >= 400) return 'Bright';
  if (lux <= 30) return 'Dark';
  return 'Dim';
}

// Granular 9-level breakdown for the dashboard specifically, modeled on
// standard lux reference charts (photography/lighting-design references)
// but scoped to the indoor residential range this sensor actually operates
// in (no astronomical tiers like starlight/moonlight -- irrelevant here).
// This is intentionally more detailed than lightLevelTag() above since the
// web UI has room to show it and a single Bright/Dim/Dark label alone
// isn't much more meaningful than a raw number to most people.
export function lightLevelDetail(lux: number): string {
  if (lux < 1) return 'Pitch Black';
  if (lux < 10) return 'Very Dark';
  if (lux < 30) return 'Dark';
  if (lux < 80) return 'Dim';
  if (lux < 200) return 'Ambient Indoor';
  if (lux < 400) return 'Well Lit';
  if (lux < 800) return 'Bright Indoor';
  if (lux < 1500) return 'Very Bright';
  return 'Daylight-Level';
}
