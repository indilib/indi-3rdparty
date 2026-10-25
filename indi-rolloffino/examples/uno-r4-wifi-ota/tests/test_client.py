import unittest
import importlib.util
from pathlib import Path
import tempfile
import socket
import threading
import zlib
import hashlib
import subprocess
import shutil

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('roof_uploader',ROOT/'upload-wifi.py')
client=importlib.util.module_from_spec(spec);spec.loader.exec_module(client)

class ClientTests(unittest.TestCase):
    def run_server(self,reply):
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1)
        port=listener.getsockname()[1];observed={};errors=[]
        def serve():
            try:
                with listener:
                    conn,_=listener.accept()
                    with conn:
                        conn.settimeout(3)
                        f=conn.makefile('rb')
                        observed['header']=f.readline().decode().split()
                        n=int(observed['header'][2]);observed['body']=f.read(n)
                        conn.sendall(reply+b'\n')
                        if reply==b'READY':
                            observed['commit']=f.readline();conn.sendall(b'APPLYING\n')
                        else: observed['after_error']=f.read(1)
            except Exception as e: errors.append(e)
        worker=threading.Thread(target=serve);worker.start()
        token='a'*32;data=b'wireless firmware body'*32
        try:
            if reply==b'READY':client.transfer('127.0.0.1',data,token,port)
            else:
                with self.assertRaises(RuntimeError):client.transfer('127.0.0.1',data,token,port)
        finally:worker.join(5);listener.close()
        self.assertFalse(worker.is_alive());self.assertFalse(errors,errors)
        self.assertEqual(observed['body'],data)
        self.assertEqual(observed['header'],['ROOFOTA1',token,str(len(data)),f'{zlib.crc32(data):08x}'])
        return observed
    def test_real_client_handshake_over_tcp(self):
        self.assertEqual(self.run_server(b'READY')['commit'],b'COMMIT\n')
    def test_rejection_does_not_send_commit(self):
        self.assertEqual(self.run_server(b'ERROR:crc')['after_error'],b'')
    def test_manifest_detects_changed_image(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'test.bin';token='a'*32;data=b'ID123'+token.encode()+b'x'*300;p.write_bytes(data)
            m={'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'board':'arduino:renesas_uno:unor4wifi','build_id':'ID123'}
            self.assertEqual(client.inspect_image(p,m,token),data)
            p.write_bytes(data+b'x')
            with self.assertRaises(RuntimeError):client.inspect_image(p,m,token)
    def test_preparation_preserves_original_and_token(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'Rolloff').mkdir();shutil.copyfile(ROOT/'prepare.py',root/'prepare.py')
            original=root/'original.h';content='#define WIFI_SSID "fake"\n#define WIFI_PASSWORD "fake"\n';original.write_text(content)
            cmd=['python3',str(root/'prepare.py'),'--secrets',str(original)]
            subprocess.run(cmd,check=True,capture_output=True)
            token=(root/'ota-token.txt').read_text();identity=(root/'build-identity.json').read_text()
            subprocess.run(cmd,check=True,capture_output=True)
            self.assertEqual(original.read_text(),content)
            self.assertEqual((root/'ota-token.txt').read_text(),token)
            self.assertNotEqual((root/'build-identity.json').read_text(),identity)
    def test_expected_identity_and_token_must_be_in_binary(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'test.bin';data=b'x'*400;p.write_bytes(data)
            m={'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'board':'arduino:renesas_uno:unor4wifi','build_id':'missing'}
            with self.assertRaises(RuntimeError):client.inspect_image(p,m,'a'*32)

if __name__=='__main__':unittest.main()
