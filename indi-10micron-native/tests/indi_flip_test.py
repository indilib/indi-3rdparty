"""Exercise the real INDI executable against a loopback mount and standard snoop XML.
No indiserver, physical roof or physical mount is used.
"""
import os, socket, subprocess, sys, tempfile, threading, time, xml.etree.ElementTree as ET
DEVICE='10Micron Native Motion'
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
port=listener.getsockname()[1];commands=[];server_errors=[]
state={'pier':'W','rejectFlip':False,'status':5,'ra':3.0,'dec':42.0,'flag':0,'count':0,'next':None,'targetRA':3.0,'targetDec':42.0}
lock=threading.Lock()
def serve():
    try:
        listener.settimeout(10)
        with listener.accept()[0] as c:
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
                    elif cmd==b':GVN#':reply=b'3.4#'
                    elif cmd==b':WSG#':reply=b'0#'
                    elif cmd==b':GVZ#':reply=b'Q-TYPE2016#'
                    elif cmd==b':GG#':reply=b'+05:00:00.0#'
                    elif cmd in (b':Ggui#',b':GRTMP#',b':GRPRS#',b':Gt#',b':Gg#',b':Gev#'):
                        reply={b':Ggui#':b'7.5#',b':GRTMP#':b'+8.5#',b':GRPRS#':b'980.2#',b':Gt#':b'+044:54:00.0#',b':Gg#':b'+092:54:00.0#',b':Gev#':b'+300.0#'}[cmd]
                    elif cmd==b':Ginfo#':
                        if state['count']:
                            state['count']-=1
                            if state['count']==0:
                                state['status']=state['next'];state['flag']=0
                                if state['next']==0:state['ra']=state['targetRA'];state['dec']=state['targetDec']
                        reply=f"{state['ra']},{state['dec']},{state['pier']},1,45,2461318,{state['status']},{state['flag']}#".encode()
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
                        state.update(status=6,flag=1,count=3,next=0);reply=b'0'
                    elif cmd==b':FLIP#':
                        if state.get('dropFlip'):return
                        reply=b'0' if state['rejectFlip'] else b'1'
                    elif cmd==b':PO#':state.update(status=3,flag=1,count=3,next=7)
                    elif cmd==b':hP#':state.update(status=2,flag=1,count=3,next=5)
                    elif cmd==b':AP#':state.update(status=0,flag=0,count=0)
                    elif cmd==b':AL#':state.update(status=7,flag=0,count=0)
                    elif cmd==b':Q#':state.update(status=6,flag=1,count=3,next=0)
                    elif cmd==b':STOP#':state.update(status=6,flag=1,count=3,next=5 if state['status'] in (2,3,5) else 1)
                    else:raise AssertionError(cmd)
                    if reply:c.sendall(reply)
    except Exception as e:server_errors.append(e)
    finally:listener.close()
