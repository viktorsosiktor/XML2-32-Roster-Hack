# XML2 32 Roster Hack Beta v0.02

**New in v0.02**
- **CelShading option (experimental).** `[Visual] CelShading=off` in `rosterhack.ini` hides the black cel-shading outline on models: heroes, capes, cutscene models, and also guns, holsters and swords. Outlines are found by part name, so heroes with and without outline parts can be mixed. Models without an outline are unaffected. Some skins need a one-time hex edit (see `xml2_hex_edit_instructions.txt`). Default is `on`, which changes nothing.
- **Simpler settings.** The limit settings are now simple on/off switches in `rosterhack.ini`: `Roster`, `CharPool`, `NameTable` and `TeamBonus`. On (the default) is the maximum, off is the game's original limit. The `Capacity`, `Playable` and `PortraitFix` settings are gone: the playable count always follows your herostat and the portrait fix is always on. Settings left in the `[Roster]` section of an older ini are ignored. Delete the old `rosterhack.ini` and let v0.02 generate a new one.
- **Team bonus limits raised.** `[Limits] TeamBonus=on` (the default) allows up to 100 team bonuses of up to 23 heroes each, where the game stops at 17 teams of 6 by default. The game silently drops team bonuses beyond its limits; the extra teams now load from your own `team_bonus.engb`. **Experimental:** only 24 teams of 10 heroes has been tested in game (with a stock team, and save and load). Set `TeamBonus=off` to switch it off.
- **Better error logging.** A memory error in the game is now written to `xml2_roster.log` (and `xml2_chain.log`) as `first-chance access violation`, with the address and module. Some of these are handled by the game itself and are harmless.
- **Mod Organizer 2 and OpenHeroSelect notes** and a "Check your install" list in the readme, including how to tell the 32-bit ASI loader apart.
- The research-only `OutlineProbe` setting has been removed.

**Included in the zip:** `rosterhack.asi`, `rosterhack.ini`, `README.txt`, `xml2_hex_edit_instructions.txt`, and an optional `controllerchain.asi` for XML2 Modern Controller v0.5.0-beta.2 (see the readme).

**Not included (download separately):**
- Ultimate ASI Loader v10.0.0 (`Ultimate-ASI-Loader-NoPDB.zip`, the 32-bit `dinput8.dll` renamed to `dinput.dll`): https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases
- XML2 Modern Controller v0.5.0-beta.2, if you use it: https://github.com/andreMeloo/XML2-Modern-Controller/releases
- Character mods

**What it does:** raises the hero limit of the 2005 PC version of X-Men Legends II to 32 heroes plus the default entry (33 hero records), lets the 22nd hero and beyond show portraits without updating the character heads package files, and enlarges the character pool and name table so cutscene characters and NPC versions of heroes still appear.

**Tested:** new game, cutscenes, first mission, first hub, stash, store, review terminal, save and load, with 32 heroes. CelShading=off on heroes, capes and cutscene models. Mod Organizer 2 by the author and another user, with the loader and plugin in the real game folder and building the XML2Patch to include the plugins.

**Not yet tested:** Danger Room (no limit tied to the hero count was found in the game's code), long missions, and other limits in the game or herostat that this plugin doesn't change (such as unique talents and team bonus data).

**Known issues:**
- A few skins need a hex edit before `CelShading=off` hides their outline.

The roster hack is only lightly tested and some unknown limits may cause bugs in the game due to the increased roster.

Please report problems in Issues and attach `xml2_roster.log` and `rosterhack.ini`.

---

## Beta v0.01

First beta release: the 32-hero roster, portrait fix, enlarged character pool and name table, and the optional controller chain.
