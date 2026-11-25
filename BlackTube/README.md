# Black Tube

Stereo optical tube compressor (AU / VST3, macOS universal), modelled on a CL 1B-style opto compressor,
on a black rack panel with a VU meter, neon-lit knobs and a live dynamics scope.

## Controls
- **Output Volume** — off, −10 … +30 dB
- **Ratio** — 2:1 … 10:1 (the opto ratio is not literal: 2 / 6 / 10 behave like 2.7 / 5.1 / 16.8, and ease off under heavy gain reduction)
- **Threshold** — off, 0 … −40 dB (−20 starts compressing around −36 dBFS)
- **Attack / Release** — fast … slow (attack 8 ms … 0.9 s, release 11 ms … 1.9 s; both slow down at higher ratios)
- **Attack/Release Select** — fixed, fix/man, manual
- **Meter** — input, compression (needle rests on 0 and swings left), output
- **Sidechain Bus** — int. / ext. (the plugin's sidechain input, when the host routes one)
- **Sidechain Low Cut** — off, 80 Hz, 220 Hz on the detector
- **Generation** — vintage mk I (softer knee, faster, a touch of 2nd harmonic, low cut ~18 Hz, −0.8 dB) / modern mk II (clean, flat)
- **Parallel Comp.** — dry … wet
- **Lamp** — click to switch the unit in / out (bypass)
- **Dynamics scope** — grey is the incoming signal, green what is kept after gain reduction, orange the gain reduction, amber the threshold

Switches: click to step forward, right-click to step back. Knobs: drag up/down, double-click to reset, scroll wheel.

## Sound model (Source/dsp/Opto.h)
Fitted to black-box measurements of a reference opto compressor plugin (docs/measurements.json):
stereo-linked power detector with attack/release in the power domain, soft-knee curve with a 69 dB opto ceiling,
an opto-cell response around 200 Hz (low notes ripple the gain, highs don't), and a slightly asymmetric detector
for the measured 2nd-harmonic colour. Matches the reference within 0.5 dB on the compression curve in every mode
and within about 10 % on release timing (`bt_dsp_test`).

## Build
Built together with Compchild from the top-level CMakeLists.txt; targets `BlackTube_AU`, `BlackTube_VST3`,
`bt_dsp_test`, `bt_render` (offline renderer for measurements), and `bt_snapshot` with `-DCC_TOOLS=ON`.
