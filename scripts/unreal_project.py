#!/usr/bin/env python3
"""Prepare, build and cook the UE5 OneClickPilot migration (iOS product; Mac editor host)."""
# SPDX-License-Identifier: MIT
from pathlib import Path
import argparse,json,os,subprocess
BASE=Path(__file__).resolve().parents[1]
PROJECT=BASE/'Unreal'
STATE=Path.home()/'Library/Developer/OneClickPilot/Unreal-5.7.4'
def engine_path(explicit=None,allow_platform_update=False):
    candidates=[Path(explicit)] if explicit else []
    registered=set()
    if os.environ.get('UE_ROOT'):candidates.append(Path(os.environ['UE_ROOT']))
    manifest=Path.home()/'Library/Application Support/Epic/UnrealEngineLauncher/LauncherInstalled.dat'
    if manifest.is_file():
        for item in json.loads(manifest.read_text()).get('InstallationList',[]):
            if item.get('AppName')=='UE_5.7':
                installed=Path(item['InstallLocation']);candidates.append(installed);registered.add(installed.resolve())
    candidates.append(Path('/Users/Shared/Epic Games/UE_5.7'))
    pending=[]
    manifests=Path.home()/'Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests'
    for file in manifests.rglob('*.item'):
        try:
            item=json.loads(file.read_text())
            if item.get('AppName')=='UE_5.7' and item.get('bIsIncompleteInstall'):
                pending.append(Path(item['InstallLocation']).resolve())
        except (OSError,ValueError,KeyError):continue
    for path in candidates:
        if path.resolve() in pending and not (allow_platform_update and path.resolve() in registered):continue
        if (path/'Engine/Build/Build.version').is_file() and (path/'Engine/Binaries/Mac/UnrealEditor.app').is_dir():return path.resolve()
    raise SystemExit('Unreal Engine 5.7.4 installation is not complete. Finish it in Epic Games Launcher, including iOS platform support.')
def run(*args,env=None):
    print('+',*map(str,args),flush=True)
    subprocess.run(list(map(str,args)),check=True,env=env)
def prepare():
    STATE.mkdir(parents=True,exist_ok=True)
    for name in ['Binaries','Intermediate','Saved','DerivedDataCache','Build']:
        destination=STATE/name;destination.mkdir(exist_ok=True)
        link=PROJECT/name
        if link.is_symlink() and link.resolve()!=destination.resolve():link.unlink()
        if not link.exists():link.symlink_to(destination,target_is_directory=True)
    for path in ['ThirdParty/IOS/OneClickFlightDynamics.embeddedframework.zip','ThirdParty/Simulator/OneClickFlightDynamics.embeddedframework.zip','ThirdParty/Mac/OneClickFlightDynamics.framework/OneClickFlightDynamics']:
        if not (PROJECT/path).exists():raise SystemExit('Missing native framework. Run build_unreal_jsbsim.py first.')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['preflight','generate','editor','assets','open','ios']);p.add_argument('--reuse-cooked',action='store_true');p.add_argument('--engine');p.add_argument('--simulator',action='store_true');a=p.parse_args()
    prepare();engine=engine_path(a.engine,allow_platform_update=a.action in ['editor','assets','generate','open'])
    version=json.loads((engine/'Engine/Build/Build.version').read_text())
    if tuple(version[key] for key in ('MajorVersion','MinorVersion','PatchVersion'))!=(5,7,4):
        raise SystemExit('OneClickPilot requires Unreal Engine 5.7.4')
    print('Engine:',engine,'version:',version,flush=True)
    if a.action=='preflight':return
    project=PROJECT/'OneClickPilot.uproject'
    build=engine/'Engine/Build/BatchFiles/Mac/Build.sh'
    editor=engine/'Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
    commandlet=engine/'Engine/Binaries/Mac/UnrealEditor-Cmd'
    if not commandlet.exists():commandlet=editor
    if a.action=='generate':run(engine/'Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh','-project='+str(project),'-game')
    elif a.action=='editor':run(build,'OneClickPilotEditor','Mac','Development','-Project='+str(project),'-WaitMutex','-NoUBA')
    elif a.action=='assets':run(commandlet,project,'-unattended','-nop4','-nosplash','-run=pythonscript','-script='+str(PROJECT/'Editor/bootstrap.py'),'-stdout','-FullStdOutLogOutput')
    elif a.action=='open':run('open','-a',engine/'Engine/Binaries/Mac/UnrealEditor.app',project)
    elif a.action=='ios':
        env=os.environ.copy()
        args=[engine/'Engine/Build/BatchFiles/RunUAT.sh','BuildCookRun','-project='+str(project),'-noP4','-platform=IOS','-clientconfig=Development',*(['-skipbuild','-skipcook'] if a.reuse_cooked else ['-build','-cook']),'-stage','-pak','-unattended','-utf8output','-UbtArgs=-NoUBA']
        if a.simulator:args.append('-clientarchitecture=iossimulator')
        run(*args,env=env)
if __name__=='__main__':main()
