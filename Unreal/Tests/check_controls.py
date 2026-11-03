"""Build and test the exact UE-independent session and transport with real JSBSim."""
# SPDX-License-Identifier: MIT
from pathlib import Path
import argparse,importlib.util,socket,subprocess,tempfile,time
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--library',type=Path,default=Path.home()/'Library/Developer/OneClickPilot/UnrealFlightDynamics/platforms/Mac/OneClickFlightDynamics.framework/OneClickFlightDynamics');a=p.parse_args()
# Never accidentally test another running FlightGear/OneClickPilot instance.
for kind,port in [(socket.SOCK_STREAM,15401),(socket.SOCK_DGRAM,15501)]:
    with socket.socket(socket.AF_INET,kind) as check:
        try:check.bind(('0.0.0.0',port))
        except OSError:raise SystemExit('Stop the other OneClickPilot instance using port '+str(port))
with tempfile.TemporaryDirectory(prefix='oneclick-unreal-controls-') as folder:
    binary=Path(folder)/'server';logfile=Path(folder)/'server.log'
    subprocess.run(['clang++','-std=c++17','-pthread','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(ROOT/'FlightDynamics'),'-I'+str(ROOT/'Source/OneClickPilot/Core'),str(ROOT/'Tests/headless_flight.cpp'),str(ROOT/'Source/OneClickPilot/Core/OneClickTransport.cpp'),str(ROOT/'Source/OneClickPilot/Core/FlightSession.cpp'),'-o',str(binary)],check=True)
    with logfile.open('w') as log:
        proc=subprocess.Popen([str(binary),str(a.library),str(ROOT/'Content/FlightModels')],stdout=log,stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                if proc.poll() is not None:raise RuntimeError(logfile.read_text())
                if 'READY JSBSim' in logfile.read_text():break
                time.sleep(.1)
            else:raise RuntimeError('Server did not become ready')
            for test in ['fly2gether_custom.py','check_network.py']:
                subprocess.run(['python3',str(ROOT/'Tests'/test),'--host','127.0.0.1'],check=True,timeout=90)
            spec=importlib.util.spec_from_file_location('controls',ROOT/'Tests/fly2gether_custom.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
            client=module.Props('127.0.0.1')
            for path,value,backend in [('/controls/flight/elevator',-.25,'fcs/elevator-cmd-norm'),('/controls/flight/aileron',.2,'fcs/aileron-cmd-norm'),('/controls/flight/rudder',.1,'fcs/rudder-cmd-norm'),('/controls/engines/current-engine/throttle',.4,'fcs/throttle-cmd-norm')]:
                client.set(path,value);time.sleep(.05)
                assert abs(client.get('/fdm/jsbsim/'+backend)-value)<1e-6,(path,backend)
            client.close()
        finally:
            proc.terminate();proc.wait(timeout=10)
        assert proc.returncode==0,logfile.read_text()
    print('PASS: real JSBSim control routing, ASan/UBSan session/transport, clean shutdown')
