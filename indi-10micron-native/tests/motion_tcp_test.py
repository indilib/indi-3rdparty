"""Local protocol emulator; never connects to telescope hardware."""
import socket, subprocess, sys, threading, time
forms=['accepted','hash','delayed_hash','forced','concurrent','parked','busy','ra_rejected','dec_rejected','invalid_input','controls','bad_reply','timeout','partial_error','drop_after_slew']+[f'reject{i}' for i in range(1,6)]
for form in forms:
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
    seen=[];errors=[];port=listener.getsockname()[1]
    code=int(form[-1]) if form.startswith('reject') else -1 if form in ('parked','busy','ra_rejected','dec_rejected') else 0
    def serve():
        try:
            listener.settimeout(4)
            with listener.accept()[0] as conn:
                conn.settimeout(4);buf=b''
                while True:
                    data=conn.recv(256)
                    if not data:break
                    buf+=data
                    while b'#' in buf:
                        command,buf=buf.split(b'#',1);command+=b'#';seen.append(command)
                        if command in (b':U2#',b':Q#',b':STOP#',b':hP#',b':PO#',b':AP#',b':AL#'):continue
                        if command==b':Ginfo#':
                            state=5 if form=='parked' else 6 if form=='busy' else 7
                            response=f'3.5,42.25,W,1,45,2461318,{state},0#'.encode()
                        elif command.startswith(b':Sr'):response=b'0' if form=='ra_rejected' else b'1'
                        elif command.startswith(b':Sd'):response=b'0' if form=='dec_rejected' else b'1'
                        elif command in (b':MS#',b':MSfs2#'):
                            if form=='drop_after_slew':return
                            if form=='timeout':time.sleep(0.4);return
                            if form=='partial_error':conn.sendall(b'3Incomplete');return
                            response=b'9' if form=='bad_reply' else str(code).encode()+b'Rejected by emulator#' if code>0 else b'0'
                        else:raise AssertionError(command)
                        for c in response:conn.sendall(bytes([c]))
                        if not response.endswith(b'#') and form in ('hash','delayed_hash'):
                            if form=='delayed_hash':time.sleep(0.005)
                            conn.sendall(b'#')
        except (BrokenPipeError, ConnectionResetError):pass
        except Exception as e:errors.append(e)
        finally:listener.close()
    t=threading.Thread(target=serve);t.start()
    result=subprocess.run([sys.argv[1],str(port),str(code),form],capture_output=True,text=True,timeout=8);t.join(5)
    assert not t.is_alive() and not errors,(form,errors)
    failing=form in ('bad_reply','timeout','partial_error','drop_after_slew')
    assert result.returncode==(1 if failing else 0),(form,result.stderr)
    if form=='controls':assert seen==[b':U2#',b':Q#',b':STOP#',b':hP#',b':PO#',b':AP#',b':AL#',b':Ginfo#'],seen
    elif form=='invalid_input':assert seen==[b':U2#',b':Ginfo#'],seen
    elif form in ('parked','busy'):assert seen==[b':U2#',b':Ginfo#',b':Ginfo#'],seen
    else:
        start=seen.index(b':Sr03:30:00.00#')
        expected=[b':Sr03:30:00.00#']
        if form!='ra_rejected':expected.append(b':Sd+42*15:00.0#')
        if form not in ('ra_rejected','dec_rejected'):expected.append(b':MSfs2#' if form=='forced' else b':MS#')
        assert seen[start:start+len(expected)]==expected,(form,seen)
        assert sum(c.startswith(b':MS') for c in seen)==(0 if form in ('ra_rejected','dec_rejected') else 1),(form,seen)
    print(form+': passed')
