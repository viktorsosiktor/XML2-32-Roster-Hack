# What is in the XML2 32 Roster Hack Beta v0.02

Everything below is in `rosterhack.asi` unless it says otherwise. All patching happens in memory;
XMen2.exe on disk is never changed. Removing the .asi removes everything.

| # | Part | What it does | Setting | Default |
|---|------|--------------|---------|---------|
| 1 | Hero record pool | Moves the 0x44-byte hero pool into plugin memory, sized for 33 records (the default entry plus up to 32 heroes); fixes all code that assumed 22 slots | `[Limits] Roster` on/off | on |
| 2 | Playable lookup | Playable-index loop follows the real herostat count | none | always on |
| 3 | Roster screen list | Replaces the 21-name list and its filler so all heroes appear | none | always on |
| 4 | Portrait fix | Lets additional character heads (the 22nd hero and later) show their portraits without updating the character heads package files | none | always on |
| 5 | Character manager | Larger character pool and name table; rewrites dependent offsets, masks and limits (keeps NPC models, including cutscene characters, from not showing because of the larger roster) | `[Limits] CharPool` on/off (64 slots), `NameTable` on/off (480 names) | on / on |
| 6 | Team bonus limits | Raises the game's limit of 17 team bonuses of 6 heroes each. The game silently dropped any team beyond the limits; the extra teams now load from your own `team_bonus.engb`. Up to 100 teams of 23 heroes; experimental, only 24 teams of 10 heroes has been tested in game | `[Limits] TeamBonus` on/off | on |
| 7 | Cel shading option | Hides the outline scene nodes (needs libIGSg.dll). Found by name: `_outline` and the known misspellings, `outline`, `804`, `3902`, `5002`, `outl`, `_outl` | `[Visual] CelShading` on/off | on |
| 8 | Diagnostics | High-water marks, "NAME TABLE FULL", crash note (first-chance access violations: address, module+offset, registers, stack), per-run snapshots | none | always on |

`controllerchain.asi` (separate, optional): loads the XML2 Modern Controller mod by andremeloo as `xml2_controller.dll` and
points libIGDisplay's DirectInputCreateEx at it, because the loader owns the name `dinput.dll`.
It has its own crash note and writes `xml2_chain.log`.

Not changed by the hack: the herostat, talents, team bonus data (you write the teams, the hack only raises the limits),
saves, skins or any game file.

## Looked at, and not included yet
- **Danger Room:** left unchanged on purpose. The game's course table appears to hold up to 75 courses. The stock game
  uses 34, and 39 with the online courses restored, so there is plenty of room. No limit tied to the number of heroes was found.
- **Danger Room discs:** not changed. Danger Room courses can also be found as collectible items in missions and may be
  offered in the store, and the hack does not add or move any Danger Room disc items.
- **Review paths (loading screens, concept art, comic covers):** there appears to be ample space. The game's code reserves
  90 entries per list and the stock game uses at most 54, so no change was needed.
- **Comic covers:** not changed. They are collectible items placed in missions, so adding more would mean editing the missions.
- **Talent limits** (15 per file, 19 per hero): not changed.

## Other limits, not changed by the hack
The hack has not looked at these yet, and some may need to be raised due to the increased roster size.
"Unknown" means no value is documented, not that there is no limit.

| Limit | Value |
|-------|-------|
| Skins per character | 9 |
| Powerstyle triggers per fightmove | 20 |
| Segment hide codes in the herostat | Unknown |
| Fightmoves for the entire team | Unknown |
| Items | Unknown |
| Loading screens per character number | Unknown |

## Credits
Limits research: BaconWizard17 and Marvel Mods (https://marvelmods.com).
