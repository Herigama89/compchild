# Compchild

Stereo vari-mu tube limiter (AU / VST3, macOS universal) on a vintage wood-panel interface.

## Controls
- **Channel A / B** — Threshold (0–10, 0 = no compression), Input Gain (±10 dB), Time Constant 1–6, DC Bias (knee shape)
- **Output** — ±10 dB
- **Mode** — L/R, Lat/Vert (sum/difference, A = lateral, B = vertical), Mid/Side (only the mid is compressed)
- **Link** (red lamp, bottom left) — channel A’s controls drive both channels (B follows on screen)
- **Power** (red lamp, bottom right) — bypass when off
- **GR A / GR B meters** — VU-style GR scale; needle rests on 0 and reads gain reduction (3 = 3 dB, 10 = 10 dB)
- **Meter skins** — Cream (default) or Black: click a meter to switch, or right-click the panel → Meter skin. Saved with the project.

## Manual
A full illustrated manual (`docs/manual.html`) is built into the plugin: click the **?** beside CHANNEL B, or right-click the panel → *Open manual…*.

## Sound model (Source/dsp/VariMu.h)
Fitted to black-box measurements of a reference vari-mu plugin (docs/measurements.json):
- clean below threshold, +8.55 dB fixed tube-stage gain at default Input/Output
- static curve `GR = u − w(1 − e^(−u/w))`, `u = level − T`, knee w = 11 dB, onset `T(k) = −16.75 − 8.97·ln(k)` dBFS
- two-stage release (61 % at τ, the rest at 4τ): TC1 24 ms, TC2 73 ms, TC3 212 ms, TC4 434 ms; TC5/TC6 are estimates
- program-dependent low-frequency distortion from gain modulation (≈1.3 % THD at 100 Hz, 0.2 % at 1 kHz, full limiting)
- output ceiling at −0.04 dBFS

Matches the reference within 0.25 dB on the compression curve; distortion and release timing track it closely.

## Build
GitHub Actions builds the installer on every push (see `.github/workflows/build.yml`); pushes to `main` publish a release with the `.pkg`.

Local build (macOS, Xcode command-line tools, CMake ≥ 3.22):
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Compchild_AU Compchild_VST3
```

The knob, power-icon and panel artwork come from the project owner's asset pack and photo; check their licences before distributing.
