---
description: "Use when writing or restructuring README.md files and monorepo docs files under ../../docs/*.md, including architecture docs, hardware docs, or documentation indexes. Covers documentation layering, README scope, and separation of current reference docs from plans/history."
name: "Documentation Structure And README Philosophy"
applyTo: "README.md, ../../docs/**/*.md"
---
# Documentation Structure And README Philosophy

- Treat documentation as layered, not flat.
- Keep the root README.md inviting and technically communicative, but not deep enough to become the full architecture reference.
- Preserve high-value onboarding context in the root README.md when editing it. Do not over-trim it into a sterile index.

## Root README.md

- Keep a concise project overview that explains what the system is and why the architecture exists.
- Preserve evidence of real engineering work in the root README.md, including an effort summary or equivalent implementation-status view.
- Keep a compact hardware snapshot in the root README.md so readers immediately understand the physical platform.
- Keep a compact mechanical model summary in the root README.md when it materially affects understanding of the firmware.
- Include a high-level architecture diagram in the root README.md when architecture is central to the project.
- Follow the diagram with plain-language bullets explaining each major subsystem.
- In those high-level README bullets, avoid file-path references and low-level implementation detail.
- Link from the root README.md into deeper documentation instead of duplicating full technical reference material there.

## Top-Level docs/ Structure

- Organize top-level docs/ systematically by intent, not as one flat pile of markdown files.
- Prefer stable categories such as architecture, configuration, interfaces, plans, and archive when that split fits the repository.
- Keep current reference documentation separate from forward-looking plans and historical refactor notes.
- If hardware or mechanics are important to understanding the firmware, give them a dedicated documentation home instead of burying them in unrelated docs.
- Use ../../docs/README.md as the documentation map and reading guide.

## Architecture Documentation

- The main architecture document should describe the current implemented architecture, not an outdated refactor target.
- Remove or isolate stale language that speaks about already-completed refactors as if they are still pending.
- Keep historical design reviews, migration plans, and refactor baselines in archive-style docs once they stop being the source of truth.
- In current architecture docs, emphasize component boundaries, runtime flows, dependency rules, and system responsibilities.

## Editing Philosophy

- When simplifying documentation, do not throw away onboarding value such as hardware context, effort summary, or the system-level view.
- Prefer moving deep detail into dedicated docs rather than deleting it outright.
- Prefer current-state reference docs for active behavior and archived docs for rationale/history.
- Keep documentation technically concrete, but avoid turning overview documents into file-by-file inventories unless that is their explicit purpose.