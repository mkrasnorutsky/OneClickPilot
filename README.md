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
