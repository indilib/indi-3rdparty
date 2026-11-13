"""Exercise the real INDI executable against a loopback mount and standard snoop XML.
No indiserver, physical roof or physical mount is used.
"""
import re, os, socket, subprocess, sys, tempfile, threading, time, xml.etree.ElementTree as ET
DEVICE='10Micron Native Motion'
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
port=listener.getsockname()[1];commands=[];server_errors=[];server_done=threading.Event()
state={'weather_mode':2 if '--weather-mode' in sys.argv and '--legacy' not in sys.argv else 0,'guide_rate':7.5,'temperature':8.5,'pressure':980.2,'lat':44.9,'lon':267.1,'elevation':300,'jd':2461318,'status':5,'ra':3.0,'dec':42.0,'flag':0,'count':0,'next':None,'targetRA':3.0,'targetDec':42.0}
if '--startup' in sys.argv:state.update(status=0,guide_rate=6.25,temperature=12.3,pressure=999.2,utc_offset=5.5)
lock=threading.Lock()
def serve():
    try:
        listener.settimeout(.2)
        while not server_done.is_set():
            try:c,_=listener.accept()
            except socket.timeout:continue
            with c:
                c.settimeout(30);buf=b''
                while True:
                    data=c.recv(1024)
                    if not data:break
                    buf+=data
                    while b'#' in buf:
                        cmd,buf=buf.split(b'#',1);cmd+=b'#'
                        with lock:commands.append(cmd)
                        reply=None
                        if cmd==b':U2#':pass
                        elif cmd==b':GVP#':reply=b'10micron GM1000HPS#'
                        elif cmd==b':GVN#':reply=b'2.15.1#' if '--legacy' in sys.argv else b'3.4#'
                        elif cmd==b':WSG#':
                            assert '--legacy' not in sys.argv,cmd
                            reply=str(state['weather_mode']).encode()+b'#'
                        elif cmd.startswith(b':WSS'):
                            assert re.fullmatch(rb':WSS[012]#',cmd),cmd
                            if state.pop('drop_mode',False):reply=None
                            elif state.pop('reject_mode',False):reply=b'0'
                            else:
                                if not state.pop('ignore_mode',False):state['weather_mode']=int(cmd[4:5])
                                reply=b'1'
                        elif cmd==b':GVZ#':reply=b'Q-TYPE2016#'
                        elif cmd==b':Ginfo#':
                            if state['count']:
                                state['count']-=1
                                if state['count']==0:
                                    state['status']=state['next'];state['flag']=0
                                    if state['next']==0:state['ra']=state['targetRA'];state['dec']=state['targetDec']
                            reply=f"{state['ra']},{state['dec']},W,1,45,{state['jd']},{state['status']},{state['flag']}#".encode()
                            if state.pop('drop_poll',False):reply=None
                        elif cmd in (b':Glms#',b':Glmt#',b':Gmte#',b':GMF#'):reply={b':Glms#':b'3#',b':Glmt#':b'4#',b':Gmte#':b'711#',b':GMF#':b'1#'}[cmd]
                        elif cmd==b':Guaf#':reply=b'1'
                        elif cmd==b':GTsid#':reply=b'2'
                        elif cmd==b':CMCFG0#':reply=b'0#'
                        elif cmd==b':CM#':
                            state.update(ra=state['targetRA'],dec=state['targetDec']);reply=b'Coordinates     matched#'
                        elif cmd.startswith(b':Sr'):
                            h,m,sec=map(float,cmd[3:-1].decode().split(':'));state['targetRA']=h+m/60+sec/3600;reply=b'1'
                        elif cmd.startswith(b':Sd'):
                            text=cmd[3:-1].decode();d,rest=text[1:].split('*');m,sec=map(float,rest.split(':'));state['targetDec']=(float(d)+m/60+sec/3600)*(-1 if text[0]=='-' else 1);reply=b'1'
                        elif cmd in (b':MS#',b':MSfs2#'):
                            time.sleep(state.pop('slew_delay',0));state.update(status=6,flag=1,count=state.pop('slew_count',3),next=0);reply=b'0'
                        elif cmd==b':PO#':
                            if '--limit-unpark' in sys.argv:state.update(status=9,flag=0,count=0)
                            else:state.update(status=3,flag=1,count=3,next=7)
                        elif cmd==b':hP#':state.update(status=2,flag=1,count=3,next=5)
                        elif cmd==b':AP#':
                            if not state.get('ignore_tracking_on'):state.update(status=0,flag=0,count=0)
                        elif cmd==b':AL#':
                            if not state.get('ignore_tracking_off'):state.update(status=7,flag=0,count=0)
                        elif cmd==b':Q#':state.update(status=0,flag=0,count=0)
                        elif cmd==b':STOP#':state.update(status=2 if state['status']==2 else 6,flag=1,count=3,next=5 if state['status'] in (2,3,5) else 1)
                        elif cmd.startswith(b':Mg'):
                            assert cmd[3:4] in (b'n',b's',b'e',b'w') and 1<=int(cmd[4:-1])<=9999
                        elif cmd in (b':RG#',b':RC#',b':RM#',b':RS#'):pass
                        elif cmd in (b':Mn#',b':Ms#',b':Me#',b':Mw#'):state.update(status=6,flag=1)
                        elif cmd in (b':Qn#',b':Qs#',b':Qe#',b':Qw#'):state.update(status=0,flag=0)
                        elif cmd==b':GG#':
                            offset=state.get('utc_offset',5.0);minutes=round(abs(offset)*60)
                            reply=f"{'-' if offset<0 else '+'}{minutes//60:02d}:{minutes%60:02d}:00.0#".encode()
                        elif cmd==b':Ggui#':reply=f"{state['guide_rate']}#".encode()
                        elif cmd.startswith(b':Rg'):state['guide_rate']=float(cmd[3:-1])
                        elif cmd==b':GRTMP#':reply=f"{state['temperature']:+.1f}#".encode()
                        elif cmd==b':GRPRS#':reply=f"{state['pressure']:.1f}#".encode()
                        elif cmd.startswith(b':SRTMP'):
                            assert re.fullmatch(rb':SRTMP[+-][0-9]{3}\.[0-9]#',cmd),cmd
                            if state['weather_mode']!=0 or state.pop('reject_temperature',False):reply=b'0'
                            else:state['temperature']=float(cmd[6:-1]);reply=b'1'
                        elif cmd.startswith(b':SRPRS'):
                            assert re.fullmatch(rb':SRPRS[0-9]{4}\.[0-9]#',cmd),cmd
                            if state.pop('bad_pressure_reply',False):reply=b'X'
                            elif state['weather_mode']!=0 or state.pop('reject_pressure',False):reply=b'0'
                            else:state['pressure']=float(cmd[6:-1])+state.pop('pressure_readback_offset',0);reply=b'1'
                        elif cmd in (b':RT0#',b':RT1#',b':RT2#'):pass
                        elif cmd.startswith((b':RR',b':RD')):reply=b'1'
                        elif cmd.startswith(b':SUDT'):
                            import datetime
                            dt=datetime.datetime.strptime(cmd[5:-1].decode(),'%Y-%m-%d,%H:%M:%S').replace(tzinfo=datetime.timezone.utc)
                            state['jd']=dt.timestamp()/86400+2440587.5;reply=b'1'
                        elif cmd.startswith(b':SG'):reply=b'1'
                        elif cmd.startswith((b':St',b':Sg')):
                            text=cmd[3:-1].decode().replace('*',':');d,m,sec=map(float,text.split(':'))
                            v=(abs(d)+m/60+sec/3600)*(-1 if text[0]=='-' else 1)
                            state['lat' if cmd.startswith(b':St') else 'lon']=v if cmd.startswith(b':St') else (-v)%360;reply=b'1'
                        elif cmd.startswith(b':Sev'):state['elevation']=float(cmd[4:-1]);reply=b'1'
                        elif cmd in (b':Gt#',b':Gg#'):
                            v=state['lat'] if cmd==b':Gt#' else -(state['lon'] if state['lon']<=180 else state['lon']-360)
                            ticks=round(abs(v)*36000);reply=f"{'-' if v<0 else '+'}{ticks//36000:03d}:{ticks//600%60:02d}:{ticks/10%60:04.1f}#".encode()
                        elif cmd==b':Gev#':reply=f"{state['elevation']:+.1f}#".encode()
                        else:raise AssertionError(cmd)
                        if reply:c.sendall(reply)
    except Exception as e:server_errors.append(e)
    finally:listener.close()
