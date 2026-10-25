# XML2 32 Roster Hack - Beta v0.01

Raises the hero limit of the 2005 PC version of **X-Men Legends II** from 21 playable heroes to as many as 31 playable heroes plus the default entry (32 hero slots).

> **Beta.** Lightly tested. Back up your saves and game files before installing. Saves made with more than 21 heroes may not load without this plugin.

It is an ASI plugin for [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader). It patches the running game in memory only. `XMen2.exe` on disk is never modified. To uninstall, delete the plugin files.

## What it does

- removes the herostat crash at 21 heroes
- allows up to 32 heroes in the roster
- fixes portraits for the 22nd hero and beyond
- enlarges the character pool (63 slots instead of 31) and the name table (400 instead of 297) so NPC and cutscene characters still appear
- works with save and load

## Requirements

- The 2005 PC version of X-Men Legends II (`XMen2.exe`). It may not work with some modified executables. The plugin checks the game's bytes first and does nothing if they do not match.
- Character mods. The roster hack only raises the limits; the heroes come from your own mods. Download character mods individually or as packages (for example AXE, BHE and MUE), as you choose, from the [Marvel Mods forum](https://marvelmods.com/forum/index.php/topic,753.0.html). Executables included with some character packages may not work with the roster hack in its current form.
- Ultimate ASI Loader, renamed `dinput.dll`. **Not included** in this download; see "Get the ASI loader" below. If you already have a recent Ultimate ASI Loader installed, you do not need another. Other ASI loaders are untested and may not work with the roster hack.

## Install

1. Back up your saves and your data folder.
2. Install the character mods you want first, individually or as packages.
3. Get the ASI loader (next section) and put its `dinput.dll` in the game folder, next to `XMen2.exe` (skip if you already have an ASI loader). If you use a controller mod installed as `dinput.dll`, read "Controller mod chain" first.
4. Put `rosterhack.asi` and `rosterhack.ini` together in **one** of:
   - the `plugins` folder in the game folder (recommended), or
   - right next to `XMen2.exe`.
5. Start the game. `xml2_roster.log` appears beside `rosterhack.ini`.

The plugin looks for `rosterhack.ini` beside the `.asi` first, then in the game folder. If it finds none, it writes a default one next to the `.asi`.

Some mod managers may not work with the roster hack, including OpenHeroSelect (OHS and OHSGUI) from Marvel Mods. Mod Organizer 2 is untested.

See [release/README.txt](release/README.txt) for the full readme (settings, troubleshooting, known unknowns).

## Get the ASI loader

1. Go to the [Ultimate ASI Loader releases](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) and download `Ultimate-ASI-Loader-NoPDB.zip` (tested with v10.0.0).
2. Extract the zip.
3. X-Men Legends II is a 32-bit game, so use the 32-bit `dinput8.dll` from the zip.
4. **Rename it to `dinput.dll`**. The game asks for that exact name.
5. Put `dinput.dll` in the game folder, next to `XMen2.exe`. If Windows asks to replace an existing `dinput.dll`, choose No.

If the game folder already has a `dinput.dll` (for example from a controller mod), do not overwrite it. See the controller section below.

## Controller mod chain (optional)

Only needed if you use a modern-controller mod that is installed as `dinput.dll` **and** you want the roster hack. Both want the name `dinput.dll`, so the controller mod must be moved out of the way. `controllerchain.asi` (included in the release zip) then links it back in. The controller mod itself is **not** included. It is [XML2 Modern Controller](https://github.com/andremeloo/xml2-modern-controller) by andremeloo; download it from its own page. Keep its `true-analog.ini` next to `XMen2.exe` as its readme says; only the `DINPUT.dll` is renamed.

Do the steps in this order so nothing is overwritten:

1. Copy the controller mod's original `dinput.dll` somewhere safe as a backup.
2. In the game folder, **rename** the controller mod's `dinput.dll` to `xml2_controller.dll` (exactly that name, next to `XMen2.exe`, not in `plugins`).
3. Only now copy the Ultimate ASI Loader's `dinput.dll` into the game folder. Copying it first would replace the controller mod's file.
4. Put `controllerchain.asi` in `plugins` (or next to `XMen2.exe`).
5. Start the game. `xml2_chain.log` appears next to `XMen2.exe`; a good log ends with `chained: the game now talks to the controller mod.`

**Compatibility:** the chain was tested with XML2 Modern Controller v0.5.0-beta.2, the current version in October 2026. It does not depend on a specific version, only on the controller mod being a DirectInput proxy that exports `DirectInputCreateEx`, but newer releases are not guaranteed to work. If a future release changes how it hooks the game, the chain may need to be updated. The source is in `src/controllerchain.cpp` and is public domain, so anyone is free to adapt it; reports and fixes are welcome in Issues.

Without `xml2_controller.dll` the chain does nothing and the game runs normally. It only works if the controller mod is a DirectInput proxy installed as `dinput.dll`. To go back to the controller mod alone, delete the loader's `dinput.dll` and the two `.asi` files, then rename `xml2_controller.dll` back to `dinput.dll`.

## Settings

`rosterhack.ini`, section `[Roster]`:

| Key | Range | Default | Meaning |
|---|---|---|---|
| Capacity | 22 to 32 | 32 | Number of hero slots |
| Playable | `auto`, or 21 to Capacity-1 | `auto` | `auto` follows the number of heroes in your herostat, so nothing needs changing when you add heroes. A number sets it by hand (old behaviour). |
| CharPool | 31 to 64 | 63 | Characters that can exist in a level at once (31 = original) |
| NameTable | 297 to 480 | 400 | Name table size (297 = original) |

## Build

x86 Native Tools Command Prompt:

```
cl /nologo /LD /O2 /MT src/rosterhack.cpp /Fe:rosterhack.asi
cl /nologo /LD /O2 /MT src/controllerchain.cpp /Fe:controllerchain.asi
```

## Credits

- Ultimate ASI Loader: ThirteenAG
- XML2 Roster Hack: project design and testing by viktorsosiktor. Reverse engineering and code written with Claude (Anthropic).

The roster hack code is released into the public domain under the [Unlicense](LICENSE). Do whatever you like with it. The public-domain dedication covers only the roster hack's own files. Ultimate ASI Loader (`dinput.dll`) is a separate download under its own license.

*Unofficial fan modification. Not affiliated with or endorsed by Marvel, Activision or Raven Software. X-Men Legends II and all related names and characters belong to their respective owners.*
