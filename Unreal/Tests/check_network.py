# SPDX-License-Identifier: MIT
"""Live transport regression: packet validation, TCP framing, views and reconnects."""
import argparse,importlib.util,socket,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--host',default='127.0.0.1');a=p.parse_args()
spec=importlib.util.spec_from_file_location('custom',Path(__file__).with_name('fly2gether_custom.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
c=m.Props(a.host)
def eq(path,value):
    got=c.get(path);assert abs(got-value)<1e-5,(path,got,value)
paths=['/sim/current-view/heading-offset-deg','/sim/current-view/pitch-offset-deg','/sim/current-view/roll-offset-deg']
control_paths=['/controls/flight/'+name for name in ['aileron','elevator','rudder','elevator-trim','flaps']]
control_paths += ['/controls/engines/engine[%d]/%s'%(i,name) for i in range(4) for name in ['throttle','propeller-pitch','mixture']]
control_paths += ['/controls/gear/gear-down','/controls/engines/current-engine/throttle','/controls/engines/current-engine/mixture','/controls/gear/brake-left','/controls/gear/brake-right']
restore={path:c.get(path) for path in paths+control_paths}
try:
    # A command split across sends must be applied only once complete.
    c.socket.sendall(b'set /sim/oneclickpilot/network-test 12')
    time.sleep(.1);c.socket.sendall(b'.5\r\nget /sim/oneclickpilot/network-test\r\n')
    assert float(c.reader.readline())==12.5
    for heading,pitch in [(0,0),(0,30),(0,-30),(90,0),(270,0),(0,-55)]:
        c.set(paths[0],heading);c.set(paths[1],pitch);time.sleep(.15)
        eq(paths[0],heading);eq(paths[1],pitch)
    packet=[.25,-.3,.15,.2]+[.08,.5,.9]*4+[.2,.3,0,0]
    def send(text):
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as u:u.sendto(text.encode(),(a.host,15501))
        time.sleep(.2)
    send(','.join(map(str,packet))+'\n');eq('/controls/flight/elevator-trim',.2)
    import json
    chunks=json.loads((Path(__file__).resolve().parents[1]/'Config/Fly2GetherProtocol.json').read_text())['fields']
    # The single-engine C172P resets unused engine channels during its system update.
    for i in [*range(7),16,17]:eq(chunks[i]['node'],packet[i])
    before=c.get('/sim/fly2gether/received-packets')
    for bad in ['1,2,3\n',','.join(['nan']+list(map(str,packet[1:]))),','.join(map(str,packet))+',1','1'+','.join(map(str,packet))+'garbage']:
        send(bad);eq('/controls/flight/elevator-trim',.2);eq('/controls/flight/aileron',.25)
    eq('/sim/fly2gether/received-packets',before)
    # The fixed-gear C172P system overwrites gear-down each frame; test its packet acceptance, not retraction.
    flaps=c.get('/controls/flight/flaps')
    packet[18]=1;packet[19]=.1;send(','.join(map(str,packet))+'\n')
    eq('/controls/flight/flaps',min(1,flaps+.1))
    packet[18]=0;packet[19]=0;packet[3]=2;send(','.join(map(str,packet))+'\n')
    eq('/controls/flight/flaps',min(1,flaps+.1));eq('/controls/flight/elevator-trim',1)
    c.send('run nasal');assert c.reader.readline().strip()==b'ERR unsupported'
    for _ in range(12):
        peer=m.Props(a.host);peer.get('/controls/flight/elevator-trim');peer.close();time.sleep(.05)
    print('PASS: TCP fragmentation, six views, C172 UDP fields, relative flaps, axis clamping, malformed UDP atomic rejection, restricted commands, reconnects')
finally:
    for path,value in restore.items():c.set(path,value)
    c.close()
