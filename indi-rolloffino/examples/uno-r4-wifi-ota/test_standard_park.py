import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('uploader', Path(__file__).with_name('upload-wifi.py'))
u = importlib.util.module_from_spec(spec)
spec.loader.exec_module(u)

class ParkTests(unittest.TestCase):
    def run_check(self, changes=None, extra='', code=0):
        fields = {'CONNECTION.CONNECT':'On', 'TELESCOPE_PARK.PARK':'On',
                  'TELESCOPE_PARK.UNPARK':'Off', 'TELESCOPE_PARK._STATE':'Ok'}
        fields.update(changes or {})
        out = '\n'.join('Generic Mount.'+k+'='+v for k,v in fields.items() if v is not None)+extra
        with patch.object(u.subprocess, 'run', return_value=SimpleNamespace(stdout=out,returncode=code)) as run:
            u.standard_parked('Generic Mount','indi.example',7626)
            self.assertIn('indi.example',run.call_args.args[0])
            self.assertIn('7626',run.call_args.args[0])

    def test_completed_park(self): self.run_check()
    def test_identical_duplicate(self): self.run_check(extra='\nGeneric Mount.TELESCOPE_PARK.PARK=On')
    def test_unavailable(self):
        with self.assertRaises(RuntimeError): self.run_check(code=1)
    def test_conflict(self):
        with self.assertRaises(RuntimeError): self.run_check(extra='\nGeneric Mount.TELESCOPE_PARK.PARK=Off')
    def test_unsafe_states(self):
        for change in ({'CONNECTION.CONNECT':'Off'}, {'TELESCOPE_PARK._STATE':'Busy'},
                       {'TELESCOPE_PARK._STATE':'Alert'}, {'TELESCOPE_PARK.PARK':None},
                       {'TELESCOPE_PARK.UNPARK':'On'}):
            with self.subTest(change=change), self.assertRaises(RuntimeError): self.run_check(change)

if __name__ == '__main__': unittest.main()
