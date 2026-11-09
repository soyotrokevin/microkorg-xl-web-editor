# microKORG XL Web Editor — Beta

A compact, browser-based sound editor for the **Korg microKORG XL**, built with plain HTML, CSS and JavaScript. Connect your synth through Web MIDI to adjust parameters and browse local preset banks.

**Status: beta.** New SysEx parameter mappings and FX type selection have hardware verification notes in the project documentation; complete UI-to-hardware validation is still pending. Back up your sounds before experimenting. This is an independent project, unaffiliated with Korg.

![microKORG XL Web Editor beta — synthesizer view](docs/images/editor-beta.png)

## Features

- Compact synthesizer dashboard designed to fit a 1280 × 720 viewport at 100% browser zoom.
- Dedicated Synthesizer, Voice, FX, Matrix, Vocoder and Presets tabs.
- 130 catalog controls, including SysEx voice, envelope, wave shaping and FX parameters.
- Independent FX1/FX2 type selectors with 18 algorithms each and masked type decoding from program dumps.
- Knobs, expanded selectors and ON/OFF switches for the arpeggiator, latch and vocoder.
- Six modulation patches and a 16-band vocoder level/pan view.
- A reproducible generator: edit the template and JSON catalog, then rebuild the standalone HTML.
- MIDI CC/NRPN and SysEx parameter-change messages, plus SysEx program reads.
- Local `.mkxl_all` bank loading, preset search and SysEx sending.
- In-memory control values and preset search retained when switching tabs.
- No build step, runtime dependencies, account or remote backend.

## Requirements

- A microKORG XL connected to your computer by USB MIDI or a suitable MIDI interface.
- A browser that supports Web MIDI with SysEx access. Chrome is the recommended starting point.
- Python 3 for the local server below, or any static HTTP server.