thread=threading.Thread(target=serve,daemon=True);thread.start()
config_dir=tempfile.TemporaryDirectory(prefix='10u-indi-test-');env=os.environ.copy();env['INDICONFIG']=os.path.join(config_dir.name,'motion_config.xml')
# INDI park data uses the child's home separately from INDICONFIG.
env['HOME']=config_dir.name
os.makedirs(os.path.join(config_dir.name,'.indi'),exist_ok=True)
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
    child={'newTextVector':'oneText','setTextVector':'oneText','newSwitchVector':'oneSwitch','setSwitchVector':'oneSwitch','newNumberVector':'oneNumber','setLightVector':'oneLight'}[tag]
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
try:
    p.stdin.write('<getProperties version="1.7"/>\n');p.stdin.flush();time.sleep(.1)
    send('newTextVector','NATIVE_ENDPOINT',{'HOST':'127.0.0.1','PORT':port})
    if '--serial' in sys.argv:
        import serial_bridge
        serial_slave=serial_bridge.configure(send,port)
    send('newNumberVector','POLLING_PERIOD',{'PERIOD_MS':100})
    if '--portable' not in sys.argv:
        send('newTextVector','ACTIVE_DEVICES',{'ACTIVE_DOME':ENCLOSURE})
        send('newSwitchVector','DOME_POLICY',{'DOME_LOCKS':'On','DOME_IGNORED':'Off'})
    send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'});wait('CONNECTION','Ok',{'CONNECT':'On'})
    if '--serial' in sys.argv:
        serial_bridge.check(serial_slave)
        start=len(events);send('newSwitchVector','NATIVE_TRANSPORT',{'TCP':'On','SERIAL':'Off'})
        wait('NATIVE_TRANSPORT','Alert',start=start)
        start=len(events);send('newTextVector','NATIVE_SERIAL',{'DEVICE':'/invalid','BAUD':'115200'})
        wait('NATIVE_SERIAL','Alert',start=start)

    wait('NATIVE_SCHEDULING_DATA','Ok',{'VALID':'1'},timeout=8)
    schedule=[e for e in events if e[1].get('name')=='NATIVE_SCHEDULING_DATA' and e[1].get('state')=='Ok'][-1]
    assert float(schedule[2]['TRACK_END_ESTIMATE_SEC'])==711*60
    assert 0<=float(schedule[2]['SAMPLE_AGE_SEC'])<15
    configure_roof()
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'Off','UNPARK':'On'})
    busy=wait('TELESCOPE_PARK','Busy',start=start)
    wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
    start=len(events);send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On','TRACK_OFF':'Off'})
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start)
    def flip(ra=3,dec=42):
        start=len(events)
        send('newSwitchVector','ON_COORD_SET',{'FLIP':'On','TRACK':'Off','SYNC':'Off','SLEW':'Off'})
        send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':ra,'DEC':dec})
        return start
    # Native FLIP cannot silently ignore a different INDI destination.
    start=flip(4,42);wait('EQUATORIAL_EOD_COORD','Alert',start=start);assert b':FLIP#' not in commands
    start=flip();wait('EQUATORIAL_EOD_COORD','Busy',start=start)
    wait('NATIVE_MOTION_STATUS','Busy',{'COMMAND':'Flip accepted; awaiting opposite native pier side, tracking and settled target'},start)
    time.sleep(.35)
    assert not any(e[1].get('name')=='EQUATORIAL_EOD_COORD' and e[1].get('state')=='Ok' for e in events[start:])
    # Opposite side while settling still cannot complete the operation.
    state.update(pier='E',flag=1);time.sleep(.25)
    assert not any(e[1].get('name')=='NATIVE_MOTION_STATUS' and 'Native flip complete' in e[2].get('COMMAND','') for e in events[start:])
    state['flag']=0
    complete='Native flip complete: opposite pier side, tracking and settled target confirmed'
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':complete},start);wait('EQUATORIAL_EOD_COORD','Ok',start=start)
    assert commands.count(b':FLIP#')==1
    if '--timeout' in sys.argv:
        # Exercise the production 180-second deadline, with fresh roof snoops.
        start=flip();wait('EQUATORIAL_EOD_COORD','Busy',start=start)
        wait('NATIVE_MOTION_STATUS','Busy',{'COMMAND':'Flip accepted; awaiting opposite native pier side, tracking and settled target'},start)
        state.update(status=6,flag=1,count=0)
        deadline=time.monotonic()+190
        while b':STOP#' not in commands and time.monotonic()<deadline:
            configure_roof();time.sleep(.2)
        assert commands.count(b':STOP#')==1, (commands[-20:], events[-25:], reader_errors, server_errors, ''.join(stderr[-10:]))
        confirmed='Flip failed or timed out; required opposite-side completion not confirmed; Native telemetry confirms STOP completed; inspect native state'
        wait('NATIVE_MOTION_STATUS','Alert',{'COMMAND':confirmed},start,timeout=8)
        wait('NATIVE_STATUS_TEXT',value={'DESCRIPTION':'stopped'},start=start)
        time.sleep(.5)
        wait('EQUATORIAL_EOD_COORD','Alert',start=len(events))
        assert commands.count(b':FLIP#')==2 and commands.count(b':STOP#')==1
        assert not any(e[1].get('name')=='NATIVE_MOTION_STATUS' and e[2].get('COMMAND')==complete for e in events[start:])
        count=commands.count(b':FLIP#');blocked=flip()
        wait('EQUATORIAL_EOD_COORD','Alert',start=blocked)
        assert commands.count(b':FLIP#')==count
        assert not server_errors and not reader_errors,(server_errors,reader_errors)
        print('INDI flip timeout: one STOP, confirmed stopped, held, retained alert and no retry passed')
        sys.exit(0)
    state['rejectFlip']=True;start=flip();wait('NATIVE_FLIP_RESULT','Ok',{'CODE':'2'},start)
    # Command rejection must not turn a healthy roof's independent indicator red.
    wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Mount rejected flip; unchanged settled tracking confirmed'},start)
    wait('NATIVE_ROOF_INTERLOCK','Ok',start=start)
    # Confirmed rejection must leave standard mount coordinates healthy.
    wait('EQUATORIAL_EOD_COORD','Ok',start=start)
    time.sleep(.6)
    polled=len(events)
    wait('NATIVE_STATE','Ok',{'TRACKING':'1','SLEW_OR_SETTLING':'0'},start=polled)
    coords=[e for e in events[start:] if e[0].startswith('set') and
            e[1].get('device')==DEVICE and e[1].get('name')=='EQUATORIAL_EOD_COORD']
    assert coords and coords[-1][1].get('state')=='Ok',coords
    assert not any(e[1].get('state')=='Alert' for e in coords),coords
    state['rejectFlip']=False
    start=flip();wait('EQUATORIAL_EOD_COORD','Busy',start=start)
    send('newSwitchVector','TELESCOPE_ABORT_MOTION',{'ABORT':'On'})
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native slew halt confirmed; tracking may remain on'},start)
    assert commands.count(b':Q#')==1
    assert not any(e[1].get('name')=='NATIVE_MOTION_STATUS' and e[2].get('COMMAND')==complete for e in events[start:])
    start=flip();wait('EQUATORIAL_EOD_COORD','Busy',start=start)
    send('newSwitchVector','NATIVE_STOP_ALL',{'STOP':'On'})
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms STOP completed; inspect native state'},start)
    count=commands.count(b':FLIP#');start=flip();wait('EQUATORIAL_EOD_COORD','Alert',start=start)
    assert commands.count(b':FLIP#')==count
    configure_roof();start=len(events)
    send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'PARK':'On'},busy+1)
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
    start=len(events)
    send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On','TRACK_OFF':'Off'})
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start)
    start=flip();wait('EQUATORIAL_EOD_COORD','Busy',start=start);roof('Ok','Ok')
    wait('NATIVE_STOP_ALL','Alert',start=start)
    assert commands.count(b':STOP#')==2
    assert not any(e[1].get('name')=='NATIVE_MOTION_STATUS' and e[2].get('COMMAND')==complete for e in events[start:])
    # Lost acknowledgement never retries or claims flip completion.
    configure_roof();start=len(events)
    send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'PARK':'On'},busy+1)
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
    start=len(events)
    send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On','TRACK_OFF':'Off'})
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start)
    count=commands.count(b':FLIP#');state['dropFlip']=True;start=flip()
    wait('NATIVE_STATUS_TEXT','Alert',start=start)
    wait('NATIVE_SCHEDULING_DATA','Alert',{'VALID':'0','TRACK_END_ESTIMATE_SEC':'-1'},start)
    assert commands.count(b':FLIP#')==count+1
    assert not any(e[1].get('name')=='NATIVE_MOTION_STATUS' and e[2].get('COMMAND')==complete for e in events[start:])
    assert not server_errors and not reader_errors,(server_errors,reader_errors)
    print('INDI flip: same-side false completion, settle guard, rejection, Abort, STOP, enclosure fault and recovery hold passed')
finally:
    p.terminate()
    try:p.wait(timeout=3)
    except subprocess.TimeoutExpired:p.kill();p.wait()
    config_dir.cleanup()
