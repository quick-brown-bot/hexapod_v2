---
name: hardware-schematics
description: >-
  Author and maintain this repo's KiCad electrical schematics as Python code —
  the boards under hardware/ (mainboard, legboard, powerboard) whose
  .kicad_sch files are generated from .py source. Use this skill whenever the
  user wants to define, edit, generate, or migrate a schematic / netlist /
  circuit / components / nets / connectors as code, set up or use the
  schematic-as-code DSL, regenerate a .kicad_sch from Python, or asks about
  keeping KiCad UUIDs stable so an existing PCB layout isn't lost. Prefer this
  code path over hand-editing a .kicad_sch, and trigger even when the user
  doesn't explicitly say "as code". NOT for PCB layout / routing / trace-width /
  copper pours in the pcb editor, BOM or netlist export, footprint or 3D-model
  design, running ERC in the GUI, firmware, or drawing diagram images — those
  are different tasks.
---

# Hardware schematics as code

This repo's electrical designs live under `hardware/` as KiCad projects
(`mainboard`, `legboard`, `powerboard`). The goal of this skill is to author
those schematics from **Python source of truth** that generates the
`.kicad_sch` files, instead of editing them by hand in Eeschema.

The `.kicad_sch` format is plain S-expressions — no external library is needed,
pure Python reads and writes it.

## The non-negotiable rule: UUID stability preserves the PCB layout

A KiCad PCB references every schematic symbol by the path
`<root_schematic_uuid>/<symbol_uuid>`. If either UUID changes, KiCad treats the
footprint as a *new* part, drops its placement and routing, and you have to
re-lay-out the board. **The entire point of this toolchain is to never let that
happen.**

Therefore:

- The generator NEVER invents a UUID for an element it has seen before. It looks
  up a **stable key** (e.g. `sym:U1`, `wire:R1.2->D1.K`, `glabel:L_4_COXA@143.5,63.5`)
  in a committed `uuids.json` registry and reuses the stored UUID.
- Stable keys are derived from *identity* (reference designator, pin, net name),
  not from *position*. Moving a component on the canvas must not change its key.
- New elements get a fresh `uuid4()`, which is then written back to `uuids.json`.
- `uuids.json` is committed to git and **never hand-edited**. It is the memory
  that links code → schematic → board across regenerations.
- The root schematic `(uuid ...)` is itself stored in the registry under a key
  like `root:leg_v2` and reused on every run.

See `references/uuid-registry.md` for the registry design and key scheme, and
`references/kicad-sch-format.md` for the exact S-expression shapes (symbol
instance, wire, junction, label, the `instances` block, `sheet_instances`).

## ⚠️ Always update documentation in the same change

Every time you touch a schematic through this skill — add a component, change a
net, migrate a board, or extend the toolchain — **update the documentation in
the same commit**. Schematic-as-code is only maintainable if the docs track it.
This is a standing requirement, not an optional nicety. Specifically:

- Update the board's own notes (a `README.md` next to the `.kicad_sch`, or the
  relevant file under `docs/`) describing what changed and why.
- Keep the connector / pin-mapping / net tables current — these are the parts
  humans actually read, and stale pin tables cause wiring mistakes.
- If you added or changed a DSL primitive or a stable-key convention, document
  it in `references/uuid-registry.md` and the generator's module docstring.
- Note the change in the project changelog / commit message in human terms
  ("added 470R + LED status indicator on IO2"), not just "regenerated sch".

If you finish a schematic change without updating docs, the task is not done.

## File layout per board

```
hardware/<board>/
├── <board>_sch.py     ← source of truth — you EDIT this
├── uuids.json         ← committed registry — generated, never hand-edited
└── <board>.kicad_sch  ← generated output — open in KiCad to lay out the PCB
```

Shared generator/DSL code lives once at `hardware/schematic/` (the
`schematic.py` library and `migrate.py` tool), imported by each board script.

## Workflow

### Authoring or editing a schematic

1. Read the board's `<board>_sch.py`. Make the change in Python inside `build()`
   (`sch.place("lib_id", "ref", at=(x, y), value=...)`, `sch.net(name, [pins])`,
   `sch.wire(a, b)`, `sch.set_value("R3", "1k")`). Import a new part's symbol with
   `sch.import_lib_symbol(...)` before placing it.
  Set `footprint=` in the Python source for any part whose PCB package matters;
  do not rely on KiCad session state or on a symbol library's default footprint
  surviving regeneration.
2. Prefer **net labels** over explicit wires where possible — connecting by net
   name avoids hand-computing wire endpoints and is how KiCad nets work anyway.
3. Run the script to regenerate the `.kicad_sch`. Confirm `uuids.json` only
   *grew* (new keys) and did not rewrite existing UUIDs — `git diff uuids.json`
   should show additions, not modifications, for unchanged parts.
4. **Verify with `kicad-cli`** (see below) — the file loads, and the netlist
   connects what you intended.
5. Open in KiCad, run ERC, update the PCB from schematic. Existing placements
   must be preserved; only genuinely new parts appear unplaced.
