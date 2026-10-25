XML2 ROSTER HACK - BETA v0.01
=============================
(X-Men Legends II: up to 32 heroes)

*** BETA DISCLAIMER ***
This is an early beta release. It has been tested only lightly (see KNOWN
UNKNOWNS below) and may have bugs, crashes, or problems that have not been
found yet. Use it at your own risk. BACK UP YOUR SAVES AND GAME FILES BEFORE
INSTALLING. Saves made with more than 21 heroes may not load without this
plugin. Please report problems with your xml2_roster.log (see TROUBLESHOOTING).


WHAT IT DOES
------------
The original PC X-Men Legends II stops working at 21 playable heroes. This
plugin raises that limit. In memory only, while the game runs, it:

  - removes the herostat crash at 21 heroes
  - allows up to 32 heroes in the roster (31 playable plus the default entry).
    The default man can be swapped for a playable character, but it is
    unclear how that will perform in every situation in the game.
  - fixes the portraits for the 22nd hero and beyond
  - enlarges the game's character pool (63 slots instead of 31) and its name
    table (400 entries instead of 297), so cutscene characters and NPC
    versions of heroes still appear
  - works with save and load

It never changes XMen2.exe on disk. To uninstall, delete the plugin files.


REQUIRED
--------
  - The 2005 PC version of X-Men Legends II (XMen2.exe). It may not work
    with some modified executables. The plugin checks the game's bytes
    first and does nothing if they do not match.
  - Character mods. The roster hack only raises the limits; the heroes come
    from your own mods. Download character mods individually or as packages
    (for example AXE, BHE and MUE), as you choose, from the Marvel Mods forum
    (marvelmods.com/forum/index.php/topic,753.0.html). Executables included
    with some character packages may not work with the roster hack in its
    current form.
  - Ultimate ASI Loader, renamed dinput.dll (by ThirteenAG, free, open
    source). It is NOT included in this download. See "GET THE ASI LOADER"
    below. If you already have a recent Ultimate ASI Loader installed, you do
    not need another. Other ASI loaders are untested and may not work with
    the roster hack.

Character mods are NOT included in this download. Add them to your game
yourself if they are not already installed.


INSTALL
-------
This download contains only the roster hack: rosterhack.asi, rosterhack.ini
and the optional controllerchain.asi. It does NOT include the ASI loader
(dinput.dll) or any game data, scripts or herostat files. Those come from the
loader's own project page and from your own game and mods.

1. BACK UP your saves and your data folder first.
2. Install the character mods you want first, individually or as packages.
3. Get the ASI loader (see GET THE ASI LOADER below) and put its dinput.dll
   in your X-Men Legends 2 game folder, the one with XMen2.exe in it. Skip
   this if you already have a recent Ultimate ASI Loader there. If you use a
   controller mod installed as dinput.dll, read CONTROLLER MOD CHAIN first.
4. Put rosterhack.asi and rosterhack.ini in ONE of these places:

   Option A (recommended): the "plugins" folder in the game folder.
       (game folder)\plugins\rosterhack.asi
       (game folder)\plugins\rosterhack.ini
       Create the plugins folder if it does not exist.

   Option B: right next to XMen2.exe.
       (game folder)\rosterhack.asi
       (game folder)\rosterhack.ini

   Keep the .asi and the .ini together. Use only ONE copy of rosterhack.asi
   (do not put it in both places).
5. Start the game. xml2_roster.log appears in the same folder as
   rosterhack.ini.

The plugin looks for rosterhack.ini beside the .asi first, then in the game
folder. If there is none, it writes a default one next to the .asi.

If you already have Ultimate ASI Loader or other plugins, keep your existing
dinput.dll (do not use two). Only one dinput.dll can be in the game folder.

YOUR HEROES (not part of this download)
The roster hack only raises the limits. Which heroes you have, and whether
they are unlocked, comes from your own mods: herostat.engb and
scripts\menus\new_game.py. A mod manager may overwrite those files later.
If a hero is shown but locked, the id in new_game.py is wrong. It must match
the herostat id exactly, for example
unlockCharacter("Beast_hero", "")

NOTE: Some mod managers may not work with the roster hack, including
OpenHeroSelect (OHS and OHSGUI) from Marvel Mods. Mod Organizer 2 is
untested.

GET THE ASI LOADER
------------------
1. Go to github.com/ThirteenAG/Ultimate-ASI-Loader/releases and download
   Ultimate-ASI-Loader-NoPDB.zip (tested with v10.0.0).
2. Extract the zip.
3. X-Men Legends II is a 32-bit game, so use the 32-bit dinput8.dll from the
   zip.
4. RENAME it to dinput.dll. The game asks for that exact name.
5. Put dinput.dll in the game folder, next to XMen2.exe. If Windows asks to
   replace an existing dinput.dll, choose No.

If the game folder already has a dinput.dll (for example from a controller
mod), do not overwrite it. See CONTROLLER MOD CHAIN below.


CONTROLLER MOD CHAIN (optional)
------------------------------
Only needed if you use a modern-controller mod that is installed as
dinput.dll AND you want to use the roster hack. Both want the file name
dinput.dll, so the controller mod has to be moved out of the way. The chain
plugin (controllerchain.asi, included here) then links the controller mod back
in. The controller mod itself is NOT included. It is XML2 Modern Controller by
andremeloo (github.com/andremeloo/xml2-modern-controller). Download it from
its own page. Keep its true-analog.ini next to XMen2.exe as its readme says;
only its DINPUT.dll is renamed.