thread=threading.Thread(target=serve,daemon=True);thread.start()
config_dir=tempfile.TemporaryDirectory(prefix='10u-indi-test-');env=os.environ.copy();env['INDICONFIG']=os.path.join(config_dir.name,'motion_config.xml')
# INDI park data uses the child's home separately from INDICONFIG.
env['HOME']=config_dir.name
os.makedirs(os.path.join(config_dir.name,'.indi'),exist_ok=True)
if '--weather-mode' in sys.argv:
    with open(env['INDICONFIG'],'w') as f:
        f.write('<INDIDriver><newSwitchVector device="'+DEVICE+'" name="NATIVE_WEATHER_UPDATE"><oneSwitch name="MANUAL">On</oneSwitch></newSwitchVector></INDIDriver>')
if '--reconcile' in sys.argv:
    with open(env['INDICONFIG'],'w') as f:
        f.write('<INDIDriver><newTextVector device="'+DEVICE+'" name="NATIVE_PARK_SAFETY"><oneText name="INTERRUPTED">1</oneText></newTextVector></INDIDriver>')
p=subprocess.Popen([sys.argv[1]],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
events=[];reader_errors=[];stderr=[]
def read():
    parser=ET.XMLPullParser(events=('end',));parser.feed('<stream>')
    try:
        for line in p.stdout:
            if line.lstrip().startswith("<?xml"):continue
            parser.feed(line)
            for _,elem in parser.read_events():
                if elem.tag.endswith('Vector') or elem.tag in ('message','getProperties','delProperty'):
                    events.append((elem.tag,dict(elem.attrib),{e.attrib.get('name'): (e.text or '').strip() for e in elem}));elem.clear()
    except Exception as e:reader_errors.append(e)
reader=threading.Thread(target=read,daemon=True);reader.start()
threading.Thread(target=lambda:stderr.extend(p.stderr),daemon=True).start()
def send(tag,name,values,device=DEVICE,state=None):
    root=ET.Element(tag,device=device,name=name)
    if state:root.set('state',state)
    child={'newTextVector':'oneText','setTextVector':'oneText','newSwitchVector':'oneSwitch','setSwitchVector':'oneSwitch','newNumberVector':'oneNumber','setLightVector':'oneLight','defSwitchVector':'defSwitch'}[tag]
    for key,value in values.items():ET.SubElement(root,child,name=key).text=str(value)
    p.stdin.write(ET.tostring(root,encoding='unicode')+'\n');p.stdin.flush()
def wait(name,state=None,value=None,start=0,timeout=8):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        for i,event in enumerate(events[start:],start):
            tag,attrs,values=event
            if tag.startswith('set') and attrs.get('name')==name and attrs.get('device')==DEVICE and (state is None or attrs.get('state')==state) and (value is None or all(values.get(k)==v for k,v in value.items())):return i
        if p.poll() is not None:raise AssertionError(('driver exited',p.returncode,''.join(stderr)))
        time.sleep(.01)
    raise AssertionError(('missing update',name,state,value,events[-12:],''.join(stderr)))
ENCLOSURE='Any Standard Enclosure'
def roof(opened='Ok',closed='Idle',moving='Idle',vector='Ok'):
    opened_ok=opened=='Ok' and closed=='Idle' and moving=='Idle'
    send('setSwitchVector','DOME_PARK',{'PARK':'Off' if opened_ok else 'On','UNPARK':'On' if opened_ok else 'Off'},ENCLOSURE,vector)
def configure_roof():
    send('setSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'},ENCLOSURE,'Ok')
    roof()
def wire(cmd):
    end=time.monotonic()+2
    while cmd not in commands and time.monotonic()<end:time.sleep(.01)
    assert cmd in commands,(cmd,commands[-20:])
def calibration_pulse_tests():
    # Use the normal 1-second telemetry period: pulse completion must not wait for it.
    send('newNumberVector','POLLING_PERIOD',{'PERIOD_MS':1000})
    state.update(status=0,flag=0);start=len(events)
    wait('NATIVE_STATE',value={'STATUS_CODE':'0'},start=start)
    for prop,member,ms in [('NS','N',20),('NS','S',80),('NS','N',35),('WE','E',200),('WE','W',60)]:
        start=len(events);t=time.monotonic()
        send('newNumberVector','TELESCOPE_TIMED_GUIDE_'+prop,{'TIMED_GUIDE_'+member:ms})
        wait('TELESCOPE_TIMED_GUIDE_'+prop,'Busy',start=start,timeout=.8)
        wait('TELESCOPE_TIMED_GUIDE_'+prop,'Idle',start=start,timeout=.8)
        assert time.monotonic()-t>=ms/1000-.005
        assert f':Mg{member.lower()}{ms:04d}#'.encode() in commands
        # Calibration is not a driver mode that locks the rate for the whole session.
        start=len(events);send('newNumberVector','NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':6.0 if member in ('N','E') else 7.5})
        wait('NATIVE_GUIDE_RATE','Ok',start=start,timeout=.8)
    before=len(commands);start=len(events)
    send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':0,'TIMED_GUIDE_S':0})
    wait('TELESCOPE_TIMED_GUIDE_NS','Idle',start=start)
    assert not any(c.startswith(b':Mg') for c in commands[before:])
    # Independent axis timers: DEC can complete while RA remains active.
    ra_start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_WE',{'TIMED_GUIDE_E':350})
    wait('TELESCOPE_TIMED_GUIDE_WE','Busy',start=ra_start)
    de_start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':60})
    wait('TELESCOPE_TIMED_GUIDE_NS','Busy',start=de_start)
    wait('TELESCOPE_TIMED_GUIDE_NS','Idle',start=de_start,timeout=.8)
    assert not any(a.get('name')=='TELESCOPE_TIMED_GUIDE_WE' and a.get('state')=='Idle' for _,a,_ in events[ra_start:])
    before=len(commands);start=len(events)
    send('newNumberVector','TELESCOPE_TIMED_GUIDE_WE',{'TIMED_GUIDE_E':0,'TIMED_GUIDE_W':0})
    wait('TELESCOPE_TIMED_GUIDE_WE','Busy',start=start)
    assert not any(c.startswith(b':Mg') for c in commands[before:])
    # Do not silently change the speed/displacement of an in-flight correction.
    before=len(commands);start=len(events);send('newNumberVector','NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':6})
    wait('NATIVE_GUIDE_RATE','Alert',start=start);assert not any(c.startswith(b':Rg') for c in commands[before:])
    wait('TELESCOPE_TIMED_GUIDE_WE','Idle',start=ra_start,timeout=.8)
    start=len(events);send('newNumberVector','NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':7.5})
    wait('NATIVE_GUIDE_RATE','Ok',start=start,timeout=.8)
    assert state['guide_rate']==7.5
    print('PASS: variable-duration calibration-style pulses and inter-pulse guide-rate changes with 1000-ms polling; independent RA/DEC timers')