6. **Update the docs** (see the rule above). Then commit `*_sch.py`,
   `uuids.json`, the `.kicad_sch`, and the doc changes together.

### Verifying with `kicad-cli`

The Python parser will happily write a file KiCad then rejects, and net-label
connectivity is invisible in a diff (it's by matching label *name*, nothing
positional). So check both, headless, with the installed `kicad-cli`:

```bash
# 1. Does it even load? Catches structurally-invalid output (see Gotchas).
kicad-cli sch erc <board>.kicad_sch -o erc.txt

# 2. Did net()/wire() actually connect the pins? This is the real correctness gate.
kicad-cli sch export netlist --format kicadsexpr -o net.net <board>.kicad_sch
```

Inspect the netlist's `(net (name …) (node (ref …)(pin …)) …)` entries to confirm
each rail/bus/signal lists exactly the pins you expect. ERC *warnings*
(`endpoint_off_grid`, unused-pin) do not imply broken nets — connectivity is
position-independent — so the netlist export, not the ERC count, is what tells
you the board is wired correctly.

### Creating a new board from scratch

Use this when there is no existing `.kicad_sch` to start from. The Python script
is the *sole* source of truth — there is no frozen `.base` file (that only exists
for migrated boards), so the script builds the whole schematic from primitives.

```python
from hardware.schematic import Schematic, UuidRegistry

reg = UuidRegistry("hardware/<board>/uuids.json")
sch = Schematic.new("<board>", reg, paper="A4")   # root uuid comes from registry

# Every part needs its symbol definition imported from a .kicad_sym library once;
# unlike a migrated board, a new sheet starts with an empty lib_symbols cache.
sch.import_lib_symbol("hardware/symbols/Seeed_Studio_XIAO_Series.kicad_sym",
                      "Seeed_Studio_XIAO_Series:XIAO-RP2040-DIP")
u1 = sch.place("Seeed_Studio_XIAO_Series:XIAO-RP2040-DIP", "U1", at=(100, 100),
               footprint="Seeed_Studio_XIAO_Series:XIAO-RP2040-DIP")

sch.import_lib_symbol(R_SMALL_LIB, "Device:R_Small")
r1 = sch.place("Device:R_Small", "R1", at=(80, 90), value="470", rotation=90,
               footprint="Resistor_SMD:R_0805_2012Metric")

sch.net("LED_OUT", [u1.pin("IO2"), r1.pin("1")])   # connect by net label

sch.write("hardware/<board>/<board>.kicad_sch")
```

Key differences from a migrated board:

- **No `.base` file.** Source of truth is just `<board>_sch.py` + `uuids.json`.
  Re-running rebuilds the sheet from scratch, so it is naturally idempotent.
- **Import every symbol.** `import_lib_symbol(<.kicad_sym>, <lib_id>)` must run
  before the first `place()` of each new part type. In-repo libraries live at
  `hardware/symbols/` and `hardware/SymbolsLib.kicad_sym`; stock KiCad symbols
  (`Device:*`, `power:*`) come from the KiCad install's libraries.
- **UUID stability still applies** the moment you first lay out the PCB: from then
  on, `uuids.json` is what protects the layout, exactly as for a migrated board.

For this repo's current V2 boards, the working default is: ordinary resistors,
small capacitors, indicator LEDs, and other small-signal diodes should use 0805
footprints unless the part's electrical or thermal requirements say otherwise.
Power-path parts are explicit exceptions — e.g. bulk polarized capacitors,
reverse-polarity diodes, and current shunts should get a package chosen from the
actual datasheet, not a blind 0805 default.

You also need a matching `<board>.kicad_pro` for KiCad to open the project — copy
one from an existing board and update the name.

### Migrating an existing schematic into code (one-time per board)

```bash
python -m hardware.schematic.migrate hardware/<board>/<board>.kicad_sch
```

The migrator freezes the original as `<board>.base.kicad_sch`, **seeds
`uuids.json`** with the root UUID and every component UUID (keyed by reference
designator), and emits `<board>_sch.py` (which loads the frozen base, applies your
edits, and writes the working `.kicad_sch`). Because the engine round-trips
untouched trees byte-for-byte, a freshly migrated board regenerates with **no UUID
churn** — verify with `git diff`. All current boards (`mainboard`, `legboard`,
`powerboard`) are already code-driven; only run this against a new board authored
by hand outside this workflow. Start with the smallest and confirm the
byte-exact round-trip before trusting it on a large board. Commit a board's
migration (`<board>_sch.py`, `<board>.base.kicad_sch`, `uuids.json`) only once
you've verified its PCB footprint links still resolve.

## The toolchain

`hardware/schematic/` already exists (see its `README.md`):

- `sexpr.py` — lossless S-expression parser + KiCad-faithful serializer. Round-trips
  every board byte-for-byte; preserving each atom's source text is what keeps diffs
  (and UUIDs) clean.
