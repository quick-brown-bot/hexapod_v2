# hex_controller_core

## Role
Controller core runtime plus shared controller-facing interfaces consumed by controller drivers and app logic.

## Public Headers
- controller.h
- controller_internal.h
- user_command.h
- controller.c

## Design Intent
- Keep controller-facing contracts and runtime state out of main.
- Allow drivers and RPC to depend on one stable controller component.
- Preserve non-circular dependency direction by keeping driver startup dispatch in main.
