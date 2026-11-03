# XML2 32 Roster Hack Beta v0.02

> **Beta disclaimer.** This is an early beta release. It has been tested only lightly (see Next steps below) and may have bugs, crashes, or problems that have not been found yet. Use it at your own risk. **Back up your saves and game files before installing.** Saves made with more than 21 heroes may not load without this plugin. Please report problems with your `xml2_roster.log` (see Reporting a problem).

Raises the hero limit of the 2005 PC version of **X-Men Legends II** from 21 playable heroes to as many as 32 playable heroes plus the default entry (33 hero records).

It is an ASI plugin for [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader). It patches the running game in memory only. `XMen2.exe` on disk is never modified. To uninstall, delete the plugin files.

## What it does

- removes the herostat crash at 21 heroes
- allows up to 32 heroes in the roster (33 hero records with the default entry)
- lets the 22nd hero and beyond show portraits without updating the character heads package files
- raises the team bonus limits to 100 teams of up to 23 heroes (the game stops at 17 teams of 6). Experimental: only 24 teams of 10 heroes has been tested in game
- enlarges the character pool (64 slots instead of 31) and the name table (480 instead of 297) so NPC and cutscene characters still appear during missions
- adds an option to hide the black cel-shading outline on models (`CelShading=off`, experimental; `CelShading=on` by default)
- works with save and load

## Requirements

