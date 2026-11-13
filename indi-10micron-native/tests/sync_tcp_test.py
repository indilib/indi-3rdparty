"""Offset-only sync framing and rejection; loopback only, no telescope hardware."""
import socket,subprocess,sys,threading,time
for mode in ('success','internal_padding','repeat','parked','busy','ra_rejected','dec_rejected','distance','wrong_mode','bad_reply','timeout','partial','invalid'):
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
                        if cmd==b':Ginfo#':reply=f'3.5,42.25,W,1,45,2461318,{5 if mode=="parked" else 6 if mode=="busy" else 0},{1 if mode=="busy" else 0}#'.encode()
                        elif cmd==b':CMCFG0#':reply=b'1#' if mode=='wrong_mode' else b'0#'
                        elif cmd.startswith(b':Sr'):reply=b'0' if mode=='ra_rejected' else b'1'
                        elif cmd.startswith(b':Sd'):reply=b'0' if mode=='dec_rejected' else b'1'
                        elif cmd==b':CM#':
                            if mode=='timeout':time.sleep(.25);return
                            if mode=='partial':c.sendall(b'Coordinates');return
                            reply=b'Unexpected#' if mode=='bad_reply' else b'Match fail: dist. too large#' if mode=='distance' else b'Coordinates     matched#' if mode=='internal_padding' else b'Coordinates matched             #'
                        else:raise AssertionError(cmd)
                        for byte in reply:c.sendall(bytes([byte]))
        except (BrokenPipeError,ConnectionResetError):pass
        except Exception as e:errors.append(e)
        finally:listener.close()
    t=threading.Thread(target=serve);t.start()
    result=subprocess.run([sys.argv[1],str(port),mode],capture_output=True,text=True,timeout=5);t.join(5)
    assert not t.is_alive() and not errors,(mode,errors)
    assert result.returncode==(1 if mode in ('wrong_mode','bad_reply','timeout','partial') else 0),(mode,result.stderr)
    assert not any(cmd in (b':CMS#',b':CMCFG1#',b':delalig#') for cmd in seen)
    if mode=='invalid':assert seen==[b':U2#'],seen
    elif mode in ('parked','busy'):assert seen==[b':U2#',b':Ginfo#'],seen
    else:
        transaction=[b':Ginfo#',b':CMCFG0#']
        if mode!='wrong_mode':transaction.append(b':Sr03:30:00.00#')
        if mode not in ('wrong_mode','ra_rejected'):transaction.append(b':Sd+42*15:00.0#')
        if mode not in ('wrong_mode','ra_rejected','dec_rejected'):transaction.append(b':CM#')
        assert seen==[b':U2#']+transaction*(2 if mode=='repeat' else 1),(mode,seen)
    print(mode+': passed')