def startup_reconciliation_tests():
    writes=(b':AP#',b':AL#',b':STOP#',b':Q#',b':PO#',b':hP#',b':RT0#',b':RT1#',b':RT2#')
    def assert_no_motion(start):
        assert not any(c in writes or c.startswith((b':MS',b':Mg',b':Rg',b':SRTMP',b':SRPRS',b':SUDT',b':Sev',b':St',b':Sg')) for c in commands[start:]),commands[start:]
    def poll(code=0):
        return wait('NATIVE_STATE',value={'STATUS_CODE':str(code),'TRACKING':'1' if code==0 else '0'},start=len(events))
    def reconnect():
        start=len(events);send('newSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'})
        wait('CONNECTION','Idle',{'DISCONNECT':'On'},start=start)
        state.update(status=0,flag=0,count=0);before=len(commands)
        start=len(events);send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'})
        wait('CONNECTION','Ok',start=start);poll();return before,time.monotonic()
    def stopped(start):
        wait('NATIVE_STATE',value={'STATUS_CODE':'1','TRACKING':'0'},start=start,timeout=8)
    # Actual first connection starts on an externally tracking mount, with no
    # roof event yet. Publish native tracking instead of commanding a reset.
    wait('NATIVE_STATE',value={'STATUS_CODE':'0','TRACKING':'1'})
    wait('NATIVE_GUIDE_RATE','Ok',{'RATE_ARCSEC_S':'6.25'})
    ref=wait('NATIVE_REFRACTION','Ok')
    assert abs(float(events[ref][2]['TEMPERATURE_C'])-12.3)<.01
    assert abs(float(events[ref][2]['PRESSURE_HPA'])-999.2)<.01
    for c in (b':Ggui#',b':GRTMP#',b':GRPRS#',b':Gt#',b':Gg#',b':Gev#'):assert c in commands,c
    clock=wait('TIME_UTC','Ok')
    assert abs(float(events[clock][2]['OFFSET'])+5.5)<.0001
    assert events[clock][2]['UTC'] and events[clock][2]['UTC']!='1970-01-01T00:00:00'
    wait('GEOGRAPHIC_COORD','Ok')
    assert b':GG#' in commands
    wait('NATIVE_ROOF_INTERLOCK','Busy')
    # Hardware capture from 2026-10-25 21:25:20 UTC, replayed as queued
    # snoop reports after Connect returns. Only device routing is substituted.
    import json
    from pathlib import Path
    captured=json.loads(Path(__file__).with_name('startup-roof-sequence.json').read_text())
    replay_start=len(commands)
    for event in captured:
        send(event['type'],event['name'],event['members'],ENCLOSURE,event['state'])
    time.sleep(.25)
    assert b':STOP#' not in commands[replay_start:], 'Captured startup roof definitions caused an unexpected STOP'
    wait('NATIVE_ROOF_INTERLOCK','Ok',start=len(events)-30)
    poll();assert_no_motion(replay_start)
    print('PASS: captured RollOff ino initial empty Ok park definition and current-open reports preserve external tracking')
    # Return to fresh discovery for the existing negative and timeout cases.
    before,t=reconnect()
    # Initial definitions are placeholders, not positive reports of closure.
    send('defSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'},ENCLOSURE,'Idle')
    send('defSwitchVector','DOME_PARK',{'PARK':'On','UNPARK':'Off'},ENCLOSURE,'Idle')
    poll();assert_no_motion(0)
    # Foreign events cannot grant permission or turn initial silence into unsafe.
    send('setSwitchVector','DOME_PARK',{'PARK':'On','UNPARK':'Off'},'Other Enclosure','Ok')
    before=len(commands);start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':42})
    wait('NATIVE_COMMAND_STATUS','Alert',start=start)
    poll();poll();assert_no_motion(0)
    # Out-of-order open/connection reports require a new park sample, but must
    # not send STOP between the individual healthy startup reports.
    roof();poll();assert_no_motion(before)
    send('setSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'},ENCLOSURE,'Ok')
    poll();assert_no_motion(before)
    roof();poll();assert_no_motion(before)
    send('defSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'Off','SHUTTER_CLOSE':'On'},ENCLOSURE,'Idle')
    poll();assert_no_motion(before)
    send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
    wait('NATIVE_ROOF_INTERLOCK','Ok',start=start);poll();assert_no_motion(before)
    # External controller changes must be read without any client write/reconnect.
    state.update(guide_rate=7.0,lat=44.95,lon=267.2,elevation=245.3,utc_offset=4.25)
    change=len(events);wire_start=len(commands)
    wait('NATIVE_GUIDE_RATE','Ok',{'RATE_ARCSEC_S':'7'},start=change,timeout=12)
    loc=wait('GEOGRAPHIC_COORD','Ok',start=change)
    assert abs(float(events[loc][2]['LAT'])-44.95)<.0001
    assert abs(float(events[loc][2]['ELEV'])-245.3)<.01
    clock=wait('TIME_UTC','Ok',{'OFFSET':'-4.2500'},start=change)
    assert_no_motion(wire_start)
    # With roof evidence reconciled, ordinary INDI Park must work immediately.
    park_start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
    wait('NATIVE_STATE',value={'STATUS_CODE':'5','PARKED':'1'},start=park_start)
    assert commands.count(b':hP#')==1 and b':STOP#' not in commands
    # Simulate an external client returning to tracking for the loss test.
    state.update(status=0,flag=0,count=0);poll()
    # Once reconciled, loss of open confirmation immediately retains old STOP policy.
    start=len(events);before=len(commands);roof(opened='Idle',closed='Ok')
    stopped(start);assert commands[before:].count(b':STOP#')==1
    configure_roof();poll(1);assert b':AP#' not in commands[before:]
    # A real MW4-style recovery park must reconcile the safety hold too.
    recovery=len(events);state.update(status=2,flag=1,count=3,next=5)
    wait('NATIVE_STATE',value={'STATUS_CODE':'5','PARKED':'1'},start=recovery)
    wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native recovery park observed; safety hold cleared'},start=recovery)
    start=len(events);send('newNumberVector','NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':6})
    wait('NATIVE_GUIDE_RATE','Ok',start=start);assert state['guide_rate']==6
    # An explicitly closed report at a fresh startup bypasses the grace period.
    before,t=reconnect();start=len(events);roof(opened='Idle',closed='Ok')
    stopped(start);assert time.monotonic()-t<3
    assert commands[before:].count(b':STOP#')==1
    # A previously advertised shutter must also be confirmed after reconnect.
    send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
    before,t=reconnect();configure_roof();poll();assert_no_motion(before)
    send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
    wait('NATIVE_ROOF_INTERLOCK','Ok',start=len(events)-1);poll();assert_no_motion(before)
    # Explicit shutter fault after readiness is immediate, not another grace window.
    start=len(events);send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'Off','SHUTTER_CLOSE':'On'},ENCLOSURE,'Alert')
    stopped(start);assert commands[before:].count(b':STOP#')==1
    # Complete silence is bounded, never an indefinite safety exemption.
    before,t=reconnect();poll();assert_no_motion(before);start=len(events)
    stopped(start);assert 4.7<=time.monotonic()-t<7.5
    assert commands[before:].count(b':STOP#')==1
    poll(1);poll(1);assert commands[before:].count(b':STOP#')==1
    before,t=reconnect()
    # Only an empty Ok definition in initial discovery gets bounded grace.
    # Empty updates, faults, explicit closure and contradictory selections do not.
    for tag,vector,members in (
        ('setSwitchVector','Ok',{'PARK':'Off','UNPARK':'Off'}),
        ('defSwitchVector','Alert',{'PARK':'Off','UNPARK':'Off'}),
        ('defSwitchVector','Busy',{'PARK':'Off','UNPARK':'Off'}),
        ('defSwitchVector','Ok',{'PARK':'On','UNPARK':'Off'}),
        ('defSwitchVector','Ok',{'PARK':'On','UNPARK':'On'})):
        start=len(events)
        send(tag,'DOME_PARK',members,ENCLOSURE,vector)
        stopped(start)
        assert time.monotonic()-t<3,(tag,vector,members)
        assert commands[before:].count(b':STOP#')==1
        before,t=reconnect()
    # Repeated empty definitions never move the original deadline.
    send('defSwitchVector','DOME_PARK',{'PARK':'Off','UNPARK':'Off'},ENCLOSURE,'Ok')
    poll();assert_no_motion(before)
    time.sleep(3)
    send('defSwitchVector','DOME_PARK',{'PARK':'Off','UNPARK':'Off'},ENCLOSURE,'Ok')
    start=len(events);stopped(start)
    assert 4.7<=time.monotonic()-t<7.5
    assert commands[before:].count(b':STOP#')==1
    # The same empty definition after valid permission is immediate loss.
    before,t=reconnect();configure_roof()
    send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
    wait('NATIVE_ROOF_INTERLOCK','Ok',start=len(events)-15)
    start=len(events)
    send('defSwitchVector','DOME_PARK',{'PARK':'Off','UNPARK':'Off'},ENCLOSURE,'Ok')
    stopped(start);assert time.monotonic()-t<3
    assert commands[before:].count(b':STOP#')==1
    before,t=reconnect()
    assert not reader_errors and not server_errors,(reader_errors,server_errors)
    print('PASS: adopt existing tracking/readbacks; delayed/out-of-order roof reports without commands; motion blocked; closed/fault/lost reports STOP; silence bounded; no replay or automatic resume')

