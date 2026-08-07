# Mac OS 9 / PowerPC launcher (Metrowerks CodeWarrior 8)

A **native classic Mac OS 9** (PPC/CFM) build of the Amulets & Armor launcher.
The other desktop ports are Qt6 (`mac/`) / .NET WinForms (`AALauncher/`); Qt
doesn't exist on classic Mac OS, so this is a hand-written **Toolbox** app
(CodeWarrior 8, InterfaceLib, MSL) that presents the launcher UI and starts the
OS 9 builds of the game, server, and script compiler that sit alongside it.

## Files

| File | Purpose |
|------|---------|
| `aalauncher_os9.c` | The launcher: Toolbox init, window + button bar, Apple/File/Options menu, cooperative `WaitNextEvent` loop, and `LaunchApplication` of the sibling apps. |
| `AALauncher.r` | Rez resources — the "could not launch" alert (`ALRT`/`DITL` 128 with a `^0` ParamText slot), a `SIZE` memory partition (4 MB pref / 2 MB min), and a `vers`. Compile with `Rez -i /Developer/Headers/FlatCarbon -o AALauncher.rsrc AALauncher.r` and add the `.rsrc` to the project. |
| `AALauncher_OS9_Prefix.h` | CW prefix (force-includes `<MacHeaders.c>`, relaxes the pragmas). |
| `AALauncher.mcp` | CodeWarrior 8 project, based on the "Mac OS Classic / C Toolbox" stationery. Access paths are machine-local — retarget on open. |

## Building

Based on the stationery: **remove** its `SimpleAlert.c`, **add**
`aalauncher_os9.c` + the compiled resources, set the **Prefix File** to
`AALauncher_OS9_Prefix.h`. Headless builds via cmdide
(`cmdide -proj -r -b -e AALauncher.mcp`). Verified on a real PowerBook G4.

## How it launches the apps (no argv on classic Mac)

Buttons call `LaunchApplication` (Process Manager) on the app of the given name
in the launcher's **own folder** — so ship the launcher next to `AmuletsAndArmor`,
`AAServer`, and `AAScriptCompiler`:

- **Play Single Player** → `AmuletsAndArmor`
- **Start A&A Server** → `AAServer`
- **Script Compiler** → `AAScriptCompiler` (opens its own SIOUX arg dialog)

Classic Mac has no `argv`, so the Unix launcher's `AA <ip> <port>` /
`AAServer <port>` command-line hand-off doesn't apply here — configuration
(server IP/port, display) is passed via shared config files the apps read
(display already works this way through `resolution.ini`).

## Status

**Phase 1 (this):** native window + button bar + menu + launching the three
sibling apps + Quit. Working end-to-end on hardware.

**Later phases:** the Network dialog + server-IP/port config file + LAN server
discovery (Open Transport); the Display Settings dialog (writes `resolution.ini`);
and the embedded web view (the framed placeholder up top) via **macsurf** — either
embedded (large CW8 integration) or by launching macsurf as a helper app.
