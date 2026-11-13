import os, pty, select, subprocess, sys, threading
binary=sys.argv[1]
for baud in (1200,2400,4800,9600,19200,38400,57600,115200):
    master,slave=pty.openpty();seen=[]
    def emulate():
        buf=b''
        while select.select([master],[],[],1)[0]:
            buf+=os.read(master,1024)
            while b'#' in buf:
                cmd,buf=buf.split(b'#',1);cmd+=b'#';seen.append(cmd)
                if cmd==b':Ginfo#':os.write(master,b'3,42,W,1,45,2461318,0,0#')
                if cmd==b':GVP#':
                    for byte in b'10micron GM1000HPS#':os.write(master,bytes([byte]))
    t=threading.Thread(target=emulate);t.start()
    result=subprocess.run([binary,os.ttyname(slave),str(baud)],capture_output=True,text=True,timeout=3)
    t.join();os.close(master);os.close(slave)
    assert result.returncode==0,result.stderr
    assert seen==[b':U2#',b':GVP#',b':Rg7.5000#',b':Ginfo#',b':Mgn0123#'],seen
for path,baud,error in [('/missing-10u-device','9600','Cannot open'),('/missing-10u-device','12345','Unsupported')]:
    r=subprocess.run([binary,path,baud],capture_output=True,text=True)
    assert r.returncode==1 and error in r.stderr,r
master,slave=pty.openpty()
r=subprocess.run([binary,os.ttyname(slave),'9600'],capture_output=True,text=True,timeout=2)
assert r.returncode==1 and 'protocol timeout' in r.stderr,r
wire=os.read(master,1024)
assert wire==b':U2#:GVP#',wire  # no replay and no control after failed handshake
os.close(master);os.close(slave)
print('PASS: all serial rates, fragmented replies, commands, invalid path/rate, timeout without replay')
