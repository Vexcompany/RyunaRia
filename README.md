# RyunaRia

Original PSP homebrew isometric action RPG written in C for **PSP / PPSSPP** (`480x272`, hardware-accelerated `sceGu`).

## Current target (`v0.2.0` Playable Prototype)

- **Platform**: PSP / PPSSPP (`BUILD_PRX = 1`, packaged as a bootable UMD `.iso`)
- **Genre**: Isometric Action RPG (`2:1` diamond projection with 2.5D depth-sorted world geometry)
- **Core pillars implemented**:
  - **Exploration**: `40x40` isometric world divided into 4 zones — *Sunlit Sanctuary* (safe hub & blessing altar), *Verdant Ruins*, *Crystal Hollow*, and *Ashen Wastes* — with 2.5D pillars, crystal spires, hazards, treasure chests, and a live radar minimap.
  - **Combat**: 3-hit melee combo arcs (`Cross`), Arcane Bolts (`Square`), radial Arcane Nova (`L + Square`), i-frame Shadow Dash (`Circle`), target lock-on (`L`), telegraphed enemy attacks, hit-flashes, knockback, and floating damage/status popups.
  - **Grinding**: Leveling & stat scaling, timed **Hunt Chain** XP/loot multipliers (`x1.0` to `x2.5`), Sanctuary Shrine permanent blessings, dynamic monster respawns, and *Rift Warden* Apex encounters.
  - **Equipment**: Weapon, Armor, and Relic slots + 12-slot backpack, 4 rarity tiers (*Common*, *Uncommon*, *Rare*, *Mythic*), physical ground loot beams, stat delta comparison, and item salvaging into Gold.
  - **Adaptive Monsters**: Live **Ecosystem Adaptation Telemetry** tracks player melee pressure, arcane kiting, dash frequency, and hunt tempo — evolving traits (*Ironhide*, *Spellward*, *Pursuer*, *Frenzy*, *Thorns*, *Leeching*) both on spawn and mid-fight, with clear tactical counterplay (`Arcane` pierces *Ironhide*; `Melee` shatters *Spellward*).

## Controls (PSP / PPSSPP)

| Input | Action |
| --- | --- |
| **Analog Stick / D-Pad** | Isometric 8-way / 360° movement |
| **Cross (`X`)** | 3-hit melee combo slash (shatters `SPELLWARD`) |
| **Square (`[]`)** | Cast Arcane Bolt (`10 MP`, pierces `IRONHIDE`) |
| **L + Square (`L + []`)** | Unleash radial Arcane Nova (`20 MP`) |
| **Circle (`O`)** | Shadow Dash evade with invulnerability frames |
| **Triangle (`/\`)** | Interact (Shrine Heal/Bless, Open Chest) or use Tonic |
| **R Trigger** | Quick Tonic heal |
| **L Trigger** | Lock onto nearest monster & inspect adaptations |
| **SELECT** | Open Equipment & Backpack screen (`X` Equip, `[]` Salvage) |
| **START** | Open Tactical Pause & Adaptive Monster Codex |

## Repository Structure

- `include/` — Modular headers (`ryunaria.h`, `render.h`, `world.h`, `items.h`, `combat.h`)
- `src/` — Engine & game systems (`main.c`, `render.c`, `world.c`, `items.c`, `combat.c`, `ui.c`)
- `tools/package_assets.py` — Generates `ICON0.PNG` and `UMD_DATA.BIN` at CI build time
- `.github/workflows/build.yml` — Automated PSPDEV C compilation + `UMDGenCLI` `.iso` builder

## Build (CI -> `.iso`)

The project is built automatically in **GitHub Actions** (`.github/workflows/build.yml`):

1. Compiles all C modules into `RyunaRia.prx` and UMD `PARAM.SFO` inside `pspdev/pspdev:latest`.
2. Generates `UMD_DATA.BIN` and `ICON0.PNG`, stages `PSP_GAME/SYSDIR/EBOOT.BIN` & `BOOT.BIN`, and packages a verified UMD-padded `dist/RyunaRia.iso` via `UMDGenCLI`.
3. Publishes `RyunaRia.iso` as the workflow artifact (generated binaries and ISOs stay out of Git).
