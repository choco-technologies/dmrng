#!/usr/bin/env python3
"""Run the RNG diagnostic over UART; uses dmod-boot's standard-library Link."""
import argparse
import importlib.util
from pathlib import Path
import re
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--boot-dir', type=Path, required=True)
parser.add_argument('--port', default='/dev/ttyACM0')
parser.add_argument('--runs', type=int, default=3)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.runs < 1:
    parser.error('--runs must be positive')
spec = importlib.util.spec_from_file_location('ymodem_send', args.boot_dir / 'scripts/ymodem_send.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
link = module.Link(args.port, 921600)
transcript = []
try:
    for run in range(args.runs):
        link.write(b'\r\ntest_dmrng\r\n')
        deadline = time.monotonic() + 20
        data = bytearray()
        while time.monotonic() < deadline:
            data.extend(link.drain(.2))
            if b'=== Results:' in data and b'> ' in data:
                break
        plain = re.sub(r'\x1b\[[0-9;]*m', '', data.decode(errors='replace'))
        transcript.append(plain)
        if '=== Results: 6/6 passed ===' not in plain:
            raise RuntimeError(f'run {run + 1} did not pass: {plain}')
        for forbidden in ('insufficient stack', 'invalid pointer', 'FAILED', 'Hard fault'):
            if forbidden in plain:
                raise RuntimeError(f'run {run + 1}: {forbidden}')
        print(f'run {run + 1}: 6/6 passed')
finally:
    link.close()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(line.rstrip() for line in ''.join(transcript).splitlines()) + '\n')
