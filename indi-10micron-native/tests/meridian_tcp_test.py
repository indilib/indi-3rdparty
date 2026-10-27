import socket, subprocess, sys, threading, time
for form in ('byte', 'hash', 'delayed_hash', 'invalid_side', 'invalid_flag', 'target_none', 'target_east', 'invalid_target', 'dropped_target'):
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
    port=listener.getsockname()[1];seen=[];errors=[]
    responses={b':Glms#':b'5#',b':Glmt#':b'15#',b':Gmte#':b'123#',b':GMF#':b'1#',b':Guaf#':b'1',b':GTsid#':b'2',b':Ginfo#':b'3,42,W,1,-1,2461318,5,0#'}
    if form=='invalid_side':responses[b':GMF#']=b'4#'
    if form=='invalid_flag':responses[b':Guaf#']=b'2'
    target=0 if form=="target_none" else 3 if form=="target_east" else 2
    responses[b":GTsid#"]=str(target).encode()
    if form=="invalid_target":responses[b":GTsid#"]=b"1"
    def serve():
        try:
            listener.settimeout(4)
            with listener.accept()[0] as conn:
                conn.settimeout(4);buffer=b''
                while True:
                    data=conn.recv(256)
                    if not data:break
                    buffer+=data
                    while b'#' in buffer:
                        command,buffer=buffer.split(b'#',1);command+=b'#';seen.append(command)
                        if command==b':U2#':continue
                        if command==b':GTsid#' and form=='dropped_target':return
                        for char in responses[command]:
                            conn.sendall(bytes([char]));time.sleep(0.0002)
                        if command in (b':Guaf#',b':GTsid#') and form in ('hash','delayed_hash'):
                            if form=='delayed_hash':time.sleep(0.02)
                            conn.sendall(b'#')
        except Exception as e:errors.append(e)
        finally:listener.close()
    t=threading.Thread(target=serve);t.start()
    result=subprocess.run([sys.argv[1],str(port),str(target)],capture_output=True,text=True,timeout=6);t.join(5)
    assert not t.is_alive() and not errors,errors
    expected=[b':U2#',b':Glms#',b':Glmt#',b':Gmte#',b':GMF#',b':Guaf#',b':GTsid#',b':Ginfo#']
    assert seen==expected[:len(seen)],seen
    if form.startswith('invalid') or form=='dropped_target':assert result.returncode==1
    else:assert result.returncode==0 and seen==expected,result.stderr
    print(form+': passed')
