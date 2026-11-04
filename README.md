# OneClickPilot

An offline iOS flight simulator using Unreal Engine 5.7.4, JSBSim, and Fly2Gether.

See [the Unreal project guide](Unreal/README.md) for build, signing, controls, and
scenery instructions. The Unreal project is at
[Unreal/OneClickPilot.uproject](Unreal/OneClickPilot.uproject). The build tools
live in the repository’s scripts directory.

## Build and debug on Mac with Xcode

Install Unreal Engine 5.7.4 with iOS platform support, Xcode, CMake, and Python 3.
From Terminal:

```sh
git clone --recurse-submodules https://github.com/mkrasnorutsky/OneClickPilot.git
cd OneClickPilot
python3 scripts/build_unreal_jsbsim.py
python3 scripts/unreal_project.py generate
open "Unreal/OneClickPilot (Mac).xcworkspace"
```

For an existing clone, run `git submodule update --init --recursive` before
building the JSBSim framework.

In Xcode, select **OneClickPilotEditor** and **My Mac**, then click **Run**
(⌘R). The generated DebugGame scheme launches the practice map under LLDB
with sound disabled. To stop at startup, set a breakpoint in
`AOneClickPilotPawn::BeginPlay()` before clicking Run.

Close other OneClickPilot sessions first to free the Fly2Gether ports.
When switching between clones on the same Mac, regenerate the workspace from
the clone you plan to debug because Unreal's generated build state is shared.

## Optional Australian Airfield scenery

The default practice map works after a fresh clone. To build the Australian
Airfield locally, first finish the Mac setup above and close Unreal Editor.

1. Add [Rural Australia](https://www.fab.com/listings/1c1467ce-a2f5-4be1-8988-9069f90a8571)
   to your Fab library. In Epic Games Launcher, choose **Unreal Engine → Library →
   Rural Australia → Add to Project**. Use a blank staging project. If the
   launcher flags Unreal 5.7.4 as incompatible, select **Show All Projects** and
   choose the **5.4** asset version.
2. Copy the staging project's entire `Content/RuralAustralia` folder into
   `Unreal/Content` in your OneClickPilot clone. Check that
   `Unreal/Content/RuralAustralia/Maps/RuralAustralia_Example_03.umap` exists.
   Keep OneClickPilot's own Unreal configuration.
3. Run these commands from the repository root.

   ```sh
   python3 scripts/unreal_project.py editor
   python3 scripts/unreal_project.py rural
   python3 scripts/unreal_project.py open
   ```
4. In Unreal Editor, open **Content Drawer → Maps → Practice_AustralianAirfield**
   and press **Play**. The Cessna uses the existing runway and instructor circuit.

The setup creates the local airfield and reusable terrain tile. The Fab assets
and generated scenery stay outside Git under the [Fab license terms](https://www.fab.com/eula).
The Australian Airfield workflow has been tested on Mac. The standard practice
map is still the default for iOS builds.
