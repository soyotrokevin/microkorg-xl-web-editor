> Imported hardware research notes. Hardware verification claims below describe the original research session, not new testing performed during this UI update. Referenced extraction tools and binaries are retained only in the local archive, not distributed here.

# microKORG XL — Sound Editor & Protocol Notes

A reverse-engineered replacement for the discontinued Korg microKORG XL Sound Editor,
which is a 32-bit ppc/i386 binary and no longer runs on Apple Silicon.

Everything here was derived from the official `.app` binary, a third-party Midi Designer
layout, and **verified against the hardware** by sending a message and diffing the
program dump. Nothing in the "verified" sections below is guesswork.

---

## 1. Status

| Capability | State |
|---|---|
| Read `.mkxl_all` banks | working |
| Send a preset to the synth (SysEx) | working, byte-identical round trip |
| Read the current program from the synth | working |
| Edit banks offline (bulk operations) | working |
| Live editing over CC / NRPN | working, 101 controls |
| Live editing over SysEx parameter change | working, covers FX and params with no CC |
| FX type selection (18 types) | working, verified |
| Full 524-parameter catalog with byte offsets | partial (232 mapped, rest estimated) |

Hardware requirements:

- `GLOBAL → PROTECT = OFF`
- `MIDI FLT → SYS EX = ENABLE`

No Korg driver is needed — the microKORG XL is USB class-compliant.

Ports:

- Output: `microKORG XL SOUND`
- Input: `microKORG XL KBD/KNOB` (not `MIDI IN`; with the wrong input the dump never returns)

---

## 2. MIDI messages

| Action | Message |
|---|---|
| Request current program | `F0 42 3<ch> 7E 10 F7` |
| Send program | `F0 42 3<ch> 7E 40` + 396 B payload + `F7` = 402 B total |
| Change one parameter | `F0 42 3<ch> 7E 41 <p1 lo hi> <p2 lo hi> <value lo hi> F7` |

`3<ch>` is `0x30 | channel` (channel 0 → `0x30`).

**14-bit pairs are split 7-bit:** `lo = v & 0x7F`, `hi = v >> 7`.

The program payload is the 346-byte program block packed 7-in-8 (each group of 8 bytes is
preceded by a mask byte holding the MSBs). 346 bytes → 396 bytes packed.

**SysEx parameter change is the only way to reach FX and arpeggiator parameters.**
They have no CC and no NRPN.

Verified examples:

| Parameter | p1 | p2 | Byte affected |
|---|---|---|---|
| Transpose | 17 | 20 | 26 |
| Distort.B1.GAIN (FX1) | 80 | 71 | 293 |
| Pan.Delay (FX1) | 80 | 81 | — |
| FX1 Type | 80 | 1 | 286 |
| FX2 Type | 80 | 49 | 310 |

Sending Transpose = 127 stores byte 26 = 112, i.e. `64 + 48` — bipolar parameters are
stored centered.

### FX type encoding (verified)

Byte value = **128 + type index**. Read with `byte & 0x7F`.

| # | Type | # | Type |
|---|---|---|---|
| 0 | OFF | 9 | MOD DELAY |
| 1 | COMPRESSOR | 10 | TAPE ECHO |
| 2 | FILTER | 11 | CHORUS |
| 3 | BAND EQ | 12 | FLANGER |
| 4 | DISTORT | 13 | VIBRATO |
| 5 | DECIMATOR | 14 | PHASER |
| 6 | DELAY | 15 | TREMOLO |
| 7 | LCR DELAY | 16 | RING MOD |
| 8 | PAN DELAY | 17 | GRAIN SHIFT |

FX1 type → byte 286, FX2 type → byte 310.

Note: changing the FX type rewrites that effect's parameters to the new algorithm's
defaults. Measured: switching FX1 from OFF to BAND EQ moved bytes 288–293 in one shot.
That is hardware behaviour, not a bug in this tool.

---

## 3. Bank file format (`.mkxl_all`)

67,984 bytes, factory US bank, 128 programs.

```
chunks marked "6140":
  AllD / PrgS / PrgD x128 / GlbD / LibP
  PrgD record: 528 bytes
    [0..31]   header
    [32..39]  program name, 8 bytes ASCII
    [32..378] program block, 346 bytes  <-- this is exactly what the synth receives
    [378..527] padding
```

The file's block needs no conversion: it *is* the SysEx payload.

Some presets carry the signature string `Tom@KorgUSA` at offset 0x178; the real payload
ends at 0x177.

---

## 4. Program block layout

**Parameters are bit-packed.** Two 4-bit parameters can share one byte (e.g. OSC 1 Wave
and OSC 1 Mod both live in byte 32). Never create two controls that write the same byte —
they overwrite each other.

Verified offsets (all confirmed against hardware):

| Bytes | Content |
|---|---|
| 16–31 | internal name |
| 21–30 | voice: detune, spread, voice assign, analog tune, transpose, vib intensity, bend range |
| 32 | OSC 1 wave / mod |
| 33–34 | OSC 1 ctrl 1 / 2 |
| 37 | OSC 2 wave |
| 38–39 | OSC 2 semitone / tune |
| 40–42 | OSC 1 / OSC 2 / noise level |
| 44–47 | filter routing, F1 cutoff, F1 resonance |
| 56–62 | amp level, amp pan, wave shape, punch level |
| 64–79 | amp EG, EG 1 attack/decay/sustain/release |
| 76–79 | EG 1 (also reachable via SysEx 17:128–131) |
| 286–302 | FX1 (type at 286) |
| 310–324 | FX2 (type at 310) |

Bipolar parameters are stored centered: `byte = 64 + value`, range −48..+48 → 16..112.