Web MIDI requires a secure browser context and permission to access MIDI devices. Use localhost for local development, or HTTPS if hosting elsewhere. See [MDN's Web MIDI access documentation](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/requestMIDIAccess).

## Use online

[**Open the editor in your browser**](https://soyotrokevin.github.io/microkorg-xl-web-editor/)

Use Chrome, connect your synth, and grant MIDI/SysEx permission when clicking **Connect**. No installation or local server is needed for the hosted version. This remains a beta; the hardware and mapping limitations below still apply.

The **Presets** tab has a shared-bank selector when banks are available. The MKXL bank loads automatically the first time you open **Presets**. An already loaded bank, including a local file, is preserved when returning to the tab. To load another shared bank or reload it, choose it and click **Load bank**, then **Send** next to a program. Loading a bank only downloads its data; it does not send anything to your synth until you click **Send**. You can still load a local `.mkxl_all` file.

## Quick start (local)

1. Download this repository with **Code → Download ZIP** and extract it, or clone it:

   ```sh
   git clone https://github.com/soyotrokevin/microkorg-xl-web-editor.git
   cd microkorg-xl-web-editor
   ```

2. In a terminal opened in the project folder, start a local server:

   ```sh
   python3 -m http.server 8781 --bind 127.0.0.1
   ```

   On Windows, `py -m http.server 8781 --bind 127.0.0.1` may be used instead.

3. Open [http://localhost:8781/mkxl_editor.html](http://localhost:8781/mkxl_editor.html) in Chrome.
4. Connect and power on your synth. Click **Connect** and allow MIDI access, including SysEx.
5. Set **CH** to the synth's MIDI channel (1–16).
6. Click **Read program** to request the current sound before editing. Until a successful read, controls show placeholder values.

Stop the local server with **Ctrl+C** when finished.

## Editing sounds

- Drag a knob upward or downward to adjust it; hold **Shift** for finer control.
- Double-click a knob to reset it to MIDI value 64. This is a midpoint reset, not the original patch value.
- Focus a knob and use the arrow keys to change its value; **Shift** reduces the step. **Home** and **End** select MIDI values 0 and 127.
- Click a selector to choose an option, or click an ON/OFF switch to toggle it.
- Switch tabs for voice and wave shaping, effects, modulation routing, vocoder bands or preset banks. Use Left/Right arrows while a tab is focused to move between tabs.
- Changes are sent immediately when MIDI is connected. Use the synth's **WRITE** operation to save a sound permanently.

![FX type selection and SysEx controls](docs/images/fx-beta.png)

## Voice and effects

**Voice** contains the added detune, spread, voice assignment, analog tuning, transpose, vibrato intensity and bend-range controls, plus a separate wave shaping panel. The main Synthesizer tab includes the extra envelope controls.

**FX** exposes FX1 and FX2 type selection (OFF plus 17 algorithms), the existing macro/dry controls and the newly mapped parameters. Changing an effect type resets its parameter values on the hardware. Click **Read program** afterward to refresh the interface. Extra FX parameter labels come from the research catalog and are algorithm-specific; this is not a complete dynamic parameter editor for every algorithm.

## Loading a preset bank

1. Open **Presets**.
2. Drop your own `.mkxl_all` file onto the drop area, or click the area to choose a file.
3. Search by preset name or exact program number.
4. On the synth, enable SysEx reception (`MIDI FLT → SYS EX = ENABLE`). If required for the intended operation, disable write protection (`GLOBAL → PROTECT = OFF`).
5. Click **Send** next to a preset to send its program data to the synth's edit buffer.
6. Audition the result and use **WRITE** on the synth if you want to keep it.

Preset files are read locally in your browser. The shared catalog includes the microKORG XL Factory Presets bank (`MKXL_FactoryPreset.mkxl_all`). The other banks are not included.

## Beta limitations

- Not every parameter, CC/NRPN mapping or packed SysEx field has been verified on hardware. Reading a program may not reproduce every setting accurately.
- Some discrete parameters, including modulation destinations, currently appear as numeric knobs rather than named choices.
- The inherited FC Mod Source NRPN entry has an out-of-range address and is blocked from sending until its mapping is corrected.
- The two distinct Key Sync parameters have separate identifiers; their original addresses and offsets are retained.
- MIDI ports are selected automatically: an output containing `sound` and an input containing `kbd` or `knob` are preferred; otherwise the first available ports are used. There is no manual port selector yet. Disconnect unrelated MIDI devices if the wrong ports are selected.
- Changing controls without a MIDI connection updates the local interface only. Those changes are not replayed automatically after connecting.
- Control state is held in memory and is lost on reload. There is no edited-bank export, local persistence or undo history.
- Smaller windows use scrolling. Compatibility with microKORG XL+, the original microKORG and other models is not established.
- The 130-control interface, SysEx framing (including addresses above 127), FX masks, local-bank preservation and automatic shared-bank loading have regression checks in desktop Chrome; complete hardware validation remains pending.

## Troubleshooting

- **Web MIDI unavailable:** use a compatible browser and open the localhost address above.
- **No MIDI outputs found:** check the USB/interface connection and any required OS MIDI drivers, then reload and reconnect.
- **MIDI permission denied:** allow MIDI/SysEx in the browser's site settings and reconnect.
- **Read program does nothing:** verify the MIDI channel, SysEx reception and automatically selected MIDI ports.
- **Controls do not match the synth:** read the program again; if a mismatch remains, report it as a beta mapping issue.

## Project structure

```text
build_editor.py               Rebuilds the standalone application
src/editor.template.html      UI, styling and MIDI runtime source
ctrlr/controls_final.json     Authoritative 130-control catalog
mkxl_editor.html              Generated standalone application
index.html / .nojekyll        GitHub Pages entry point
presets/                     Public MKXL bank and shared-bank catalog
docs/protocol-notes.md        Imported protocol and hardware research notes
docs/images/                 Screenshots
tests/editor.cjs              Browser and simulated MIDI regression checks
abrir_editor.command         Portable macOS local launcher
```

Do not edit `mkxl_editor.html` directly: the next build will replace it. Edit `src/editor.template.html` for UI/runtime changes and `ctrlr/controls_final.json` for control metadata, then run:

```sh
python3 build_editor.py
```

The web application still has no runtime dependencies. Browser tests use Playwright as a development dependency:

```sh
npm install
npx playwright install chromium
npm test
```

Set `CHROME_PATH` to an existing Chrome executable if you want to test with that installation instead of Playwright's browser. Tests simulate MIDI; they do not connect to or modify hardware.

On macOS, `abrir_editor.command` starts the local server from its own folder and opens Chrome. You can also use the cross-platform local-server instructions above.

Earlier HTML versions, CLI utilities, mapping sweeps, binaries, logs and research data from the local working folder are preserved under `archive/2026-11-09/` locally. That archive and the existing Python environment are excluded from Git and GitHub Pages. The old CLI tools retain their original directory layout inside the archive; hard-coded research paths may need adjustment before reuse.

To report a bug, open an issue with your OS, browser version, synth model, connection type, parameter and steps to reproduce. Do not include private preset banks unless you intend to share them publicly.

## Protocol notes

See [protocol and hardware research notes](docs/protocol-notes.md) for parameter-change framing, bank packing, FX type encoding and known gaps. Hardware verification statements there refer to the supplied research session. This update tested the browser and simulated MIDI messages without sending anything to a connected synth.

## Publishing shared banks

Add banks you are allowed to redistribute to `presets/`, then list them in `presets/banks.json`:

```json
[
  { "name": "My sound bank", "file": "presets/my-sound-bank.mkxl_all" }
]
```

Bank paths must point inside this site's `presets/` directory. Everything added to this public repository is publicly downloadable. Local files selected in the editor are never uploaded.

GitHub Pages serves the root of the `main` branch. Updates pushed to `main` are published automatically.