- `registry.py` — `UuidRegistry` (load/lookup/seed/save `uuids.json`).
- `schematic.py` — the `Schematic` DSL: `new`/`load`, `place`, `wire`, `junction`,
  `label`, `net`, `set_value`, and pin world-coordinate resolution for all four
  rotations.
- `migrate.py` — freezes a board, seeds the registry, emits the source-of-truth script.

When **extending** it (new primitive, new format element), match
`references/kicad-sch-format.md` exactly — KiCad is strict about the `lib_symbols`
library, the per-symbol `instances` block, and the trailing `sheet_instances` /
`embedded_fonts` entries. After any change to `sexpr.py`, re-run the byte-exact
round-trip check on all three boards before trusting it.

## Gotchas (hard-won)

- **`net(..., shape=…)` / `label(..., shape=…)` only accept a valid KiCad
  `global_label` shape: `input | output | bidirectional | tri_state | passive`.**
  Anything else (e.g. `shape="power"`) is written without complaint — the Python
  parser accepts it, `.write()` succeeds, `uuids.json` looks fine — and then
  KiCad fails the *entire* schematic to load with a generic "Cannot load
  schematic file" (no line number). For power rails use a real `power:*` symbol
  or just the default `passive`. `kicad-cli sch erc` is how you catch this fast.
- **Derived `(extends …)` stock symbols have no pins** — `import_lib_symbol`
  won't flatten them. Harvest a standalone copy instead: load the source (a
  `.kicad_sym`, or an existing board's `lib_symbols`), re-key the symbol to a
  bare name, wrap the symbol nodes in a `node("kicad_symbol_lib", …)`, write it
  to an in-repo `.kicad_sym`, then `import_lib_symbol` from that. (Done this way
  for `RJ25` + `SP3485CN` → `hardware/.../symbols/Hexapod_V2.kicad_sym`.)
- **Stacked module pins share coordinates.** Big modules (ESP32-WROOM) draw
  several same-net pins (e.g. all GND) at one location, and `_lib_pin` keys by
  both pin number and name (number: last wins; name: first wins). Connect by a
  *unique* pin name where you can, and confirm the merge in the exported netlist.
- **Two-pin net-label rotation rule (`R`, `C`, `D`, `LED`).** Use
  `rotation=180` on the opposite-side label only for an unrotated (`rotation=0`)
  two-pin part when that improves readability. If the part itself is already
  rotated (`90/270`), do not also rotate labels by default. If rotated by 180 - reverse pins where you rotate labels.

  Rotated 90 deg LED example (no extra label rotation needed):

  ```python
  d5 = sch.place("Device:LED_Small", "D5", at=(180, 88), value="SBEC",
                 rotation=90, footprint=LED_0805_FOOTPRINT)
  sch.net("+5V", [r4.pin("1")])
  sch.net("LED_SBEC", [r4.pin("2"), d5.pin("2")])
  sch.net("GND", [d5.pin("1")])
  ```
- **`place()` writes the placed symbol's `Footprint` property exactly as you pass
  it.** If you omit `footprint=`, the generated sheet gets an empty placed-symbol
  footprint even when the library symbol carries a default package. For boards
  that already have PCB layout work behind them, always put the intended package
  in the Python source of truth.
- **Current-shunt footprints need a datasheet check, not a guess.** For the
  planned CMK-R010-class ~3.5 mm current shunt, a stock KiCad candidate exists
  (`Resistor_SMD:R_Shunt_Ohmite_LVK12`), but treat that as a starting point only
  and verify pad geometry against the exact part before committing it.
- **Place on the 1.27 mm grid** to avoid `endpoint_off_grid` ERC warnings. They
  don't break connectivity (labels still attach at the exact pin coordinate), so
  they're cosmetic — but a clean ERC is worth the grid-aligned `at=` coordinates.

## What this toolchain does NOT do

- **Auto-routing / auto-placement of wires** — you specify wire paths, or (better)
  connect by net label and skip wires entirely.
- **Inventing pin positions for unknown symbols** — a new symbol needs its pin
  data supplied or pulled from a `.kicad_sym` library.
- **Resolving `(extends …)` derived symbols.** `import_lib_symbol` copies the
  named symbol verbatim; it does not flatten an inherited base. Many stock KiCad
  parts are derived and carry *zero* pins of their own — e.g. `Connector:RJ25`
  extends `6P6C`, `Interface_UART:SP3485CN` extends `LTC2850xS8`. Import one and
  `pin()` raises `KeyError` (or the part places with no connectivity). Fix:
  harvest a flattened standalone copy into an in-repo `.kicad_sym` (see Gotchas).
- **Importing a symbol straight out of a `.kicad_sch`.** `import_lib_symbol`
  scans `find_all("symbol")`, which is *non-recursive*. A `.kicad_sym` has
  `(symbol …)` at the root (works); a `.kicad_sch` nests them inside
  `lib_symbols`, so the scan silently finds nothing. To reuse a board's symbol,
  wrap its `lib_symbols` children in a `kicad_symbol_lib` node and write that out
  first.
- **Hierarchical sheets** — single-sheet only for now; add later if needed.

When you hit one of these, say so plainly rather than guessing.
