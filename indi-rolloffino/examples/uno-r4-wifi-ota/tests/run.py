#!/usr/bin/env python3
"""Compile actual baseline/candidate sketches against host hardware doubles only."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='roof-host-test-') as tmp:
 for label,defines in [('original',['-DORIGINAL_SKETCH']),('candidate',[])]:
  exe=str(pathlib.Path(tmp)/label)
  subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-DROOF_HOST_TEST','-I'+str(root/'stubs'),*defines,str(root/'test_sketch.cpp'),'-o',exe],check=True)
  subprocess.run([exe],check=True)
 exe=str(pathlib.Path(tmp)/'upload')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(root/'test_upload.cpp'),'-o',exe],check=True)
 subprocess.run([exe],check=True)