def control_alert_reconciliation_tests():
    # Closed roof, parked and healthy. A rejected request must not remain a
    # persistent claim that the controller's mode or manual speed is bad.
    def request_both():
        start=len(events);send('newSwitchVector','TELESCOPE_TRACK_MODE',{'TRACK_SOLAR':'On'})
        mode=wait('TELESCOPE_TRACK_MODE','Alert',start=start)
        start=len(events);send('newSwitchVector','TELESCOPE_SLEW_RATE',{'2x':'On'})
        rate=wait('TELESCOPE_SLEW_RATE','Alert',start=start)
        return mode,rate
    before=len(commands);mode,rate=request_both()
    wait('TELESCOPE_TRACK_MODE','Idle',start=mode+1)
    wait('TELESCOPE_SLEW_RATE','Idle',start=rate+1)
    assert not any(c.startswith(b':RT') or c in (b':RG#',b':RC#',b':RM#',b':RS#') for c in commands[before:])
    assert any(c==b':Ginfo#' for c in commands[before:])
    assert not any(a.get('name')=='TELESCOPE_TRACK_MODE' and a.get('state')=='Ok' for _,a,_ in events[mode:])
    # Configuration replay/edits while connected must not latch false equipment
    # failures, switch transport, rename the dome, or disable its lock.
    start=len(events);send('newTextVector','NATIVE_ENDPOINT',{'HOST':'203.0.113.77','PORT':'9999'})
    wait('NATIVE_ENDPOINT','Alert',start=start)
    wait('NATIVE_ENDPOINT','Idle' if '--serial' in sys.argv else 'Ok',{'HOST':'127.0.0.1','PORT':str(port)},start=start)
    start=len(events);send('newTextVector','NATIVE_SERIAL',{'DEVICE':'/no/such/device','BAUD':'115200'})
    wait('NATIVE_SERIAL','Alert',start=start)
    serial_event=wait('NATIVE_SERIAL','Ok' if '--serial' in sys.argv else 'Idle',start=start)
    assert events[serial_event][2]['DEVICE']!='/no/such/device'
    assert events[serial_event][2]['BAUD']=='9600'
    start=len(events);send('newSwitchVector','NATIVE_TRANSPORT',{'TCP':'Off' if '--serial' not in sys.argv else 'On','SERIAL':'On' if '--serial' not in sys.argv else 'Off'})
    wait('NATIVE_TRANSPORT','Alert',start=start)
    wait('NATIVE_TRANSPORT','Ok',{'SERIAL':'On' if '--serial' in sys.argv else 'Off'},start=start)
    start=len(events);send('newTextVector','ACTIVE_DEVICES',{'ACTIVE_DOME':'Wrong Enclosure','ACTIVE_GPS':'Wrong GPS'})
    wait('ACTIVE_DEVICES','Alert',start=start)
    names_event=wait('ACTIVE_DEVICES','Ok',{'ACTIVE_DOME':ENCLOSURE},start=start)
    assert events[names_event][2]['ACTIVE_GPS']!='Wrong GPS'
    start=len(events);send('newSwitchVector','DOME_POLICY',{'DOME_IGNORED':'On','DOME_LOCKS':'Off'})
    wait('DOME_POLICY','Alert',start=start)
    wait('DOME_POLICY','Ok',{'DOME_LOCKS':'On','DOME_IGNORED':'Off'},start=start)
    # Valid configuration is not permission to move with a closed/unconfirmed roof.
    before=len(commands);start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
    wait('NATIVE_COMMAND_STATUS','Alert',start=start);assert b':PO#' not in commands[before:]
    # Native fault and unknown samples do not clear an Alert.
    for code in (8,98):
        state.update(status=code,flag=0,count=0);start=len(events)
        wait('NATIVE_STATE','Alert',{'STATUS_CODE':str(code)},start=start)
        mode,rate=request_both();marker=len(events)
        wait('NATIVE_STATE','Alert',{'STATUS_CODE':str(code)},start=marker)
        wait('NATIVE_STATE','Alert',{'STATUS_CODE':str(code)},start=len(events))
        assert not any(a.get('name') in ('TELESCOPE_TRACK_MODE','TELESCOPE_SLEW_RATE') and a.get('state')=='Idle' for _,a,_ in events[marker:])
    # Neither moving nor settling telemetry may erase these failures.
    configure_roof();state.update(status=6,flag=1,count=0);marker=len(events)
    wait('NATIVE_STATE',value={'STATUS_CODE':'6','SLEW_OR_SETTLING':'1'},start=marker)
    wait('NATIVE_STATE',value={'STATUS_CODE':'6'},start=len(events))
    assert not any(a.get('name') in ('TELESCOPE_TRACK_MODE','TELESCOPE_SLEW_RATE') and a.get('state')=='Idle' for _,a,_ in events[marker:])
    # Recovered native tracking clears to neutral, without any preset write.
    before=len(commands);marker=len(events);state.update(status=0,flag=0,count=0)
    wait('TELESCOPE_TRACK_MODE','Idle',start=marker)
    wait('TELESCOPE_SLEW_RATE','Idle',start=marker)
    assert not any(c.startswith(b':RT') or c in (b':RG#',b':RC#',b':RM#',b':RS#') for c in commands[before:])
    # Successful explicit requests retain their ordinary Ok acknowledgement;
    # polling must not repaint all presets green or destroy this request result.
    start=len(events);send('newSwitchVector','TELESCOPE_TRACK_MODE',{'TRACK_SOLAR':'On'})
    wait('TELESCOPE_TRACK_MODE','Ok',{'TRACK_SOLAR':'On'},start=start);wire(b':RT1#')
    start=len(events);send('newSwitchVector','TELESCOPE_SLEW_RATE',{'2x':'On'})
    wait('TELESCOPE_SLEW_RATE','Ok',{'2x':'On'},start=start);wire(b':RC#')
    marker=len(events);wait('NATIVE_STATE',value={'STATUS_CODE':'0'},start=marker)
    assert not any(a.get('name') in ('TELESCOPE_TRACK_MODE','TELESCOPE_SLEW_RATE') and a.get('state')=='Idle' for _,a,_ in events[marker:])
    # Actual communication failure must turn the active connection red and
    # cannot be cleared by remembered configuration or repainting.
    state['drop_poll']=True;marker=len(events)
    wait('NATIVE_STATE','Alert',start=marker,timeout=6)
    wait('NATIVE_TRANSPORT','Alert',start=marker)
    wait('NATIVE_SERIAL' if '--serial' in sys.argv else 'NATIVE_ENDPOINT','Alert',start=marker)
    assert not reader_errors and not server_errors,(reader_errors,server_errors)
    print('PASS: stale preset Alerts reconcile to Idle on healthy native poll; no writes/false Ok; faults, unknown and movement preserve Alerts; explicit requests remain functional')

