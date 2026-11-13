"""Exercise the real INDI executable against a loopback mount and standard snoop XML.
No indiserver, physical roof or physical mount is used.
"""
import os, socket, subprocess, sys, tempfile, threading, time, xml.etree.ElementTree as ET
DEVICE='10Micron Native Motion'
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
port=listener.getsockname()[1];commands=[];server_errors=[];server_done=threading.Event()
state={'status':5,'ra':3.0,'dec':42.0,'flag':0,'count':0,'next':None,'targetRA':3.0,'targetDec':42.0}
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
                            reply=f"{state['ra']},{state['dec']},W,1,45,2461318,{state['status']},{state['flag']}#".encode()
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
                        elif cmd==b':AP#':state.update(status=0,flag=0,count=0)
                        elif cmd==b':AL#':state.update(status=7,flag=0,count=0)
                        elif cmd==b':Q#':state.update(status=0,flag=0,count=0)
                        elif cmd==b':STOP#':state.update(status=2 if state['status']==2 else 6,flag=1,count=3,next=5 if state['status'] in (2,3,5) else 1)
                        else:raise AssertionError(cmd)
                        if reply:c.sendall(reply)
    except Exception as e:server_errors.append(e)
    finally:listener.close()
thread=threading.Thread(target=serve,daemon=True);thread.start()
config_dir=tempfile.TemporaryDirectory(prefix='10u-indi-test-');env=os.environ.copy();env['INDICONFIG']=os.path.join(config_dir.name,'motion_config.xml')
# INDI park data uses the child's home separately from INDICONFIG.
env['HOME']=config_dir.name
os.makedirs(os.path.join(config_dir.name,'.indi'),exist_ok=True)
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
    send('newNumberVector','POLLING_PERIOD',{'PERIOD_MS':100})
    if '--portable' not in sys.argv:
        send('newTextVector','ACTIVE_DEVICES',{'ACTIVE_DOME':ENCLOSURE})
        send('newSwitchVector','DOME_POLICY',{'DOME_LOCKS':'Off' if '--ignored' in sys.argv else 'On','DOME_IGNORED':'On' if '--ignored' in sys.argv else 'Off'})
    send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'})
    wait('CONNECTION','Ok',{'CONNECT':'On'})
    wait('TELESCOPE_PARK','Ok',{'PARK':'On'})
    assert not any(a.get('name')=='NATIVE_MOTION_ARM' for _,a,_ in events), 'Custom arming property must be absent'
    if '--portable' in sys.argv or '--ignored' in sys.argv:
        assert any(a.get('name')=='DOME_POLICY' and v.get('DOME_IGNORED')=='On' for _,a,v in events)
        expected_name=ENCLOSURE if '--ignored' in sys.argv else ''
        assert any(a.get('name')=='ACTIVE_DEVICES' and v.get('ACTIVE_DOME')==expected_name for _,a,v in events)
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
        busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
        start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43})
        busy=wait('EQUATORIAL_EOD_COORD','Busy',start=start);wait('EQUATORIAL_EOD_COORD','Ok',start=busy+1)
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
        busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'PARK':'On'},busy+1)
        # Retry after confirmed completion is idempotent; it sends no new PO.
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
        busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
        before=commands.count(b':PO#');start=len(events)
        send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
        wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native telemetry confirms already unparked and settled'},start)
        assert commands.count(b':PO#')==before
        assert commands.count(b':PO#')==2 and commands.count(b':hP#')==1 and commands.count(b':MS#')==1
        print('Explicit ignored enclosure' if '--ignored' in sys.argv else 'Outdoor defaults', ': ordinary unpark, idempotent retry, slew and native park completion without any arm action or script: passed')
        raise SystemExit(0)
    if '--enclosure' in sys.argv:
        def blocked():
            before=len(commands);start=len(events)
            send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
            wait('NATIVE_COMMAND_STATUS','Alert',start=start)
            assert not any(c==b':PO#' for c in commands[before:])
        blocked()  # selected device has not reported anything
        send('setSwitchVector','DOME_PARK',{'UNPARK':'On','PARK':'Off'},'Other Enclosure','Ok');blocked()
        roof();blocked()  # park before confirmed connection cannot unlock
        send('setSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'},ENCLOSURE,'Ok');blocked()
        roof(vector='Busy');blocked()
        roof(vector='Alert');blocked()
        send('setSwitchVector','DOME_PARK',{'PARK':'On','UNPARK':'On'},ENCLOSURE,'Ok');blocked()
        send('setSwitchVector','DOME_PARK',{'PARK':'Off','UNPARK':'Off'},ENCLOSURE,'Ok');blocked()
        configure_roof()
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
        busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
        # Device and policy cannot be changed while connected, bypassing the lock.
        start=len(events);send('newTextVector','ACTIVE_DEVICES',{'ACTIVE_DOME':''})
        wait('ACTIVE_DEVICES','Alert',{'ACTIVE_DOME':ENCLOSURE},start)
        start=len(events);send('newSwitchVector','DOME_POLICY',{'DOME_IGNORED':'On','DOME_LOCKS':'Off'})
        wait('DOME_POLICY','Alert',{'DOME_LOCKS':'On'},start)
        start=len(events);send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On','TRACK_OFF':'Off'})
        wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start)
        before=commands.count(b':STOP#');start=len(events)
        send('setSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'},ENCLOSURE,'Idle')
        wait('NATIVE_STOP_ALL','Alert',start=start)
        time.sleep(.3);assert commands.count(b':STOP#')==before+1
        blocked()
        configure_roof()
        # Returning dome telemetry cannot silently restart a stopped operation.
        start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43})
        wait('EQUATORIAL_EOD_COORD','Alert',start=start)
        # Deliberate recovery park remains possible once enclosure is open.
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
        busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'PARK':'On'},busy+1)
        # An advertised shutter must independently report Open/Ok.
        send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'Off','SHUTTER_CLOSE':'On'},ENCLOSURE,'Ok');blocked()
        send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Busy');blocked()
        send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
        start=len(events);wait('NATIVE_ROOF_INTERLOCK','Ok',start=start-1)
        p.stdin.write('<delProperty device="'+ENCLOSURE+'" name="DOME_SHUTTER"/>\n');p.stdin.flush();blocked()
        send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
        # Reconnecting the mount invalidates all previously observed enclosure state.
        start=len(events);send('newSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'})
        wait('CONNECTION','Idle',{'DISCONNECT':'On'},start)
        start=len(events);send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'})
        wait('CONNECTION','Ok',{'CONNECT':'On'},start);blocked()
        configure_roof();blocked() # a previously advertised shutter also needs re-observation
        send('setSwitchVector','DOME_SHUTTER',{'SHUTTER_OPEN':'On','SHUTTER_CLOSE':'Off'},ENCLOSURE,'Ok')
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
        busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
        # A disappeared whole enclosure blocks despite the remembered name.
        p.stdin.write('<delProperty device="'+ENCLOSURE+'"/>\n');p.stdin.flush();blocked()
        print('Standard arbitrary-name enclosure: missing/busy/alert/contradictory/foreign state, active edits, disconnect STOP, recovery, shutter loss, deletion and reconnect guards: passed')
        raise SystemExit(0)
    if '--reconcile' in sys.argv:
        # Legacy overly broad latch must not override fresh native truth.
        wait('TELESCOPE_PARK','Ok',{'PARK':'On'})
        assert not any(v.get('INTERRUPTED')=='1' for _,a,v in events if a.get('name')=='NATIVE_PARK_SAFETY')
        before=len(commands);start=len(events)
        send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
        wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native telemetry confirms already parked'},start)
        assert not any(c in (b':hP#',b':PO#',b':STOP#') for c in commands[before:])
        start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
        wait('NATIVE_COMMAND_STATUS','Alert',start=start)  # configured enclosure still unavailable
        # MW4 changes state while INDI is disconnected; reconnect must poll it.
        send('newSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'})
        wait('CONNECTION','Idle',{'DISCONNECT':'On'},start)
        state.update(status=7,flag=0,count=0)
        start=len(events);send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'})
        wait('CONNECTION','Ok',{'CONNECT':'On'},start)
        wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},start)
        wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native connection polled; motion admission awaits enclosure policy'},start)
        # External park/unpark are observations, with no driver motion writes.
        before=len(commands);start=len(events);state.update(status=5)
        wait('TELESCOPE_PARK','Ok',{'PARK':'On'},start)
        start=len(events);state.update(status=7)
        wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},start)
        assert not any(c in (b':hP#',b':PO#',b':STOP#') for c in commands[before:])
        # An external completed park during our slew cancels only the operation.
        configure_roof();state['slew_count']=100
        start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43})
        wait('EQUATORIAL_EOD_COORD','Busy',start=start)
        # Wait for acknowledgement, then replace mock native operation.
        end=time.monotonic()+3
        while b':MS#' not in commands and time.monotonic()<end:time.sleep(.01)
        time.sleep(.15);before=commands.count(b':STOP#');start=len(events)
        state.update(status=5,flag=0,count=0)
        wait('TELESCOPE_PARK','Ok',{'PARK':'On'},start)
        wait('NATIVE_COMMAND_STATUS','Alert',{'COMMAND':'Active operation superseded by native parked state; inspect native state'},start)
        assert commands.count(b':STOP#')==before
        # A real poll timeout publishes unknown, then deliberate reconnect
        # reads MW4's new state and clears the old command failure.
        start=len(events);state['drop_poll']=True
        wait('NATIVE_STATE','Alert',{'PARKED':'-1','TRACKING':'-1'},start,timeout=5)
        wait('TELESCOPE_PARK','Alert',{'PARK':'Off','UNPARK':'Off'},start)
        fault=wait('NATIVE_COMMAND_STATUS','Alert',start=start)
        assert 'request=:Ginfo# phase=read' in events[fault][2]['COMMAND']
        send('newSwitchVector','CONNECTION',{'CONNECT':'Off','DISCONNECT':'On'})
        wait('CONNECTION','Idle',{'DISCONNECT':'On'},start)
        state.update(status=7,flag=0,count=0)
        start=len(events);send('newSwitchVector','CONNECTION',{'CONNECT':'On','DISCONNECT':'Off'})
        wait('CONNECTION','Ok',{'CONNECT':'On'},start)
        wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},start)
        wait('NATIVE_COMMAND_STATUS','Ok',{'COMMAND':'Native connection polled; motion admission awaits enclosure policy'},start)
        print('Legacy latch migration, reconnect cleanup, external state/operation reconciliation, poll timeout and recovery: passed')
        raise SystemExit(0)
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'UNPARK':'On','PARK':'Off'})
    wait('NATIVE_COMMAND_STATUS','Alert',start=start)
    assert b':PO#' not in commands
    configure_roof()
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'Off','UNPARK':'On'})
    wait('TELESCOPE_PARK','Busy',start=start)
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
    assert commands.count(b':PO#')==1
    if '--limit-unpark' in sys.argv:
        assert commands.count(b':AL#')==1, 'Unpark must disable outside-limits tracking once'
        wait('NATIVE_STATE','Busy',{'STATUS_CODE':'9'},start)
        wait('NATIVE_STATE','Ok',{'STATUS_CODE':'7'},start)
    # A normal unpark must not persist an interrupted-park warning.
    assert not any(v.get('INTERRUPTED')=='1' for _,a,v in events if a.get('name')=='NATIVE_PARK_SAFETY')
    # MW4 is emulated by changing controller state without any driver command.
    start=len(events);before=len(commands);state.update(status=5,flag=0,count=0)
    wait('TELESCOPE_PARK','Ok',{'PARK':'On'},start)
    assert not any(c in (b':hP#',b':PO#',b':STOP#') for c in commands[before:])
    start=len(events);state.update(status=7,flag=0,count=0)
    wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},start)
    # Ekos Slew to Target sends SYNC followed by TRACK coordinates.
    start=len(events);send('newSwitchVector','ON_COORD_SET',{'SYNC':'On','TRACK':'Off','SLEW':'Off'})
    send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':3.1,'DEC':42.1})
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native offset sync acknowledged; model refinement disabled'},start)
    wait('EQUATORIAL_EOD_COORD','Ok',start=start)
    assert commands.count(b':CM#')==1 and commands.count(b':CMCFG0#')==1
    assert not any(c in (b':CMS#',b':CMCFG1#',b':delalig#') for c in commands)
    send('newSwitchVector','ON_COORD_SET',{'SYNC':'Off','TRACK':'On','SLEW':'Off'})
    # Vendor-private light updates cannot change the standard enclosure permission.
    send('setLightVector','ROOF STATUS',{'ROOF_OPENED':'Idle'},ENCLOSURE,'Ok')
    time.sleep(.05);assert b':STOP#' not in commands;roof()
    # Regression: stale Alert must not remain visible while the next slew's
    # native acknowledgement is delayed beyond Ekos' one-second status poll.
    roof('Idle','Ok')
    start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43})
    wait('EQUATORIAL_EOD_COORD','Alert',start=start)
    roof();state['slew_delay']=1.5
    start=len(events);sent_at=time.monotonic()
    send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43})
    busy=wait('EQUATORIAL_EOD_COORD','Busy',start=start)
    assert time.monotonic()-sent_at<1.0, 'Busy must precede delayed native acknowledgement'
    done=wait('EQUATORIAL_EOD_COORD','Ok',start=busy+1)
    assert not any(a.get('name')=='EQUATORIAL_EOD_COORD' and a.get('state')=='Alert'
                   for _,a,_ in events[busy:done+1]), 'Stale Alert resurfaced during slew'
    assert commands.count(b':MS#')==1
    start=len(events);send('newSwitchVector','NATIVE_SLEW_SIDE',{'AUTO':'Off','WEST':'On','EAST':'Off'})
    send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43})
    wait('EQUATORIAL_EOD_COORD','Busy',start=start);wait('NATIVE_SLEW_SIDE','Ok',{'AUTO':'On'},start)
    start=len(events);send('newSwitchVector','TELESCOPE_ABORT_MOTION',{'ABORT':'On'})
    wait('EQUATORIAL_EOD_COORD','Ok',start=start)
    assert commands.count(b':MSfs2#')==1 and commands.count(b':Q#')==1
    start=len(events);send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'Off','TRACK_OFF':'On'});wait('TELESCOPE_TRACK_STATE',value={'TRACK_OFF':'On'},start=start)
    end=time.monotonic()+3
    while b':AL#' not in commands and time.monotonic()<end:time.sleep(.01)
    assert b':AL#' in commands
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms command completed'},start)
    # Turn tracking on, then invalidate standard enclosure status: STOP and retain fault.
    roof();start=len(events);send('newSwitchVector','TELESCOPE_TRACK_STATE',{'TRACK_ON':'On','TRACK_OFF':'Off'});wait('TELESCOPE_TRACK_STATE','Ok',{'TRACK_ON':'On'},start)
    start=len(events);roof('Ok','Ok')
    end=time.monotonic()+3
    while b':STOP#' not in commands and time.monotonic()<end:time.sleep(.01)
    assert commands.count(b':STOP#')==1
    confirmed=wait('NATIVE_STOP_ALL','Alert',start=start)
    end=time.monotonic()+3
    while time.monotonic()<end:
        messages=[e[2].get('COMMAND','') for e in events[start:] if e[1].get('name')=='NATIVE_MOTION_STATUS']
        if any('Native telemetry confirms STOP completed' in message for message in messages):break
        time.sleep(.01)
    else:raise AssertionError('Safety STOP completion was not confirmed')
    # Locked slew must write no targets or MS.
    time.sleep(.2);count=sum(c.startswith((b':Sr',b':Sd',b':MS')) for c in commands)
    start=len(events);send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43});wait('EQUATORIAL_EOD_COORD','Alert',start=start)
    assert count==sum(c.startswith((b':Sr',b':Sd',b':MS')) for c in commands)
    count=len([c for c in commands if c.startswith((b':Sr',b':Sd',b':CM'))])
    start=len(events);send('newSwitchVector','ON_COORD_SET',{'SYNC':'On','TRACK':'Off','SLEW':'Off'})
    send('newNumberVector','EQUATORIAL_EOD_COORD',{'RA':4,'DEC':43});wait('EQUATORIAL_EOD_COORD','Alert',start=start)
    assert count==len([c for c in commands if c.startswith((b':Sr',b':Sd',b':CM'))])
    send('newSwitchVector','ON_COORD_SET',{'SYNC':'Off','TRACK':'On','SLEW':'Off'})
    configure_roof()
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'})
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'PARK':'On'},busy+1)
    assert commands.count(b':hP#')==1
    # Only a real STOP persists interruption; native Park remains truthful/Alert.
    roof();start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'Off','UNPARK':'On'})
    busy=wait('TELESCOPE_PARK','Busy',start=start);wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},busy+1)
    start=len(events);send('newSwitchVector','TELESCOPE_PARK',{'PARK':'On','UNPARK':'Off'});wait('TELESCOPE_PARK','Busy',start=start)
    start=len(events);send('newSwitchVector','NATIVE_STOP_ALL',{'STOP':'On'})
    wait('NATIVE_STATE',value={'STATUS_CODE':'5'},start=start)
    wait('NATIVE_MOTION_STATUS','Ok',{'COMMAND':'Native telemetry confirms STOP completed; inspect native state'},start)
    wait('TELESCOPE_PARK','Alert',{'PARK':'On'},start)
    config=ET.parse(env['INDICONFIG']).getroot()
    assert config.find(".//newTextVector[@name='NATIVE_PARK_SAFETY']/oneText[@name='INTERRUPTED']").text.strip()=='1'
    # A newly started driver must load the latch through INDI's config mechanism.
    restarted=subprocess.run([sys.argv[1]],input='<getProperties version="1.7"/>\n',capture_output=True,text=True,env=env,timeout=3)
    restored=ET.fromstring('<stream>'+''.join(line for line in restarted.stdout.splitlines() if not line.startswith('<?xml'))+'</stream>')
    assert restored.find(".//defTextVector[@name='NATIVE_PARK_SAFETY']/defText[@name='INTERRUPTED']").text.strip()=='1'
    # No transient PARK/Ok may escape while the explicit STOP warning is set.
    assert not any(a.get('name')=='TELESCOPE_PARK' and a.get('state')=='Ok' and v.get('PARK')=='On'
                   for _,a,v in events[start:])
    # A new MW4 park sequence also resolves the warning without driver motion.
    before=len(commands);start=len(events);state.update(status=7,flag=0,count=0)
    wait('TELESCOPE_PARK','Ok',{'UNPARK':'On'},start)
    start=len(events);state.update(status=2,flag=1,count=3,next=5)
    wait('TELESCOPE_PARK','Busy',start=start)
    wait('TELESCOPE_PARK','Ok',{'PARK':'On'},start)
    assert not any(c in (b':hP#',b':PO#',b':STOP#') for c in commands[before:])
    config=ET.parse(env['INDICONFIG']).getroot()
    assert config.find(".//newTextVector[@name='NATIVE_PARK_SAFETY']/oneText[@name='INTERRUPTED']").text.strip()=='0'
    print('Real INDI callbacks, standard enclosure policy, native completion, abort/STOP, persistent interrupted-park latch and recovery: passed')
finally:
    p.terminate()
    try:p.wait(3)
    except subprocess.TimeoutExpired:p.kill();p.wait()
    server_done.set();thread.join(3)
    config_dir.cleanup()
if reader_errors:raise AssertionError(reader_errors)
if server_errors and not all(isinstance(e,(ConnectionResetError,BrokenPipeError)) for e in server_errors):raise AssertionError(server_errors)
