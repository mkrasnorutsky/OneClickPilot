# SPDX-License-Identifier: MIT
"""Exercise the live Fly2Gether TCP receiver with the C172P parked on the runway."""
import argparse,json,socket,time

class Props:
    def __init__(self,host):
        self.socket=socket.create_connection((host,15401),timeout=5)
        self.socket.settimeout(5);self.reader=self.socket.makefile('rb');self.send('data')
    def send(self,line):self.socket.sendall((line+'\r\n').encode())
    def get(self,path):
        self.send('get '+path);value=self.reader.readline().decode().strip()
        if value.lower() in ['true','false']:return float(value.lower()=='true')
        if not value:raise RuntimeError('Missing property: '+path)
        return float(value)
    def set(self,path,value):self.send('set '+path+' '+str(value))
    def run(self,name):self.send('run '+name)
    def close(self):self.reader.close();self.socket.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--host',required=True);args=parser.parse_args()
    client=Props(args.host);restore={};passed=[]
    def expect(path,value,tolerance=.001):
        actual=client.get(path)
        if abs(actual-value)>tolerance:raise AssertionError((path,actual,value))
    try:
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            try:
                if client.get('/sim/oneclickpilot/ready'):break
            except RuntimeError:pass
            time.sleep(.2)
        else:raise RuntimeError('Aircraft did not finish startup')
        if client.get('/velocities/groundspeed-kt')>.5:raise RuntimeError('Park the aircraft before running control tests')
        for path in ['/controls/switches/master-bat','/controls/switches/master-alt','/controls/switches/master-avionics','/controls/lighting/landing-lights','/controls/lighting/taxi-light','/controls/lighting/nav-lights','/controls/lighting/beacon','/controls/lighting/strobe','/controls/anti-ice/pitot-heat','/controls/engines/current-engine/carb-heat']:
            try:restore[path]=client.get(path)
            except RuntimeError:
                if path!='/controls/engines/current-engine/carb-heat':raise
                restore[path]=0 # C172P creates this cockpit input on first use.
            for value in [0,1]:client.set(path,value);expect(path,value)
            client.set(path,restore[path]);passed.append(path)
        path='/controls/switches/magnetos';restore[path]=client.get(path)
        for value in [1,2,3]:client.set(path,value);expect(path,value)
        passed.append('magnetos RIGHT/LEFT/BOTH')
        path='/instrumentation/altimeter/setting-inhg';restore[path]=client.get(path)
        client.run('oneclickpilot-baro-up');expect(path,restore[path]+.01)
        client.run('oneclickpilot-baro-down');expect(path,restore[path]);passed.append('altimeter commands')
        for path,value in [('/instrumentation/comm/frequencies/standby-mhz',118.5),('/instrumentation/nav/frequencies/standby-mhz',110.5),('/instrumentation/transponder/inputs/knob-mode',5)]:
            restore[path]=client.get(path);client.set(path,value);expect(path,value);passed.append(path)
        brake='/controls/gear/brake-parking';restore[brake]=client.get(brake)
        client.run('oneclickpilot-parking-brake');expect(brake,1-int(restore[brake]))
        expect('/sim/oneclickpilot/parking-brake-manual',1)
        client.run('oneclickpilot-parking-brake');expect(brake,restore[brake]);passed.append('parking brake toggle/manual ownership')
        engine=int(client.get('/controls/engines/active-engine'))
        for name,value in [('throttle',.08),('mixture',.9)]:
            path='/controls/engines/current-engine/'+name;restore[path]=client.get(path)
            client.set(path,value);time.sleep(.3);expect(path,value)
            expect('/controls/engines/engine[%d]/%s'%(engine,name),value)
        passed.append('TCP engine axes reach physical engine')
        packet=','.join(map(str,[0,0,0,0]+[.08,.5,.9]*4+[0,0,0,0]))+'\n'
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
            for _ in range(5):udp.sendto(packet.encode(),(args.host,15501));time.sleep(.1)
        expect('/controls/engines/current-engine/throttle',.08)
        expect('/controls/engines/current-engine/mixture',.9)
        expect('/controls/engines/engine[%d]/throttle'%engine,.08)
        expect('/controls/engines/engine[%d]/mixture'%engine,.9)
        passed.append('UDP engine axes survive aircraft system updates')
        offset=2 if client.get('/fdm/jsbsim/fuel/tank') else 0
        for name,left,right in [('left',1,0),('right',0,1),('both',1,1)]:
            client.run('oneclickpilot-fuel-'+name)
            expect('/consumables/fuel/tank[%d]/selected'%offset,left)
            expect('/consumables/fuel/tank[%d]/selected'%(offset+1),right)
        passed.append('fuel selector LEFT/RIGHT/BOTH')
        starter='/controls/switches/starter'
        for _ in range(3):client.set(starter,1);expect(starter,1);time.sleep(.25)
        client.set(starter,0);expect(starter,0);passed.append('starter hold/release')
        peer=Props(args.host);peer.set(starter,1)
        if peer.get(starter)!=1:raise AssertionError('Peer starter press was not applied')
        expect(starter,1);peer.close();time.sleep(.5);expect(starter,0);passed.append('starter disconnect release')
        client.set(starter,1);expect(starter,1);time.sleep(2.6);expect(starter,0);passed.append('starter heartbeat timeout')
        print(json.dumps({'host':args.host,'passed':passed,'custom_received':client.get('/sim/fly2gether/custom-received')},indent=2))
    finally:
        for path,value in restore.items():client.set(path,value)
        client.set('/controls/switches/starter',0);client.close()
if __name__=='__main__':main()