def tracking_transition_tests():
    configure_roof()
    def native(code):
        state.update(status=code,flag=0,count=0)
        wait('NATIVE_STATE',value={'STATUS_CODE':str(code)},start=len(events))
    def track(on):
        start=len(events)
        send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On' if on else 'Off','TRACK_OFF':'Off' if on else 'On'})
        return start
    def park():
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
        wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start=start)
        wait('NATIVE_STATE',value={'STATUS_CODE':'5','PARKED':'1','TRACKING':'0'},start=start)
    def unpark():
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'Off','UNPARK':'On'})
        wait('NATIVE_STATE',value={'PARKED':'0','TRACKING':'0'},start=start)
    def slew():
        start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':42})
        wait('NATIVE_COMMAND_STATUS','Busy',{'COMMAND':'Slew accepted; completion requires native telemetry'},start=start)
        wait('NATIVE_STATE',value={'STATUS_CODE':'0','TRACKING':'1','SLEW_OR_SETTLING':'0'},start=start)
    # Reproduce the hardware sequence: Unpark does not have to start tracking.
    unpark();native(1);state['ignore_tracking_on']=True
    before=len(commands);t=time.monotonic();start=track(True)
    wait('TELESCOPE_TRACK_STATE','Busy',start=start)
    block=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':42})
    wait('NATIVE_COMMAND_STATUS','Alert',start=block)
    assert b':MS#' not in commands[before:]
    fail=wait('NATIVE_COMMAND_STATUS','Alert',{'COMMAND':'Tracking ON not confirmed; native_status=1 (stopped); pending request released; no command replay'},start=start,timeout=8)
    assert 4.9<=time.monotonic()-t<7.5
    wait('TELESCOPE_TRACK_STATE','Alert',{'TRACK_ON':'Off','TRACK_OFF':'On'},start=fail)
    assert commands[before:].count(b':AP#')==1
    assert not any(c in (b':STOP#',b':Q#',b':hP#',b':PO#') for c in commands[before:])
    # No reconnect or Abort needed; a normal first slew starts native tracking.
    slew();assert p.poll() is None
    # Ignored OFF is also bounded, truthful, and not automatically replayed.
    state['ignore_tracking_off']=True;before=len(commands);start=track(False)
    fail=wait('NATIVE_COMMAND_STATUS','Alert',start=start,timeout=8)
    wait('TELESCOPE_TRACK_STATE','Alert',{'TRACK_ON':'On','TRACK_OFF':'Off'},start=fail)
    assert commands[before:].count(b':AL#')==1
    state['ignore_tracking_off']=False;start=track(False)
    wait('TELESCOPE_TRACK_STATE','Idle',{'TRACK_OFF':'On'},start=start)
    wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start=start)
    # Explicit Park cancels only a tracking transition, before its deadline.
    native(1);start=track(True);wait('TELESCOPE_TRACK_STATE','Busy',start=start)
    before=len(commands);t=time.monotonic();park();assert time.monotonic()-t<4
    assert b':hP#' in commands[before:] and b':STOP#' not in commands[before:] and b':Q#' not in commands[before:]
    # The same explicit Park recovery works while an ignored OFF awaits tracking.
    unpark();native(0);state['ignore_tracking_off']=True;start=track(False)
    wait('NATIVE_COMMAND_STATUS','Busy',start=start);park();state['ignore_tracking_off']=False
    # Abort remains available and confirms the halt, followed by Park.
    unpark();native(1);start=track(True);wait('TELESCOPE_TRACK_STATE','Busy',start=start)
    start=len(events);send('newSwitchVector','TELESCOPE_ABORT_MOTION',{'ABORT':'On'})
    wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native slew halt confirmed; tracking may remain on'},start=start);park()
    # Fresh moving telemetry must NOT qualify for early Park cancellation.
    unpark();native(1);start=track(True);wait('TELESCOPE_TRACK_STATE','Busy',start=start)
    native(6);state['flag']=1;before=len(commands);block=len(events)
    send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
    wait('NATIVE_COMMAND_STATUS','Alert',start=block);assert b':hP#' not in commands[before:]
    wait('NATIVE_COMMAND_STATUS','Alert',{'COMMAND':'Tracking ON not confirmed; native_status=6 (slewing_or_stopping); pending request released; no command replay'},start=start,timeout=8)
    # Even after expiration, uncertain/moving state retains a recovery hold.
    before=len(commands);block=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':42})
    wait('NATIVE_COMMAND_STATUS','Alert',start=block);assert b':MS#' not in commands[before:]
    start=len(events);send('newSwitchVector','TELESCOPE_ABORT_MOTION',{'ABORT':'On'})
    wait('NATIVE_COMMAND_STATUS','Ok',start=start);park()
    # Return to the ordinary Off/On and slew path; no persistent lockout.
    unpark();native(1);state['ignore_tracking_on']=False;start=track(True)
    wait('TELESCOPE_TRACK_STATE','Ok',{'TRACK_ON':'On'},start=start)
    start=track(False);wait('TELESCOPE_TRACK_STATE','Idle',{'TRACK_OFF':'On'},start=start)
    wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start=start)
    start=track(True);wait('TELESCOPE_TRACK_STATE','Ok',{'TRACK_ON':'On'},start=start)
    slew();park()
    assert not reader_errors and not server_errors,(reader_errors,server_errors)
    print('PASS: ignored On/Off deadlines, truthful tracking, no replay, slew after timeout, early explicit Park, Abort, moving-state hold and ordinary tracking recovery')

