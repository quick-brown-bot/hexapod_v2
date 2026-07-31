# KiCad `.kicad_sch` S-expression reference

KiCad 9 (`version 20250114`, `generator "eeschema"`). The file is a single
nested S-expression. Indentation is tabs. This reference captures the shapes the
generator must emit. Source examples are the real boards under `hardware/`.

## Top-level skeleton

```scheme
(kicad_sch
  (version 20250114)
  (generator "eeschema")
  (generator_version "9.0")
  (uuid "<ROOT-UUID>")          ; stable — registry key root:<board>
  (paper "A4")
  (lib_symbols
    (symbol "Connector:RJ25" ...) ; cached copy of every symbol used
    ...)
  ;; --- placed instances, wires, junctions, labels follow ---
  (symbol ...)                    ; one per placed component
  (wire ...)
  (junction ...)
  (global_label ...)
  (sheet_instances
    (path "/" (page "1")))
  (embedded_fonts no))
```

The `lib_symbols` block is a cache of the full symbol definition (pins, graphics,
properties) for every `lib_id` placed in the sheet. KiCad needs it present and
self-consistent. During migration, copy these verbatim; for new symbols, pull
the definition from the relevant `.kicad_sym` library.

## Placed symbol (component instance)

```scheme
(symbol
  (lib_id "power:+6V")
  (at 49.53 114.3 0)             ; x y rotation — POSITION, not part of stable key
  (unit 1)
  (exclude_from_sim no)
  (in_bom yes)
  (on_board yes)
  (dnp no)
  (fields_autoplaced yes)
  (uuid "<SYMBOL-UUID>")          ; stable — registry key sym:<ref> (e.g. sym:U1)
  (property "Reference" "#PWR01" (at 49.53 118.11 0) (effects (font (size 1.27 1.27)) (hide yes)))
  (property "Value" "+6V" (at 49.53 109.22 0) (effects (font (size 1.27 1.27))))
  (property "Footprint" "" ...)
  (property "Datasheet" "" ...)
  ;; one (pin "<num>" (uuid ...)) per pin may appear
  (instances
    (project ""
      (path "/<ROOT-UUID>"        ; references the root schematic uuid above
        (reference "#PWR01")
        (unit 1)))))
```

**Critical:** the PCB links a footprint to this symbol via
`/<ROOT-UUID>/<SYMBOL-UUID>`. Both must stay stable across regenerations or the
board placement/routing for that part is lost. The `(at ...)` may change freely.

## Wire

```scheme
(wire
  (pts (xy 93.98 153.67) (xy 124.46 153.67))
  (stroke (width 0) (type default))
  (uuid "<WIRE-UUID>"))           ; stable — key wire:<endpointA>-><endpointB>
```

Wire UUIDs matter less for the PCB (nets are matched by connectivity, not UUID),
but keeping them stable keeps `git diff` clean. Derive the key from the
connected pins/net, e.g. `wire:R1.2->D1.K`, not from coordinates.

## Junction

```scheme
(junction
  (at 60.96 64.77)
  (diameter 0)
  (color 0 0 0 0)
  (uuid "<JUNCTION-UUID>"))       ; key junction:<x>,<y>
```

## Labels

Local label `(label "NAME" ...)`, hierarchical `(hierarchical_label ...)`, and
global:

```scheme
(global_label "L_{4}_COXA"
  (shape output)                  ; input | output | bidirectional | tri_state | passive
  (at 143.51 63.5 0)
  (fields_autoplaced yes)
  (effects (font (size 1.27 1.27)) (justify left))
  (uuid "<LABEL-UUID>"))          ; key glabel:<name>@<x>,<y>
```

Connecting components by matching label *text* is the netlist mechanism — two
pins with same-named labels are one net, no wire required. Prefer this.

## `sheet_instances` (always last, before embedded_fonts)

```scheme
(sheet_instances
  (path "/" (page "1")))
(embedded_fonts no)
```

## Notes for the generator

- Coordinates are millimetres, grid is typically 1.27 mm (50 mil). Keep pins on
  grid or wires won't connect in ERC.
- Rotation is degrees: 0/90/180/270.
- Emit `(at x y rot)` from layout logic; emit every `(uuid ...)` from the
  registry. Those are the only two concerns that must be separated cleanly.
- Round-trip test after any format change: regenerate a migrated board and
  confirm `git diff` shows no UUID changes.
