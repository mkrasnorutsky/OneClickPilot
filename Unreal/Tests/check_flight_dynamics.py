"""Exercise the real shared JSBSim C ABI without Unreal Engine."""
import argparse,ctypes as c,math
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('library',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
class Initial(c.Structure):
    _fields_=[('size',c.c_uint32)]+[(n,c.c_double) for n in ['lat','lon','terrain','alt','heading','speed']]+[('running',c.c_int)]
class State(c.Structure):
    _fields_=[('size',c.c_uint32)]+[(n,c.c_double) for n in ['lat','lon','alt','roll','pitch','heading','speed','vertical','rpm','elapsed']]
lib=c.CDLL(str(a.library.resolve()))
lib.ocp_fdm_create.argtypes=[c.c_char_p,c.c_char_p,c.POINTER(Initial),c.c_char_p,c.c_size_t];lib.ocp_fdm_create.restype=c.c_void_p
for name,args,result in [('destroy',[c.c_void_p],None),('step',[c.c_void_p,c.c_double],c.c_int),('set',[c.c_void_p,c.c_char_p,c.c_double],c.c_int),('get',[c.c_void_p,c.c_char_p,c.POINTER(c.c_double)],c.c_int),('state',[c.c_void_p,c.POINTER(State)],c.c_int),('error',[c.c_void_p],c.c_char_p)]:
    f=getattr(lib,'ocp_fdm_'+name);f.argtypes=args;f.restype=result
assert lib.ocp_fdm_abi()==1
resources=str(root/'Content/FlightModels').encode()
def create(alt=1500,speed=90):
    initial=Initial(c.sizeof(Initial),50.83138094,-.29991299,7,alt,20,speed,1)
    error=c.create_string_buffer(1024)
    handle=lib.ocp_fdm_create(resources,b'ocp172p',c.byref(initial),error,len(error))
    assert handle,error.value
    return handle
def state(h):
    s=State();s.size=c.sizeof(State);assert lib.ocp_fdm_state(h,c.byref(s)),lib.ocp_fdm_error(h)
    assert all(math.isfinite(getattr(s,n)) for n,_ in s._fields_)
    return s
def set(h,path,v):assert lib.ocp_fdm_set(h,path.encode(),v),path
for elevator in [-.3,.3]:
    h=create()
    try:
        set(h,'fcs/throttle-cmd-norm',.7);set(h,'fcs/elevator-cmd-norm',elevator)
        assert lib.ocp_fdm_step(h,-1)==-1
        assert lib.ocp_fdm_step(h,float('nan'))==-1
        assert not lib.ocp_fdm_set(h,b'fcs/elevator-cmd-norm',float('nan'))
        assert not lib.ocp_fdm_set(h,b'does/not/exist',1)
        start=state(h)
        for _ in range(120):assert lib.ocp_fdm_step(h,1/120)==1
        end=state(h)
        assert abs(end.elapsed-start.elapsed-1)<1e-8
        assert end.rpm>0,end.rpm
        if elevator<0:up=end.pitch
        else:down=end.pitch
    finally:lib.ocp_fdm_destroy(h)
assert up>down,(up,down)
h=create(11.6,0)
try:
    set(h,'fcs/left-brake-cmd-norm',1);set(h,'fcs/right-brake-cmd-norm',1)
    for _ in range(1200):assert lib.ocp_fdm_step(h,1/120)==1
    s=state(h);assert 7<s.alt<16,s.alt
    assert s.speed<5,s.speed
    before=s.elapsed
    assert lib.ocp_fdm_step(h,100)==30
    assert abs(state(h).elapsed-before-.25)<1e-8
finally:lib.ocp_fdm_destroy(h)
for pedal in (-1, 1):
    h = create(11.6, 0)
    try:
        set(h, 'fcs/throttle-cmd-norm', .4)
        set(h, 'fcs/rudder-cmd-norm', pedal)
        set(h, 'fcs/steer-cmd-norm', pedal)
        for _ in range(360):
            assert lib.ocp_fdm_step(h, 1/120) == 1
        steer = c.c_double()
        assert lib.ocp_fdm_get(h, b'fcs/steer-pos-deg', c.byref(steer))
        assert abs(steer.value - pedal * 10) < .1, steer.value
        heading_change = (state(h).heading - 20 + 180) % 360 - 180
        assert pedal * heading_change > 1, (pedal, heading_change)
        print(f"PASS: pedal {pedal:+}, nosewheel {steer.value:+.1f} deg, heading change {heading_change:+.1f} deg")
    finally:
        lib.ocp_fdm_destroy(h)
# A centered practice aircraft should not develop a steep bank immediately.
h = create(1500, 65)
try:
    set(h, 'fcs/throttle-cmd-norm', 1)
    for _ in range(240):
        assert lib.ocp_fdm_step(h, 1/120) == 1
    assert abs(state(h).roll) < 6, state(h).roll
finally:
    lib.ocp_fdm_destroy(h)
h = create(11.6, 0)
try:
    set(h, 'fcs/throttle-cmd-norm', 1)
    for _ in range(22):
        assert lib.ocp_fdm_step(h, .25) == 30
    assert 15 < state(h).speed < 48
    for _ in range(56):
        if state(h).speed > 48:
            set(h, 'fcs/elevator-cmd-norm', -.26)
        assert lib.ocp_fdm_step(h, .25) == 30
    departure = state(h)
    assert departure.alt > 75 and departure.speed > 45, (departure.alt, departure.speed)
    print(f"PASS: runway departure to {departure.alt:.0f} ft at {departure.speed:.0f} kt")
finally:
    lib.ocp_fdm_destroy(h)
for _ in range(10):h=create();lib.ocp_fdm_destroy(h)
print('PASS: shared ABI, C172 load, finite flight state, pitch response, running engine, parked ground contact, fixed stepping, invalid input, bounded catch-up, repeated lifetime')
