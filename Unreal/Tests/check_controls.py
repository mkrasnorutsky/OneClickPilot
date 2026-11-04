"""Build and test the native X-Plane UDP receiver under sanitizers."""
# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
core = root / 'Source/OneClickPilot/Core'
with tempfile.TemporaryDirectory(prefix='oneclick-xplane-controls-') as directory:
    binary = Path(directory) / 'check_xplane_transport'
    subprocess.run([
        'xcrun', 'clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-g', '-I', str(core),
        str(root / 'Tests/check_xplane_transport.cpp'), str(core / 'OneClickTransport.cpp'),
        '-o', str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: X-Plane UDP axes, parking brake, flap detents, lights, views and malformed packets')