- The 2005 PC version of X-Men Legends II (`XMen2.exe`). The plugin checks the game's bytes first and does nothing if they do not match, so an executable it does not recognise is left alone. Modified executables may not match.
- Character mods. The roster hack only raises the limits; the heroes come from your own mods. Download character mods individually or as packages, as you choose, from the [Marvel Mods forum](https://marvelmods.com/forum/index.php/topic,753.0.html). The Marvel Mods AXE, BHE and MUE character package mods were compared byte by byte with the original `XMen2.exe`. They differ only in certain text strings (data file names and a hero name list), and the roster hack does not patch any of them, so the plugin should load and patch them the same way. This was checked by comparing the files, not by playing every package.
- The **32-bit** Ultimate ASI Loader, renamed `dinput.dll`. **Not included** in this download; see "Get the ASI loader" below. X-Men Legends II is a 32-bit game, so a 64-bit loader will not work. If you already have a recent **32-bit** Ultimate ASI Loader installed, you do not need another. Other ASI loaders are untested and may not work with the roster hack.

## Get the ASI loader

1. Go to the [Ultimate ASI Loader releases](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) and download `Ultimate-ASI-Loader-NoPDB.zip` (tested with v10.0.0).
2. Extract the zip.
3. X-Men Legends II is a 32-bit game, so use the 32-bit `dinput8.dll` from the zip.

## Roster Hack Install (Direct Method)

1. Back up your saves and your data folder.
2. Install the character mods you want first, individually or as packages.
3. **Rename the 32-bit ASI loader `dinput8.dll` to `dinput.dll`**. The game asks for that exact name. **WARNING:** If the game folder already has a `dinput.dll`, do not overwrite it. If you use andremeloo's modern controller mod, read "Controller Mod Chain (optional)" below before moving to step 4.
4. Put `dinput.dll` in the game folder, next to `XMen2.exe` (skip if you already have a 32-bit Ultimate ASI Loader there). If Windows asks to replace an existing `dinput.dll`, choose No and consider any other mods that may conflict with the roster hack.
5. Put `rosterhack.asi` and `rosterhack.ini` together in only one of these places:
   - the `plugins` folder in the game folder (recommended; the release zip already has a `plugins` folder with these files, so you can copy that folder into the game folder), or
   - right next to `XMen2.exe`.
6. Start the game and check that everything is working. `xml2_roster.log` appears beside `rosterhack.ini`; see "Check Your Install" below for what a good log looks like. If you use the controller chain, `xml2_chain.log` also appears next to `XMen2.exe` and a good log ends with `chained: the game now talks to the controller mod.` Check that your controller works.

The plugin looks for `rosterhack.ini` beside the `.asi` first, then in the game folder. If it finds none, it writes a default one next to the `.asi`.

Notes for Mod Organizer 2 and OpenHeroSelect are in the sections below.

## Controller Mod Chain (optional)

Only needed if you use [XML2 Modern Controller](https://github.com/andremeloo/xml2-modern-controller) by andremeloo **and** you want the roster hack. The modern-controller mod conflicts with the roster hack. Both want the name `dinput.dll`, so the controller mod must be moved out of the way. `controllerchain.asi` (included in the release zip) then links it back in. The controller mod itself is **not** included. Download it from its own page. Keep its `true-analog.ini` next to `XMen2.exe` as its readme says; only the `dinput.dll` is renamed.

Do the steps in this order so nothing is overwritten:

1. Copy the controller mod's original `dinput.dll` somewhere safe as a backup.
2. **Rename** the controller mod's `dinput.dll` to `xml2_controller.dll` (exactly that name). Keep it, and its `true-analog.ini`, in the game folder next to `XMen2.exe`, not in `plugins`.
3. Put `controllerchain.asi` in `plugins` (or next to `XMen2.exe`).
4. Continue with step 4 above in "Roster Hack Install (Direct Method)".

**Compatibility:** the chain was tested with XML2 Modern Controller v0.5.0-beta.2, the current version in October 2026. It does not depend on a specific version, only on the controller mod being a DirectInput proxy that exports `DirectInputCreateEx`, but newer releases are not guaranteed to work. If a future release changes how it hooks the game, the chain may need to be updated. The source is in `src/controllerchain.cpp` and is public domain, so anyone is free to adapt it; reports and fixes are welcome in Issues.

Without `xml2_controller.dll` the chain does nothing and the game runs normally. It only works if the controller mod is a DirectInput proxy installed as `dinput.dll`. To go back to the controller mod alone, delete the loader's `dinput.dll` and the two `.asi` files, then rename `xml2_controller.dll` back to `dinput.dll`.

## Settings

`rosterhack.ini` section `[Limits]`. Delete `rosterhack.ini` from Beta v0.01 and allow v0.02 to generate a new `.ini` as the settings have changed. Each setting is **on** or **off**. On (the default) uses the maximum limit; off uses the game's original limit. Only `off` turns a setting off; anything else counts as on. The playable heroes count (it follows your herostat; tested at startup with 25 and 32 heroes) and the portrait fix are built in. Settings left in the `[Roster]` section of an older ini are ignored.

| Key | Default | On | Off |
|---|---|---|---|
| Roster | on | Up to 32 heroes plus the default entry (33 hero records) | Original limit: 21 heroes plus the default entry |
| CharPool | on | 64 characters can exist in a level at once | Original: 31 |
| NameTable | on | Name table of 480 names | Original: 297 |
| TeamBonus | on | Up to 100 team bonuses of up to 23 heroes each. The extra teams come from your own `team_bonus.engb`. **Experimental:** only 24 teams of 10 heroes has been tested in game. | Original: 17 teams of 6 heroes |
| CelShading (section [Visual]) | on | Models keep their black cel-shading outline | EXPERIMENTAL. Hides the outline on models (bodies, capes, guns and holsters). Models that have no outline, such as Spider-Man, are unaffected. Outlines are found by part name. A few skins name the outline differently and need a small hex edit (see "Skins that need a hex edit" below). |

## Skins that need a hex edit (CelShading=off)

The plugin hides the model outline by the name of the model part. The following skins have outline name conflicts, so they can't be added to the roster hack. As a result, the character outline stays visible with `CelShading=off` unless the skin file (`.igb` in the `actors` folder) is edited:

- Default hero: `2401` (Scarlet Witch)
- Default NPC: `7401`, `7402`, `7403`, `7404`, `7405` and `8601`
- Mod hero: `15412` (Psylocke)

No other default or modded skins have been identified to need an edit at this time. The author considered only the skins provided by the default game and the three Marvel Mods XML2 character mod packages (AXE, BHE and MUE). Step-by-step instructions are in `xml2_hex_edit_instructions.txt`, included in the src and the release zip.

If you find a skin whose outline is not hidden with `CelShading=off`, please report it in a *Limit or glitch report* (for an NPC skin, tell us which mission it appears in).

## Build

x86 Native Tools Command Prompt:

```
cl /nologo /LD /O2 /MT src/rosterhack.cpp /Fe:rosterhack.asi
cl /nologo /LD /O2 /MT src/controllerchain.cpp /Fe:controllerchain.asi
```

## Mod Organizer 2

While the roster hack is compatible with Mod Organizer 2 (MO2), there is no official patch available that allows MO2 to recognize roster hack plugins. The plugins will need to be added directly to the game files, or you will need to build a working MO2 XML2Patch. Instructions for rebuilding the patch are not included here. Whichever route you choose, you will need to extract the `global.ini` file from the MUAPatch.zip located within the MUA1 MO2 Plugins among the [Marvel Mods Mod Organizer plugins](https://github.com/EthanReed517/Marvel-Mods-ModOrganizer-Plugins/releases) and include it in your game files next to `XMen2.exe` or the game will not launch.

MO2 should function with the aforementioned `global.ini`, and the loaders and plugins previously mentioned above in "Roster Hack Install (Direct Method)" and "Controller Mod Chain (optional)" added directly to the game files as instructed. This was tested by the author, and the roster loaded all 32 heroes. Rebuilding the MO2 XML2Patch was also confirmed by the author and another tester. If you hit a crash using MO2, please report the bug for troubleshooting.

If you also use the controller chain, `xml2_controller.dll` and `true-analog.ini` go in the real game folder with the loader next to `XMen2.exe`.

## Creating Data Files

The roster hack only raises the limits. Which heroes you can have comes from your own mods. The herostat needs to include every character you want in your roster. Characters may also appear locked at the start unless you update the `new_game.py` and `new_game_hard.py` script files. If you aren't sure how to update the script files, you can use the unlock all characters cheat. The herostat file may have unidentified limit breaches (such as unique talents); the roster hack does not change them yet.

OpenHeroSelect (OHS) by Marvel Mods is not compatible with the roster hack as released. The author was able to generate a herostat, character heads package and unlock scripts with it, but could not find a way to update the team bonuses properly. OHS may also have other unknown incompatibilities.

Other options: you can use Quick Batch and other tools by Marvel Mods at [Modding Tools](https://marvelmods.com/forum/index.php/board,45.0.html) to create your herostat and other data files, and the Marvel Mods community has many tutorials and videos with instructions for creating a herostat and data files manually. The roster hack removes the need to update the character heads package file.

I highly recommend the Game Changer mod, found in [The Outsider's Crypt (XML2)](https://marvelmods.com/forum/index.php/topic,11561.0.html) on Marvel Mods, as it adds many quality of life improvements to the game, like saving and changing your team from anywhere. These features are excellent for testing early development mods. It is also a great resource for understanding how to unlock characters from the start in the `new_game.py` script.

## Check Your Install

If something is wrong, go through this list first:

1. `dinput.dll` is the renamed **32-bit** loader, in the real game folder next to `XMen2.exe`. The file in `Ultimate-ASI-Loader-NoPDB.zip` is called `dinput8.dll`, is the only file in the zip, and is 32-bit (2,797,288 bytes for v10.0.0). The 64-bit build is a separate download; do not use it.
2. `xml2_roster.log` appears after you start the game. If it does not, the loader or the plugin is not loading.
3. The log says `all 58 sites match` and `0 write failure(s)` in every patch line, and `herostat entries:` matches your roster. A healthy log lists all your hero names.
4. With the controller mod: the controller's original `dinput.dll` was renamed to `xml2_controller.dll`, and `xml2_chain.log` ends with `chained: the game now talks to the controller mod.` If the log says `has no DirectInputCreateEx`, the controller mod is not a DirectInput proxy installed as `dinput.dll`, so the controller chain cannot link it.
5. Lines starting `first-chance access violation` in the log are not always a crash: the game handles some of them itself. They only matter if the game actually crashed.
6. A hero is shown but locked: check the id in `new_game.py`. It must match the herostat id exactly, for example `unlockCharacter("Beast_hero", "")` or use the unlock all characters cheat.
7. A hero or NPC does not show in a cutscene or mission: look for `NAME TABLE FULL` in the log. `NameTable=on` is already the largest size the plugin allows. Also check that `CharPool=on` and that the log says `character manager created` with `64 pool slots, 480 names`. If the log says `NAME TABLE FULL`, or those lines are missing or show smaller numbers, please report it with your log.

## Reporting a problem

Open an issue on the Issues tab and choose *Bug report* (crash or won't start) or *Limit or glitch report* (powers, talents, bolt-ons, team bonuses, skins). Attach `xml2_roster.log` (next to `rosterhack.ini`, usually the plugins folder), `xml2_chain.log` (in the game folder, next to `XMen2.exe`) and `rosterhack.ini` (or a screenshot of its settings), and say how you run the game (direct install or a mod manager). Save and close all open documents first, including `rosterhack.ini` in Notepad, because Notepad can show settings that the game did not actually load. See `SUPPORT.md`. Never share personally identifiable information in the bug and limit reports.

## Uninstall

Delete `rosterhack.asi` and `rosterhack.ini` from the plugins folder (and `dinput.dll` if no other plugin needs it). Saves made with more than 21 heroes may not load without the plugin.

## Next steps

Still to be tested: the Danger Room, long missions, and identify herostat limit breaches. No Danger Room limit tied to the number of heroes was found in the game's code, but it has not been played with a full roster.

## Credits

- Ultimate ASI Loader: ThirteenAG
- Thanks to Marco from Marvel Mods for the Mod Organizer 2 crash reporting, troubleshooting, and testing.
- Limits and other game research: BaconWizard17 and [Marvel Mods](https://marvelmods.com), from their documentation of the game's limits and overall inner-workings of the game, including compatibility with MO2.
- XML2 32 Roster Hack: project design and testing by viktorsosiktor. Reverse engineering and code written with Claude (Anthropic).

The roster hack code is released into the public domain under the [Unlicense](LICENSE). Do whatever you like with it. The public-domain dedication covers only the roster hack's own files. Ultimate ASI Loader (`dinput.dll`) and XML2 Modern Controller are separate downloads under their own license.

*Unofficial fan modification. Not affiliated with or endorsed by Marvel, Activision or Raven Software. X-Men Legends II and all related names and characters belong to their respective owners.*
