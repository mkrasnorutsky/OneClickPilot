#!/usr/bin/env python3
"""Prepare, build and cook the UE5 OneClickPilot migration (iOS product; Mac editor host)."""
# SPDX-License-Identifier: MIT
from pathlib import Path
import xml.etree.ElementTree as ET
import argparse,json,os,re,subprocess
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
def fix_xcode_paths():
    # Xcode canonicalizes the project directory through our Intermediate symlink.
    project_files = PROJECT / 'Intermediate/ProjectFiles'
    physical = project_files.resolve()
    if physical == project_files:
        return

    def rebase(match):
        relative = match[2]
        if (physical / relative).is_file():
            return match[0]
        target = Path(os.path.normpath(project_files / relative))
        if not target.is_file():
            return match[0]
        return match[1] + os.path.relpath(target, physical)

    for config in (project_files / 'XcconfigsMac').glob('*.xcconfig'):
        text = config.read_text()
        updated = re.sub(r'(?m)^((?:INFOPLIST_FILE|CODE_SIGN_ENTITLEMENTS) = )((?:\.\./)+[^\n]+)$', rebase, text)
        if updated != text:
            config.write_text(updated)

    # The same symlink also affects the source references used by Xcode search.
    for target in ('OneClickPilot (Mac)', 'OneClickPilotEditor (Mac)', 'OneClickPilot (IOS)'):
        project = project_files / f'{target}.xcodeproj/project.pbxproj'
        if not project.is_file():
            continue
        text = project.read_text()
        updated = re.sub(r'(path = ")((?:\.\./)+[^"]+)(?="; sourceTree = SOURCE_ROOT;)', rebase, text)
        if updated != text:
            project.write_text(updated)

def configure_mac_debug_scheme():
    scheme = (PROJECT / 'Intermediate/ProjectFiles/OneClickPilotEditor (Mac).xcodeproj'
              / 'xcshareddata/xcschemes/OneClickPilotEditor.xcscheme')
    tree = ET.parse(scheme)
    build = tree.find('BuildAction')
    build.set('buildImplicitDependencies', 'NO')
    launch = tree.find('LaunchAction')
    launch.set('buildConfiguration', 'DebugGame')
    args = launch.find('CommandLineArguments')
    if args is None:
        args = ET.SubElement(launch, 'CommandLineArguments')
    for entry in args.findall('CommandLineArgument'):
        if entry.get('argument') == '/Game/Maps/Practice':
            args.remove(entry)
    existing = {entry.get('argument') for entry in args}
    for argument in ('/Game/Maps/Practice_Editable', '-game', '-windowed', '-ResX=1280', '-ResY=960', '-nosound'):
        if argument not in existing:
            ET.SubElement(args, 'CommandLineArgument', argument=argument, isEnabled='YES')
    ET.indent(tree)
    tree.write(scheme, encoding='utf-8', xml_declaration=True)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['preflight','generate','editor','assets','bake','rural','open','ios']);p.add_argument('--reuse-cooked',action='store_true');p.add_argument('--engine');p.add_argument('--simulator',action='store_true');a=p.parse_args()
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
    if a.action=='generate':
        run(engine/'Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh','-project='+str(project),'-game')
        fix_xcode_paths()
        configure_mac_debug_scheme()
    elif a.action=='editor':run(build,'OneClickPilotEditor','Mac','Development','-Project='+str(project),'-WaitMutex','-NoUBA')
    elif a.action in ('assets', 'bake', 'rural'):
        script = {'assets': 'bootstrap.py', 'bake': 'bake_practice.py',
                  'rural': 'setup_rural_australia.py'}[a.action]
        run(commandlet,project,'-unattended','-nop4','-nosplash','-run=pythonscript','-script='+str(PROJECT/'Editor'/script),'-stdout','-FullStdOutLogOutput')
    elif a.action=='open':run('open','-a',engine/'Engine/Binaries/Mac/UnrealEditor.app',project)
    elif a.action=='ios':
        env=os.environ.copy()
        args=[engine/'Engine/Build/BatchFiles/RunUAT.sh','BuildCookRun','-project='+str(project),'-noP4','-platform=IOS','-clientconfig=Development',*(['-skipbuild','-skipcook'] if a.reuse_cooked else ['-build','-cook']),'-stage','-pak','-unattended','-utf8output','-UbtArgs=-NoUBA']
        if a.simulator:args.append('-clientarchitecture=iossimulator')
        run(*args,env=env)
if __name__=='__main__':main()
