# UUID registry design

`uuids.json` is the durable link between Python source, the generated
`.kicad_sch`, and the laid-out PCB. It maps a **stable key** (derived from
element identity) to a UUID. Reusing UUIDs across regenerations is what stops
KiCad from discarding the board layout. It is committed to git and never edited
by hand.

## Shape

```json
{
  "root:leg_v2":        "831ebfd1-5c42-44e9-9291-1090bc16e0c5",
  "sym:U1":             "94c271cb-a474-4be1-9282-7dfb93594de4",
  "sym:R1":             "004cfe56-4caa-48d7-94c0-723f9493f764",
  "sym:#PWR01":         "46fd77d0-2948-4447-ac4c-edf101672801",
  "wire:R1.2->D1.K":    "3fa1bc00-1234-4abc-8def-0123456789ab",
  "junction:60.96,64.77": "008dad09-b2fd-44c2-a02a-0f486d6573a4",
  "glabel:L_4_COXA":    "0d352a8c-8b34-4d6b-bc72-a6286adbb01e"
}
```

## Key scheme

Keys must come from **identity**, never from canvas position, so that moving a
part on the sheet does not orphan its UUID (and its board footprint).

| Element            | Key format                          | Notes |
|--------------------|-------------------------------------|-------|
| Root schematic     | `root:<board>`                      | one per `.kicad_sch` file |
| Component symbol   | `sym:<reference>`                   | `sym:U1`, `sym:R1`, `sym:#PWR01` |
| Wire               | `wire:<pinA>-><pinB>`               | endpoints sorted so A→B order is deterministic |
| Junction           | `junction:<x>,<y>`                  | the rare case where position *is* identity |
| Global label       | `glabel:<netname>`                  | sanitize `{}` etc.; if a net has several labels, suffix `#1`, `#2` |
| Local label        | `label:<netname>#<n>`               | |

Reference designators (`R1`, `U1`) are the anchor of identity. If you renumber a
part, you are renaming its key — decide deliberately whether that part is "the
same part" (keep the key, edit the registry mapping in code by re-keying) or a
new one (let it get a new UUID).

## `UuidRegistry` behaviour

```python
class UuidRegistry:
    def __init__(self, path): ...        # load json, or {} if missing
    def get(self, key: str) -> str:      # return existing UUID, or mint uuid4(),
                                         # store it, mark dirty
    def save(self):                      # write back sorted json if dirty
```

Rules the generator must honour:

1. **Lookup before mint.** Every UUID written into the `.kicad_sch` comes from
   `registry.get(key)`. Never call `uuid4()` directly in emit code.
2. **Append-only in practice.** A normal edit only *adds* keys. `git diff
   uuids.json` showing a *changed* value for an existing key is a red flag —
   it means a stable key collided or was recomputed, and the PCB link for that
   element just broke. Stop and fix the key derivation.
3. **Migration seeds the registry.** `migrate.py` walks the existing
   `.kicad_sch` and records every `(uuid ...)` it finds under the correct
   stable key, including `root:<board>`, before any regeneration happens.
4. **Sorted output.** Save with sorted keys and stable formatting so diffs are
   readable and merge-friendly.

## Verifying stability

After any change, the regenerated board must not move existing footprints:

```bash
python hardware/<board>/<board>_sch.py
git diff hardware/<board>/uuids.json    # additions only for unchanged parts
# open in KiCad → Update PCB from Schematic → existing parts stay placed
```

When you add or change a key convention here, update this table **and** the
generator docstring in the same change — see the documentation rule in SKILL.md.
