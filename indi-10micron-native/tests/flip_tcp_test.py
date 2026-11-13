"""Native FLIP request tests. Loopback only; never contacts telescope hardware."""
import socket,subprocess,sys,threading,time
for mode in ('accepted','different_target','hash','delayed_hash','rejected','rejected_moving','rejected_side','rejected_pointing','rejected_drop','parked','busy','settling','tracking_off','fault','bad_reply','timeout','disconnect'):
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
    port=listener.getsockname()[1];seen=[];errors=[]
    def serve():
        try:
            listener.settimeout(4)
            with listener.accept()[0] as c:
                c.settimeout(4);buf=b''
                while True:
                    data=c.recv(256)
                    if not data:break
                    buf+=data
                    while b'#' in buf:
                        cmd,buf=buf.split(b'#',1);cmd+=b'#';seen.append(cmd)
                        if cmd==b':U2#':continue
                        if cmd==b':Ginfo#':
                            if mode=='rejected_drop' and b':FLIP#' in seen:return
                            changed=b':FLIP#' in seen
                            side='E' if mode=='rejected_side' and changed else 'W'
                            ra=4.0 if mode=='rejected_pointing' and changed else 3.5
                            status=6 if mode=='rejected_moving' and changed else {'parked':5,'busy':6,'tracking_off':7,'fault':99}.get(mode,0)
                            reply=f'{ra},42.25,{side},1,45,2461318,{status},{int(mode=="settling")}#'.encode()
                        elif cmd==b':FLIP#':
                            if mode=='disconnect':return
                            if mode=='timeout':time.sleep(.25);return
                            reply=b'9' if mode=='bad_reply' else b'0' if mode.startswith('rejected') else b'1'
                        else:raise AssertionError(cmd)
                        for byte in reply:c.sendall(bytes([byte]))
                        if cmd==b':FLIP#' and mode in ('hash','delayed_hash'):
                            if mode=='delayed_hash':time.sleep(.005)
                            c.sendall(b'#')
        except (BrokenPipeError,ConnectionResetError):pass
        except Exception as e:errors.append(e)
        finally:listener.close()
    t=threading.Thread(target=serve);t.start()
    result=subprocess.run([sys.argv[1],str(port),mode],capture_output=True,text=True,timeout=5);t.join(5)
    assert not t.is_alive() and not errors,(mode,errors)
    failing=mode in ('bad_reply','timeout','disconnect','rejected_drop')
    assert result.returncode==(1 if failing else 0),(mode,result.stderr)
    blocked=mode in ('parked','busy','settling','tracking_off','fault','different_target')
    expected=[b':U2#',b':Ginfo#']+([] if blocked else [b':FLIP#'])+([b':Ginfo#'] if mode.startswith('rejected') else [])+([] if failing else [b':Ginfo#'])
    assert seen==expected,(mode,seen) # No target writes, setting changes or automatic retry.
    print(mode+': passed')