def weather_mode_tests():
    prop='NATIVE_WEATHER_UPDATE'
    initial=len(commands)
    def refresh():
        start=len(events);send('newSwitchVector','NATIVE_REFRESH_CONTROLS',{'REFRESH':'On'})
        wait('NATIVE_REFRESH_CONTROLS','Ok',start=start)
    if '--legacy' in sys.argv:
        refresh()
        assert not any(a.get('name')==prop for _,a,_ in events)
        assert b':WSG#' not in commands and not any(c.startswith(b':WSS') for c in commands)
        start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':9.5})
        wait('NATIVE_REFRACTION','Ok',start=start)
        assert state['temperature']==9.5
        print('PASS: unverified older firmware keeps v0.23 refraction path without weather-mode commands')
        return
    assert any(a.get('name')==prop and v.get('AUTO_CONTINUOUS')=='On' for _,a,v in events)
    assert not any(c.startswith(b':WSS') for c in commands),commands  # Saved setting never replayed.
    send('setSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'},ENCLOSURE,'Ok')
    roof(opened='Idle',closed='Ok');refresh()
    # Both automatic policies block input writes BEFORE SRTMP/SRPRS.
    def blocked_inputs():
        before=len(commands);start=len(events)
        send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':9.5,'PRESSURE_HPA':981.2})
        wait('NATIVE_REFRACTION','Alert',start=start)
        assert not any(c.startswith((b':SRTMP',b':SRPRS',b':WSS')) for c in commands[before:]),commands[before:]
        assert state['status']==5
    blocked_inputs()
    def select(member,expected='Ok'):
        before=len(commands);start=len(events)
        send('newSwitchVector',prop,{member:'On'})
        wait(prop,expected,start=start)
        return before,start
    select('MANUAL');assert state['weather_mode']==0
    refresh();start=len(events)
    send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':9.5,'PRESSURE_HPA':981.2})
    wait('NATIVE_REFRACTION','Ok',start=start)
    assert state['temperature']==9.5 and state['pressure']==981.2
    start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':8.5,'PRESSURE_HPA':980.2})
    wait('NATIVE_REFRACTION','Ok',start=start)
    select('AUTO_NOT_TRACKING');assert state['weather_mode']==1
    refresh();blocked_inputs()
    # Invalid vectors and native-motion conflicts must not reach the mode writer.
    for values in [{'MANUAL':'On','AUTO_CONTINUOUS':'On'},{'MANUAL':'Off','AUTO_NOT_TRACKING':'Off','AUTO_CONTINUOUS':'Off'},{'BAD':'On'}]:
        before=len(commands);start=len(events);send('newSwitchVector',prop,values);wait(prop,'Alert',start=start)
        assert not any(c.startswith(b':WSS') for c in commands[before:])
    state.update(status=6,flag=1);before,start=select('MANUAL','Alert')
    assert not any(c.startswith(b':WSS') for c in commands[before:])
    state.update(status=5,flag=0)
    # Native rejection and a valid but mismatching readback retain actual mode/connection.
    state['reject_mode']=True;before,start=select('MANUAL','Alert')
    assert state['weather_mode']==1 and commands[before:].count(b':WSS0#')==1
    assert any(a.get('name')==prop and v.get('AUTO_NOT_TRACKING')=='On' for _,a,v in events[start:])
    refresh()
    state['ignore_mode']=True;before,start=select('AUTO_CONTINUOUS','Alert')
    assert state['weather_mode']==1 and commands[before:].count(b':WSS2#')==1
    refresh()
    # External changes are read, not fought: refresh and the regular 10-second sample.
    state['weather_mode']=2;refresh();wait(prop,'Ok',{'AUTO_CONTINUOUS':'On'})
    state['weather_mode']=0;start=len(events)
    wait(prop,'Ok',{'MANUAL':'On'},start=start,timeout=12)
    select('AUTO_CONTINUOUS')
    # Save omits this controller-owned mode, so reconnect cannot restore stale client preference.
    start=len(events);send('newSwitchVector','CONFIG_PROCESS',{'CONFIG_SAVE':'On'})
    wait('CONFIG_PROCESS','Ok',start=start)
    with open(env['INDICONFIG']) as f:assert 'NATIVE_WEATHER_UPDATE' not in f.read()
    before=len(commands);start=len(events)
    send('newSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'});wait('CONNECTION','Idle',{'CONNECT':'Off'},start=start)
    send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'});wait('CONNECTION','Ok',{'CONNECT':'On'},start=start)
    assert state['weather_mode']==2 and not any(c.startswith(b':WSS') for c in commands[before:])
    refresh()
    # All mode/refraction operations above must leave the native parked state and motion untouched.
    motion=(b':Mg',b':Mn',b':Ms',b':Me',b':Mw',b':MS',b':PO',b':hP',b':AP',b':AL',b':Q',b':STOP')
    assert not any(c.startswith(motion) for c in commands[initial:]),commands[initial:]
    assert state['status']==5
    # A genuinely missing acknowledgement invalidates the selection; no write replay.
    state['drop_mode']=True;before,start=select('MANUAL','Alert')
    wait('NATIVE_STATE','Alert',start=start)
    assert commands[before:].count(b':WSS0#')==1
    last=next(v for _,a,v in reversed(events) if a.get('name')==prop)
    assert all(v=='Off' for v in last.values()),last
    assert not reader_errors and not server_errors,(reader_errors,server_errors)
    print('PASS: native mode readback, all choices, automatic ownership, parked writes, external updates, no config replay, rejection/mismatch and real fault handling')

def refraction_rejection_tests():
    send('setSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'},ENCLOSURE,'Ok')
    roof(opened='Idle',closed='Ok')
    wait('TELESCOPE_PARK','Ok',{'PARK':'On'})
    def write(values,expected='Ok'):
        before=len(commands);start=len(events)
        send('newNumberVector','NATIVE_REFRACTION',values)
        wait('NATIVE_REFRACTION',expected,start=start)
        return before,start
    def refresh():
        start=len(events);send('newSwitchVector','NATIVE_REFRESH_CONTROLS',{'REFRESH':'On'})
        wait('NATIVE_REFRESH_CONTROLS','Ok',start=start)
    refresh()  # Populate the same current values displayed in the user's session.
    initial=len(commands);first_event=len(events)
    # Ekos may send both members even when the user edited only temperature.
    before,_=write({'TEMPERATURE_C':9.5,'PRESSURE_HPA':980.2})
    assert b':SRTMP+009.5#' in commands[before:]
    assert not any(c.startswith(b':SRPRS') for c in commands[before:])
    before,_=write({'PRESSURE_HPA':982.5})
    assert b':SRPRS0982.5#' in commands[before:]
    assert not any(c.startswith(b':SRTMP') for c in commands[before:])
    before,_=write({'TEMPERATURE_C':-1})
    assert b':SRTMP-001.0#' in commands[before:]
    # A concurrent external pressure change is read back, never overwritten by a temperature edit.
    state['pressure']=981.5
    before,_=write({'TEMPERATURE_C':-2,'PRESSURE_HPA':982.5})
    assert not any(c.startswith(b':SRPRS') for c in commands[before:])
    assert state['pressure']==981.5
    write({'TEMPERATURE_C':-1,'PRESSURE_HPA':982.5})
    # Valid 0 replies must not destroy a healthy connection or parked state.
    state['reject_temperature']=True
    before,start=write({'TEMPERATURE_C':10,'PRESSURE_HPA':983.5},'Alert')
    assert not any(c.startswith(b':SRPRS') for c in commands[before:])
    assert state['temperature']==-1 and state['pressure']==982.5
    refresh()
    state['reject_pressure']=True
    before,start=write({'TEMPERATURE_C':9.1,'PRESSURE_HPA':983.5},'Alert')
    actual=next(v for tag,a,v in events[start:] if tag=='setNumberVector' and a.get('name')=='NATIVE_REFRACTION' and a.get('state')=='Alert')
    assert float(actual['TEMPERATURE_C'])==9.1 and float(actual['PRESSURE_HPA'])==982.5,actual
    assert commands[before:].count(b':SRPRS0983.5#')==1
    assert state['temperature']==9.1 and state['pressure']==982.5
    refresh()
    state['pressure_readback_offset']=1
    before,start=write({'PRESSURE_HPA':983.5},'Alert')
    assert commands[before:].count(b':SRPRS0983.5#')==1
    assert state['pressure']==984.5
    refresh()
    write({'TEMPERATURE_C':8.5,'PRESSURE_HPA':980.2})
    wait('NATIVE_STATE',value={'STATUS_CODE':'5'},start=first_event)
    assert state['status']==5
    for _,a,v in events[first_event:]:
        if a.get('name')=='TELESCOPE_PARK':assert v.get('UNPARK')!='On',(a,v)
        if a.get('name')=='NATIVE_STATUS_TEXT':assert a.get('state')!='Alert',(a,v)
    motion=(b':Mg',b':Mn',b':Ms',b':Me',b':Mw',b':MS',b':PO',b':hP',b':AP',b':AL',b':Q',b':STOP')
    assert not any(c.startswith(motion) for c in commands[initial:]),commands[initial:]
    assert not reader_errors and not server_errors,(reader_errors,server_errors)
    # An invalid acknowledgement is an actual framing fault: do not weaken fail-closed handling.
    state['bad_pressure_reply']=True;before=len(commands);start=len(events)
    send('newNumberVector','NATIVE_REFRACTION',{'PRESSURE_HPA':983.5})
    wait('NATIVE_STATE','Alert',start=start)
    assert commands[before:].count(b':SRPRS0983.5#')==1
    assert not any(c.startswith(motion) for c in commands[before:])
    print('PASS: strict native refraction formatting; field-specific writes; rejection/partial readback preserve connection and parked state without replay or motion')

