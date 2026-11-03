"""Verify the backend is dynamic, correctly sliced and exports only the stable C ABI."""
# SPDX-License-Identifier: MIT
from pathlib import Path
import argparse,plistlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--artifact',type=Path,default=Path.home()/'Library/Developer/OneClickPilot/UnrealFlightDynamics/OneClickFlightDynamics.xcframework');a=p.parse_args()
info=plistlib.loads((a.artifact/'Info.plist').read_bytes())
assert len(info['AvailableLibraries'])==3
expected={'_ocp_fdm_'+name for name in ['abi','create','destroy','step','set','get','state','error']}
for entry in info['AvailableLibraries']:
    framework=a.artifact/entry['LibraryIdentifier']/entry['LibraryPath'];binary=framework/'OneClickFlightDynamics'
    archs=set(entry['SupportedArchitectures']);assert archs==({'arm64'} if entry['SupportedPlatform']=='ios' and 'SupportedPlatformVariant' not in entry else {'arm64','x86_64'})
    subprocess.run(['lipo',str(binary),'-verify_arch',*archs],check=True)
    for arch in archs:
        symbols=subprocess.check_output(['nm','-arch',arch,'-gUj',str(binary)],text=True)
        assert set(symbols.split())==expected,(entry,arch,symbols)
        headers=subprocess.check_output(['otool','-arch',arch,'-hv',str(binary)],text=True)
        assert 'DYLIB' in headers,headers
        dependencies=subprocess.check_output(['otool','-arch',arch,'-L',str(binary)],text=True)
        assert '@rpath/OneClickFlightDynamics.framework/OneClickFlightDynamics' in dependencies
    print('PASS dynamic framework:',entry['LibraryIdentifier'],sorted(archs))
print('PASS: C-only shared-library boundary; no JSBSim C++ exports')
