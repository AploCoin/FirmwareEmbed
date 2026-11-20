#!/usr/bin/env python3
"""Exercise the post-build gate without PlatformIO or secrets."""
import configparser
from pathlib import Path
import runpy
import json
import tempfile

project = Path(__file__).resolve().parents[1]
config = configparser.ConfigParser(interpolation=None)
config.read(project / 'platformio.ini')
board = config['platformio']['default_envs']
with tempfile.TemporaryDirectory(prefix='lite-image-gate-') as tmp:
    class Env(dict):
        def GetProjectConfig(self): return config
        def StringifyMacro(self, value): return '"' + value + '"'
        def Append(self, **kwargs): pass
        def subst(self, value): return tmp
        def GetProjectOption(self, value): return config['env:' + board][value]
        def AddPostAction(self, target, callback): self.callback = callback
    env = Env(PIOENV=board)
    runpy.run_path(str(project / 'build_metadata.py'), init_globals={'Import':lambda _:None,'env':env})
    assert json.loads((Path(tmp)/'identity.json').read_text())['templateBinary'] is True
    (Path(tmp)/'include').mkdir()
    (Path(tmp)/'include/DeviceConfig.private.h').write_text('// simulated personal build')
    runpy.run_path(str(project / 'build_metadata.py'), init_globals={'Import':lambda _:None,'env':env})
    assert json.loads((Path(tmp)/'identity.json').read_text())['templateBinary'] is False
    path = Path(tmp) / 'firmware.bin'
    marker = ('APLO-LITE-LAYOUT:' + config['release']['layout']).encode('ascii')
    maximum = int(config['env:' + board]['board_upload.maximum_size'])
    path.write_bytes(marker + b'\0' * (maximum - len(marker)))
    env.callback([], [], env)
    for invalid in [b'\0' * maximum, marker + b'\0' * (maximum + 1 - len(marker))]:
        path.write_bytes(invalid)
        try:
            env.callback([], [], env)
        except RuntimeError:
            pass
        else:
            raise AssertionError('gate accepted invalid image')
print('PASS: exact-capacity binary accepted; missing marker and oversized binary rejected')