Do the steps in this order, so that nothing is overwritten:

1. Copy the controller mod's original dinput.dll to a safe place as a backup.
2. In the game folder, RENAME the controller mod's dinput.dll to
       xml2_controller.dll
   (exactly that name, in the same folder as XMen2.exe, not in plugins).
3. Only now copy the Ultimate ASI Loader's dinput.dll into the game folder.
   (If you copy it before step 2, it replaces the controller mod's file.)
4. Put controllerchain.asi in the plugins folder (or next to XMen2.exe).
5. Start the game. The chain writes xml2_chain.log next to XMen2.exe. A good
   log ends with "chained: the game now talks to the controller mod."

Notes:
  - Compatibility: the chain was tested with XML2 Modern Controller
    v0.5.0-beta.2, the current version in October 2026. It depends only on the
    controller mod being a DirectInput proxy that exports DirectInputCreateEx,
    but newer releases are not guaranteed to work. If a future release
    changes how it hooks the game, the chain may need to be updated. The
    source (controllerchain.cpp) is public domain, so anyone is free to adapt
    it.
  - Without xml2_controller.dll the chain does nothing and the game runs
    normally, without the controller mod.
  - It only works if the controller mod is a DirectInput proxy installed as
    dinput.dll. Otherwise the log says "has no DirectInputCreateEx".
  - Never keep two dinput.dll files in the game folder.
  - To go back to the controller mod alone: delete the loader's dinput.dll,
    delete rosterhack.asi and controllerchain.asi, and rename
    xml2_controller.dll back to dinput.dll.


UNINSTALL
---------
Delete rosterhack.asi and rosterhack.ini from the plugins folder (and
dinput.dll if no other plugin needs it). Saves made with more than 21 heroes
may not load without the plugin.


SETTINGS (rosterhack.ini, section [Roster])
-------------------------------------------
Capacity   Number of hero slots. 22 to 32. Default 32.
Playable   auto (default), or a number from 21 to Capacity-1.
           auto follows the number of heroes in your herostat, so nothing
           needs changing when you add heroes. A number sets it by hand (the
           old behaviour). If you ask for more than Capacity-1 the log says
           "does not fit ... using 31" and uses the smaller number.
           If your ini still has a number from an older version, change it
           to auto.
CharPool   Character pool size. 31 to 64. Default 63.
           31 turns this part off (original game size).
NameTable  Name table size. 297 to 480. Default 400.
           297 turns this part off (original game size).

Notes:
  - Each hero uses about two names (the hero, and a "<Name>Simple" NPC
    version). Names are registered when the game starts and reset for each
    level. If the table fills up, later names get the default model. The
    log then shows a line starting "NAME TABLE FULL". Raise NameTable if you
    see it.
  - The character pool is how many characters can exist in one level at the
    same time. The log shows the peak ("character pool: N of M slots in
    use"). Raise CharPool if the peak reaches the maximum.
  - Names are not case sensitive.
  - Keep Capacity at least as large as the number of heroes in
    herostat.engb (the default entry counts as one).


TROUBLESHOOTING
---------------
  - Look at xml2_roster.log first. A healthy log says "0 write failure(s)"
    in every patch line and lists all your hero names.
  - "settings folder:" near the top of the log shows which rosterhack.ini
    was used.
  - A hero is shown but locked: check the id in new_game.py.
  - A hero or NPC shows the default model in a cutscene: look for
    "NAME TABLE FULL" in the log and raise NameTable.
  - Do not run old test or probe .asi files together with this plugin. They
    hook the same places and will conflict.
  - When reporting a problem, include xml2_roster.log and rosterhack.ini.


KNOWN UNKNOWNS
--------------
  - Tested: new game, cutscenes, first mission, first hub, stash, store,
    review terminal, save and load, with up to 32 heroes. Tested with a
    direct install only (files copied into the game folder).
  - Playable=auto was tested at startup with 25 and 32 heroes.
  - Not fully tested: Mod Organizer 2, the Danger Room, long missions, and
    every hero in every mission.
  - Swapping the default man for a playable character (the 32nd hero) works
    in the situations tested, but it is unclear how it will perform in
    every situation in the game.
  - The herostat file may have its own limits (such as unique talents).
    Those are not changed by this plugin.


CREDITS
-------
  - Ultimate ASI Loader: ThirteenAG
  - XML2 Roster Hack: project design and testing by viktorsosiktor. Reverse
    engineering and code written with Claude (Anthropic).


LICENSE AND LEGAL
-----------------
The roster hack (rosterhack.asi and its source code) is released into the
public domain (the Unlicense). Do whatever you like with it.

The public-domain dedication covers only the roster hack's own files. It does
not apply to Ultimate ASI Loader (dinput.dll) by ThirteenAG, which is a
separate download under its own license (MIT).

Unofficial fan modification. Not affiliated with or endorsed by Marvel,
Activision or Raven Software. X-Men Legends II and all related names and
characters belong to their respective owners.