---

## 5. Where the mappings came from

Three independent sources, in decreasing order of reliability:

### 5.1 Official editor binary (descriptor table)

`mac/editor_i386` — thin i386 slice of the official .app (`lipo -thin i386`).

Descriptors are 48-byte records in 9 arrays:

| Field | Meaning |
|---|---|
| `+0x00` | pointer to name string |
| `+0x08` | offset **within its page** |
| `+0x10` | bit mask (width of the field) |
| `+0x14` | minimum |
| `+0x18` | maximum |
| `+0x2c` | value-list key (e.g. `MfxType`, `OnOff`) |

524 parameters extracted → `ctrlr/korg_params.json`, `ctrlr/catalogo_parametros.csv`.

**Critical:** `+0x08` is page-relative. For the program block (page 2) the base is 16, so
`offset = 16 + id`. Reading it as absolute collapses parameters from different pages onto
the same byte (Program Name 1, Timbre SW and Sub Output Select all appeared at 16).

### 5.2 Midi Designer layout (`.mididesigner`)

`2021-02-07-MICROKORG XL.mididesigner` is an `NSKeyedArchiver` binary plist, 7.4 MB,
23,843 objects. Parse with `plistlib.load` and walk `$objects`.

- MIDI models are dicts containing `command`, `control`, `data1`, `nrpnLsb`, `sysexData`.
- `control` points to a ControlModel that carries `label`.
- SysEx payloads live in `NS.data` objects; resolve the UID, do **not** recurse blindly
  (the graph is cyclic: model ↔ control).
- 1427 models, 1138 labeled, 639 with a usable SysEx template.

Its labels are contaminated: value buttons (`+64`, `-63`, `ON/OFF`, `-1`) look like
parameters. Discard labels starting with a digit or sign, and dedupe by offset.

Extracted: 85 unique parameter-change addresses, 66 of which move bytes on the hardware,
yielding 27 clean new parameters (voice, EG1, wave shape, FX1/FX2).
Output: `ctrlr/mididesigner_full.json`, `ctrlr/addr_map.json`, `ctrlr/addr_sweep.jsonl`.

### 5.3 Empirical sweeps

- **NRPN sweep** (8 pages × 128): 1024 addresses, 47 seconds, 53 respond, 50 already known.
  The NRPN space is essentially exhausted — the missing parameters are not there.
- **SysEx parameter-change sweep**: 85 addresses, 59 mapped to a single byte.

Method: send value A → dump, send value B → dump, the bytes that differ are the parameter.

---

## 6. Current web editor

The current editor is generated by `build_editor.py` from `src/editor.template.html` and
`ctrlr/controls_final.json`. See the root README for layout, build and usage instructions.
The earlier overlay layout has been replaced with compact tabs.

---

## 7. Files

### Scripts

| File | Purpose |
|---|---|
| `mkxl_bank.py` | read/analyse `.mkxl_all` banks (`list`, `dump`, `stats`, `csv`) |
| `mkxl_send.py` | SysEx build/parse, `send --verify`, `sendall`, `recv`, `ports` |
| `mkxl_edit.py` | offline bank editing: `params`, `show`, `set`, `level`, `pan`, `copy`, `rename` |
| `build_editor.py` | emits `mkxl_editor.html` from `controls_final.json` |
| `scan_nrpn.py` | NRPN space sweep → `ctrlr/nrpn_scan.jsonl` |
| `learn_map*.py` | map control → byte by sending and diffing |
| `sweep_pc.py` | sweep SysEx parameter-change addresses → `ctrlr/addr_sweep.jsonl` |
| `test_pc.py` | probe the parameter-change message framing |
| `test_fxtype.py` | verify FX type byte and encoding |
| `abrir_editor.command` | launch server + Chrome |

### Data

| File | Content |
|---|---|
| `ctrlr/controls_final.json` | **source of truth**: 130 controls with kind, address, offset |
| `ctrlr/korg_params.json` | 524 official descriptors: name, min, max, mask, offset, confidence |
| `ctrlr/catalogo_parametros.csv` | same, as a spreadsheet |
| `ctrlr/param_sysex.json` | 59 parameters reachable only by SysEx, with byte |
| `ctrlr/addr_map.json` | 85 parameter-change addresses with their labels |
| `ctrlr/addr_sweep.jsonl` | raw sweep results, one line per address |
| `ctrlr/mididesigner_full.json` | 1427 MIDI models extracted from the layout |
| `ctrlr/nrpn_scan.jsonl` | NRPN sweep results |

---

## 8. Pitfalls

- **mido strips `F0`/`F7` from received SysEx.** The function byte is at `g[3]`, not
  `g[4]`. Getting this wrong makes the device look unresponsive when the cable is fine.
  Conversely, `mido.Message('sysex', data=...)` rejects `F0` — send the payload without
  the delimiters.
- **Always restore the patch** after a sweep: re-send a known preset. Sweeps leave the
  edit buffer dirty.
- **Do not sweep into global parameter space.** MIDI channel and filters live there; a
  blind sweep can leave the synth deaf to MIDI.
- **Switching FX type resets that effect's parameters** to defaults.
- The static `.mkxl_all` block and the live parameter-change path are different worlds:
  offline edits write bytes, live edits go through addresses.

---

## 9. Known gaps

- 287 of the 524 official parameters still have estimated, unverified offsets.
- The vocoder block is structurally generated (pan/level pairs stepping by 2 from byte
  216) rather than table-driven, so it needs its own formula rather than a table entry.
- Only 27 of the 43 byte-mappings the Midi Designer layout revealed were clean enough to
  ship; the rest were duplicates or had no usable label.
