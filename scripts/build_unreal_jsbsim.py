#!/usr/bin/env python3
"""Build JSBSim behind a dynamic C ABI for the Unreal iOS migration."""
# SPDX-License-Identifier: MIT
from pathlib import Path
import argparse,os,plistlib,shutil,subprocess,zipfile
import sys
ROOT=Path(__file__).resolve().parents[1]/'Unreal'
STATE=Path.home()/'Library/Developer/OneClickPilot'
SOURCE=STATE/'source-repositories/jsbsim-upstream'
WORK=STATE/'UnrealFlightDynamics'
REVISION='66f7fafc07fe13e7ede9db20873030fed2881713'
NAME='OneClickFlightDynamics'
def run(*args):
    print('+',*map(str,args),flush=True)
    subprocess.run(list(map(str,args)),check=True)
def resources():
    dest=ROOT/'Content/FlightModels';dest.mkdir(parents=True,exist_ok=True)
    run(sys.executable, ROOT/'FlightDynamics/generate_practice_model.py', dest)
    shutil.copy2(SOURCE/'COPYING',dest/'JSBSim-COPYING')
    notices=dest/'Licenses';notices.mkdir(exist_ok=True)
    for source,name in [(SOURCE/'COPYING','JSBSim-LGPL.txt'),(SOURCE/'src/GeographicLib/LICENSE.txt','GeographicLib-MIT.txt'),(SOURCE/'src/simgear/xml/COPYING','XML-COPYING.txt'),(ROOT/'FlightDynamics/LICENSE','Adapter-MIT.txt')]:shutil.copy2(source,notices/name)
    print('Staged the OneClickPilot model and JSBSim notices',flush=True)
def build(label,sdk,arch,jobs):
    build=WORK/'build'/label
    flags=['-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=OFF','-DBUILD_PYTHON_MODULE=OFF','-DBUILD_DOCS=OFF','-DSKBUILD=ON','-DCMAKE_MACOSX_BUNDLE=OFF','-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_OSX_ARCHITECTURES='+arch,'-DCMAKE_OSX_DEPLOYMENT_TARGET='+('14.0' if sdk=='macosx' else '18.0')]
    if sdk!='macosx':flags+=['-DCMAKE_SYSTEM_NAME=iOS']
    run('cmake','-S',SOURCE,'-B',build,*flags)
    run('cmake','--build',build,'--target','libJSBSim','--parallel',jobs)
    framework=WORK/'slices'/label/(NAME+'.framework');framework.mkdir(parents=True,exist_ok=True)
    headers=framework/'Headers';headers.mkdir(exist_ok=True);shutil.copy2(ROOT/'FlightDynamics/FlightDynamics.h',headers/(NAME+'.h'))
    modules=framework/'Modules';modules.mkdir(exist_ok=True)
    (modules/'module.modulemap').write_text('framework module '+NAME+' { umbrella header "'+NAME+'.h" export * }\n')
    sdkpath=subprocess.check_output(['xcrun','--sdk',sdk,'--show-sdk-path'],text=True).strip()
    target=arch+('-apple-macos14.0' if sdk=='macosx' else '-apple-ios18.0'+('-simulator' if sdk=='iphonesimulator' else ''))
    exports=build/'exports.txt';exports.write_text(''.join('_ocp_fdm_'+name+'\n' for name in ['abi','create','destroy','step','set','get','state','error']))
    run('xcrun','clang++','-target',target,'-isysroot',sdkpath,'-std=c++17','-O2','-dynamiclib','-fvisibility=hidden','-I'+str(SOURCE/'src'),ROOT/'FlightDynamics/FlightDynamics.cpp',build/'src/libJSBSim.a','-install_name','@rpath/'+NAME+'.framework/'+NAME,'-Wl,-exported_symbols_list,'+str(exports),'-o',framework/NAME)
    info={'CFBundleIdentifier':'com.mkrasnorutsky.oneclickpilot.flightdynamics','CFBundleName':NAME,'CFBundleExecutable':NAME,'CFBundlePackageType':'FMWK','CFBundleVersion':'1','CFBundleShortVersionString':'1.0','MinimumOSVersion':'14.0' if sdk=='macosx' else '18.0','CFBundleSupportedPlatforms':[{'macosx':'MacOSX','iphoneos':'iPhoneOS','iphonesimulator':'iPhoneSimulator'}[sdk]]}
    (framework/'Info.plist').write_bytes(plistlib.dumps(info))
    return framework
def merge(name,frameworks):
    dest=WORK/'platforms'/name/(NAME+'.framework')
    shutil.copytree(frameworks[0],dest,dirs_exist_ok=True)
    if len(frameworks)>1:run('lipo','-create',*[p/NAME for p in frameworks],'-output',dest/NAME)
    return dest
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--host-only',action='store_true');p.add_argument('--jobs',type=int,default=8);a=p.parse_args()
    if not (SOURCE/'.git').is_dir():
        SOURCE.parent.mkdir(parents=True,exist_ok=True)
        run('git','clone','--depth','1','--branch','v1.3.3','https://github.com/JSBSim-Team/jsbsim.git',SOURCE)
    assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=SOURCE,text=True).strip()==REVISION
    resources()
    if a.host_only:
        build('mac-'+os.uname().machine,'macosx',os.uname().machine,a.jobs);return
    variants=[('ios-arm64','iphoneos','arm64'),('sim-arm64','iphonesimulator','arm64'),('sim-x86_64','iphonesimulator','x86_64'),('mac-arm64','macosx','arm64'),('mac-x86_64','macosx','x86_64')]
    slices={label:build(label,sdk,arch,a.jobs) for label,sdk,arch in variants}
    platforms={'IOS':merge('IOS',[slices['ios-arm64']]),'Simulator':merge('Simulator',[slices['sim-arm64'],slices['sim-x86_64']]),'Mac':merge('Mac',[slices['mac-arm64'],slices['mac-x86_64']])}
    artifact=WORK/(NAME+'.xcframework')
    if artifact.exists():shutil.rmtree(artifact)
    run('xcodebuild','-create-xcframework',*[item for f in platforms.values() for item in ['-framework',f]],'-output',artifact)
    notices=artifact/'Licenses';notices.mkdir();shutil.copy2(SOURCE/'COPYING',notices/'JSBSim-LGPL.txt');shutil.copy2(ROOT/'FlightDynamics/LICENSE',notices/'Adapter-MIT.txt')
    for platform,framework in platforms.items():
        out=ROOT/'ThirdParty'/platform;out.mkdir(parents=True,exist_ok=True)
        if platform=='Mac':
            link=out/(NAME+'.framework')
            if link.is_symlink():link.unlink()
            if not link.exists():link.symlink_to(framework,target_is_directory=True)
        else:
            archive=WORK/(platform+'.embeddedframework.zip')
            with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
                for file in framework.rglob('*'):
                    if file.is_file():z.write(file,Path(NAME+'.embeddedframework')/file.relative_to(framework.parent))
            link=out/(NAME+'.embeddedframework.zip')
            if link.is_symlink():link.unlink()
            if not link.exists():link.symlink_to(archive)
    print('Built',artifact)
if __name__=='__main__':main()