def settings_guard_tests():
    # A real closed/locked enclosure: no ignored-policy shortcut and no movement.
    send('setSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'},ENCLOSURE,'Ok')
    start=len(events);roof(opened='Idle',closed='Ok')
    wait('NATIVE_ROOF_INTERLOCK','Alert',start=start)
    wait('TELESCOPE_PARK','Ok',{'PARK':'On'},start=start)
    start=len(events);send('newSwitchVector','NATIVE_REFRESH_CONTROLS',{'REFRESH':'On'})
    wait('NATIVE_REFRESH_CONTROLS','Ok',start=start)
    initial=len(commands)
    # The user's exact rejected operation now works, and the original value is restored.
    for value in (6.0,7.5):
        start=len(events);send('newNumberVector','NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':value})
        wait('NATIVE_GUIDE_RATE','Ok',start=start)
        start=len(events);send('newSwitchVector','NATIVE_REFRESH_CONTROLS',{'REFRESH':'On'})
        wait('NATIVE_REFRESH_CONTROLS','Ok',start=start)
        assert state['guide_rate']==value
        assert any(a.get('name')=='NATIVE_GUIDE_RATE' and a.get('state')=='Ok' and float(v['RATE_ARCSEC_S'])==value for _,a,v in events[start:])
    for values in ({'TEMPERATURE_C':9.1,'PRESSURE_HPA':982.5}, {'TEMPERATURE_C':8.5,'PRESSURE_HPA':980.2}):
        start=len(events);send('newNumberVector','NATIVE_REFRACTION',values)
        wait('NATIVE_REFRACTION','Ok',start=start)
        assert state['temperature']==values['TEMPERATURE_C'] and state['pressure']==values['PRESSURE_HPA']
    # Native readback for a partial input must preserve the untouched pressure.
    start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':9.1})
    wait('NATIVE_REFRACTION','Ok',start=start);assert state['pressure']==980.2
    start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':8.5})
    wait('NATIVE_REFRACTION','Ok',start=start)
    # Parked guiding still fails; changing settings must not authorize motion.
    start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':100})
    wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start)
    motion=(b':Mg',b':Mn',b':Ms',b':Me',b':Mw',b':MS',b':PO',b':hP',b':AP',b':AL',b':Q',b':STOP')
    assert not any(c.startswith(motion) for c in commands[initial:]),commands[initial:]
    assert state['status']==5 and state['flag']==0
    def reject_settings(include_refraction=True):
        before=len(commands)
        writes=[('NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':6})]
        if include_refraction:writes.append(('NATIVE_REFRACTION',{'TEMPERATURE_C':9.1,'PRESSURE_HPA':982.5}))
        for name,values in writes:
            start=len(events);send('newNumberVector',name,values);wait(name,'Alert',start=start)
        assert not any(c.startswith((b':Rg',b':SRTMP',b':SRPRS')) for c in commands[before:]),commands[before:]
    # Keep the enclosure permitted for native motion-state conflict tests, avoiding a safety STOP.
    configure_roof();time.sleep(.15)
    for status,flag in [(6,1),(0,1),(9,0),(99,0)]:
        state.update(status=status,flag=flag);reject_settings()
    state.update(status=0,flag=0);start=len(events);wait('NATIVE_STATE',value={'STATUS_CODE':'0'},start=start)
    start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':800})
    wait('TELESCOPE_TIMED_GUIDE_NS','Busy',start=start);reject_settings(False)
    # Working 10Micron driver allows refraction refreshes during guiding.
    ref_start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':9.1,'PRESSURE_HPA':982.5})
    wait('NATIVE_REFRACTION','Ok',start=ref_start)
    ref_start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':8.5,'PRESSURE_HPA':980.2})
    wait('NATIVE_REFRACTION','Ok',start=ref_start)
    wait('TELESCOPE_TIMED_GUIDE_NS','Idle',start=start)
    start=len(events);send('newSwitchVector','TELESCOPE_MOTION_NS',{'MOTION_NORTH':'On'})
    wait('TELESCOPE_MOTION_NS','Busy',start=start);wire(b':Mn#');reject_settings()
    start=len(events);send('newSwitchVector','TELESCOPE_MOTION_NS',{'MOTION_NORTH':'Off'})
    wait('TELESCOPE_MOTION_NS','Idle',start=start);wire(b':Qn#')
    # Communication failure must not permit either setting writer.
    state['drop_poll']=True;start=len(events);wait('NATIVE_STATE','Alert',start=start,timeout=6)
    reject_settings()
    assert not reader_errors and not server_errors,(reader_errors,server_errors)
    print('PASS: parked closed-roof settings read/write/restore without motion; pulses, native faults, active pulse/manual motion and communication failure remain blocked')

