"""Export gerbers + drill files for V2 boards via kicad-cli and zip them.

Usage:
    python hardware/export_gerbers.py <version> [board ...]

If no board is given, every subfolder of hardware containing a matching
<name>.kicad_pcb file is exported (folders without one are skipped). All
boards share the same version tag so a full release can be cut in one go.

Examples:
    python hardware/export_gerbers.py v2.1
    python hardware/export_gerbers.py v2.1 powerboard
    python hardware/export_gerbers.py v2.1 powerboard mainboard

Writes gerbers and drill files into <board>/output/, then zips them into
<board>/output/<board>-<version>.zip (matching the existing zips already
checked in for powerboard). Each zip is also copied into hardware/archive/
so every board/version combination can be ordered from one folder.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

V2_DIR = Path(__file__).resolve().parent
ARCHIVE_DIR = V2_DIR / "archive"


def find_boards() -> list[str]:
    boards = []
    for d in sorted(V2_DIR.iterdir()):
        if d.is_dir() and (d / f"{d.name}.kicad_pcb").exists():
            boards.append(d.name)
    return boards


def export_board(board: str, version: str) -> None:
    board_dir = V2_DIR / board
    pcb_path = board_dir / f"{board}.kicad_pcb"
    if not pcb_path.exists():
        print(f"error: {pcb_path} not found", file=sys.stderr)
        raise SystemExit(1)

    output_dir = board_dir / "output"
    output_dir.mkdir(exist_ok=True)

    subprocess.run(
        [
            "kicad-cli",
            "pcb",
            "export",
            "gerbers",
            "--board-plot-params",
            "--no-protel-ext",
            "--output",
            str(output_dir),
            str(pcb_path),
        ],
        check=True,
    )
    subprocess.run(
        [
            "kicad-cli",
            "pcb",
            "export",
            "drill",
            "--excellon-separate-th",
            "--output",
            str(output_dir),
            str(pcb_path),
        ],
        check=True,
    )

    zip_path = output_dir / f"{board}-{version}.zip"
    exts = {".gbr", ".gbrjob", ".drl", ".rpt"}
    files = sorted(p for p in output_dir.iterdir() if p.suffix in exts)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in files:
            zf.write(f, arcname=f.name)

    ARCHIVE_DIR.mkdir(exist_ok=True)
    archive_path = ARCHIVE_DIR / zip_path.name
    shutil.copyfile(zip_path, archive_path)

    print(f"wrote {zip_path} ({len(files)} files)")
    print(f"copied to {archive_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="version tag, e.g. v2.1")
    parser.add_argument(
        "boards",
        nargs="*",
        help="board directory names to export (default: all boards under hardware)",
    )
    args = parser.parse_args()

    boards = args.boards or find_boards()
    if not boards:
        print(f"error: no boards with a .kicad_pcb found under {V2_DIR}", file=sys.stderr)
        return 1

    for board in boards:
        export_board(board, args.version)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