try:
    p.stdin.write('<getProperties version="1.7"/>\n'.replace('\\n','\n'));p.stdin.flush();time.sleep(.1)
    send('newTextVector','NATIVE_ENDPOINT',{'HOST':'127.0.0.1','PORT':port})
    if '--serial' in sys.argv:
        import serial_bridge
        serial_slave=serial_bridge.configure(send,port)
    send('newNumberVector','POLLING_PERIOD',{'PERIOD_MS':100})
    if '--enclosure' in sys.argv or '--settings-guard' in sys.argv or '--refraction' in sys.argv or '--weather-mode' in sys.argv or '--tracking' in sys.argv or '--reconcile-controls' in sys.argv or '--startup' in sys.argv:
        send('newTextVector','ACTIVE_DEVICES',{'ACTIVE_DOME':ENCLOSURE})
        send('newSwitchVector','DOME_POLICY',{'DOME_LOCKS':'On','DOME_IGNORED':'Off'})
    send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'})
    wait('CONNECTION','Ok')
    if '--serial' in sys.argv:
        serial_bridge.check(serial_slave)
        start=len(events);send('newSwitchVector','NATIVE_TRANSPORT',{'TCP':'On','SERIAL':'Off'})
        wait('NATIVE_TRANSPORT','Alert',start=start)
        start=len(events);send('newTextVector','NATIVE_SERIAL',{'DEVICE':'/invalid','BAUD':'115200'})
        wait('NATIVE_SERIAL','Alert',start=start)

    if '--weather-mode' in sys.argv:
        weather_mode_tests()
    elif '--refraction' in sys.argv:
        refraction_rejection_tests()
    elif '--settings-guard' in sys.argv:
        settings_guard_tests()
    elif '--startup' in sys.argv:
        startup_reconciliation_tests()
    elif '--reconcile-controls' in sys.argv:
        control_alert_reconciliation_tests()
    elif '--tracking' in sys.argv:
        tracking_transition_tests()
    elif '--calibration' in sys.argv:
        calibration_pulse_tests()
    else:
        if '--enclosure' in sys.argv:
            configure_roof();time.sleep(.1)
        before=len(commands);start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':100})
        wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start);assert b':Mgn0100#' not in commands[before:]

        assert any(a.get('name')=='TELESCOPE_TIMED_GUIDE_NS' for _,a,_ in events)
        start=len(events);send('newSwitchVector','NATIVE_REFRESH_CONTROLS',{'REFRESH':'On'})
        wait('NATIVE_REFRESH_CONTROLS','Ok',start=start)
        assert state['guide_rate']==7.5
        start=len(events);send('newTextVector','TIME_UTC',{'UTC':'2026-10-25T03:04:05','OFFSET':'-5'})
        wait('TIME_UTC','Ok',start=start)
        assert b':SUDT2026-10-25,03:04:05#' in commands and b':SG+05:00:00#' in commands
        start=len(events);send('newNumberVector','GEOGRAPHIC_COORD',{'LAT':44.9,'LONG':267.1,'ELEV':300})
        wait('GEOGRAPHIC_COORD','Ok',start=start)
        assert b':Sg+092*54:00.0#' in commands
        state.update(status=7,flag=0);start=len(events);wait('NATIVE_STATE',value={'STATUS_CODE':'7'},start=start)
        start=len(events);send('newSwitchVector','TELESCOPE_TRACK_MODE',{'TRACK_SOLAR':'On'});wait('TELESCOPE_TRACK_MODE','Ok',start=start);wire(b':RT1#')
        assert state['status']==7
        start=len(events);send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On','TRACK_OFF':'Off'});wait('TELESCOPE_TRACK_STATE','Ok',start=start)
        wire(b':AP#')
        state.update(status=0,flag=0);start=len(events);wait('NATIVE_STATE',value={'STATUS_CODE':'0'},start=start)
        # Time and site changes must not run during tracking.
        before=len(commands);start=len(events);send('newTextVector','TIME_UTC',{'UTC':'2026-10-25T03:04:05','OFFSET':'-5'})
        wait('TIME_UTC','Alert',start=start);assert not any(c.startswith(b':SUDT') for c in commands[before:])
        start=len(events);send('newNumberVector','NATIVE_GUIDE_RATE',{'RATE_ARCSEC_S':7.0});wait('NATIVE_GUIDE_RATE','Ok',start=start)
        assert b':Rg7.0000#' in commands and state['guide_rate']==7
        start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':-5.2,'PRESSURE_HPA':975.5});wait('NATIVE_REFRACTION','Ok',start=start)
        assert state['temperature']==-5.2 and state['pressure']==975.5
        # Standard track modes and native custom-rate conversion.
        for name,cmd in [('TRACK_SOLAR',b':RT1#'),('TRACK_LUNAR',b':RT0#'),('TRACK_SIDEREAL',b':RT2#')]:
            start=len(events);send('newSwitchVector','TELESCOPE_TRACK_MODE',{name:'On'});wait('TELESCOPE_TRACK_MODE','Ok',start=start);wire(cmd)
        start=len(events);send('newSwitchVector','TELESCOPE_TRACK_MODE',{'TRACK_CUSTOM':'On'});wait('TELESCOPE_TRACK_MODE','Ok',start=start)
        start=len(events);send('newNumberVector','TELESCOPE_TRACK_RATE',{'TRACK_RATE_RA':15.041067,'TRACK_RATE_DE':1.5041067});wait('TELESCOPE_TRACK_RATE','Ok',start=start)
        assert b':RR+0.000000#' in commands and b':RD+0.100000#' in commands
        # All four pulse directions and a genuine elapsed-duration Busy -> Idle lifecycle.
        for prop,member,cmd in [('NS','N',b':Mgn0400#'),('NS','S',b':Mgs0400#'),('WE','W',b':Mgw0400#'),('WE','E',b':Mge0400#')]:
            start=len(events);t=time.monotonic();send('newNumberVector','TELESCOPE_TIMED_GUIDE_'+prop,{'TIMED_GUIDE_'+member:400})
            wait('TELESCOPE_TIMED_GUIDE_'+prop,'Busy',start=start)
            wait('TELESCOPE_TIMED_GUIDE_'+prop,'Idle',start=start);assert time.monotonic()-t>=.39 and cmd in commands
        # Invalid durations and conflicting directions must never reach the wire.
        for values in ({'TIMED_GUIDE_N':10000},{'TIMED_GUIDE_N':-.1},{'TIMED_GUIDE_N':1.5},{'TIMED_GUIDE_N':10,'TIMED_GUIDE_S':10}):
            start=len(events);before=len(commands);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',values);wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start)
            assert not any(c.startswith(b':Mg') for c in commands[before:])
        # Both axes may guide concurrently; same-axis replacement and Park are refused.
        start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':800});wait('TELESCOPE_TIMED_GUIDE_NS','Busy',start=start)
        start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_WE',{'TIMED_GUIDE_E':800});wait('TELESCOPE_TIMED_GUIDE_WE','Busy',start=start)
        before=len(commands);start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_S':200});wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start)
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'});wait('TELESCOPE_PARK','Alert',start=start)
        assert b':hP#' not in commands[before:] and b':Mgs0200#' not in commands[before:]
        wait('TELESCOPE_TIMED_GUIDE_NS','Idle',start=start);wait('TELESCOPE_TIMED_GUIDE_WE','Idle',start=start)
        # Manual speed and independent stop command, followed by ordinary pulse availability.
        start=len(events);send('newSwitchVector','TELESCOPE_SLEW_RATE',{'1x':'On'})
        # INDI standard switch names are indices 1x..4x.
        wait('TELESCOPE_SLEW_RATE','Ok',start=start)
        for prop,member,cmd,halt in [('NS','NORTH',b':Mn#',b':Qn#'),('NS','SOUTH',b':Ms#',b':Qs#'),('WE','WEST',b':Mw#',b':Qw#'),('WE','EAST',b':Me#',b':Qe#')]:
            start=len(events);send('newSwitchVector','TELESCOPE_MOTION_'+prop,{'MOTION_'+member:'On'});wait('TELESCOPE_MOTION_'+prop,'Busy',start=start);wire(cmd)
            before=len(commands);start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':100});wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start);assert b':Mgn0100#' not in commands[before:]
            start=len(events);send('newSwitchVector','TELESCOPE_MOTION_'+prop,{'MOTION_'+member:'Off'});wait('TELESCOPE_MOTION_'+prop,'Idle',start=start);wire(halt)
        # Abort cancels the pulse bookkeeping without a later false completion.
        start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':1500});wait('TELESCOPE_TIMED_GUIDE_NS','Busy',start=start)
        start=len(events);send('newSwitchVector','TELESCOPE_ABORT_MOTION',{'ABORT':'On'});wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start)
        time.sleep(1.7);assert not any(a.get('name')=='TELESCOPE_TIMED_GUIDE_NS' and a.get('state')=='Idle' for _,a,_ in events[start:])
        if '--enclosure' in sys.argv:
            start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':9999});wait('TELESCOPE_TIMED_GUIDE_NS','Busy',start=start);wire(b':Mgn9999#')
            start=len(events);send('setSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'},ENCLOSURE,'Ok')
            wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start);wire(b':STOP#')
            configure_roof();time.sleep(.5)
            before=len(commands);start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_WE',{'TIMED_GUIDE_E':100});wait('TELESCOPE_TIMED_GUIDE_WE','Alert',start=start)
            assert b':Mge0100#' not in commands[before:]
            print('PASS: enclosure loss cancels guide lifecycle, STOPs once and retains motion hold')
        # A valid settings rejection remains local; real transport loss is tested separately.
        if '--enclosure' not in sys.argv:
            state['reject_pressure']=True;start=len(events);send('newNumberVector','NATIVE_REFRACTION',{'TEMPERATURE_C':-6,'PRESSURE_HPA':976});wait('NATIVE_REFRACTION','Alert',start=start)
            if '--recovery' in sys.argv:
                state['drop_poll']=True;start=len(events);wait('NATIVE_STATE','Alert',start=start,timeout=6)
                start=len(events);send('newSwitchVector','NATIVE_RECOVERY',{'TELEMETRY':'On','MANUAL':'Off'})
                wait('NATIVE_RECOVERY','Ok',start=start)
                # Fault has already closed transport; recovery must not replay SRPRS or movement.
                before=len(commands);wait('NATIVE_COMMAND_STATUS','Alert',{'COMMAND':'Telemetry reconnected; motion held. No command replay or automatic capture recovery.'},start=start,timeout=15)
                start=len(events);send('newNumberVector','TELESCOPE_TIMED_GUIDE_NS',{'TIMED_GUIDE_N':100});wait('TELESCOPE_TIMED_GUIDE_NS','Alert',start=start)
                assert not any(c.startswith((b':Mg',b':MS',b':SRTMP',b':SRPRS')) for c in commands[before:])
        print('PASS: real INDI controls, guide lifecycle, rejection, manual motion, custom rates, time/site, readback and recovery' if '--recovery' in sys.argv else 'PASS: real INDI controls, guide lifecycle, rejection, manual motion, custom rates, time/site and readback')

finally:
    p.terminate()
    try:p.wait(3)
    except subprocess.TimeoutExpired:p.kill();p.wait()
    server_done.set();thread.join(3);config_dir.cleanup()
if reader_errors:raise AssertionError(reader_errors)
if server_errors and not all(isinstance(e,(ConnectionResetError,BrokenPipeError)) for e in server_errors):raise AssertionError(server_errors)
