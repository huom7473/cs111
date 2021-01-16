import subprocess
import os
import re
import time
import sys

run_options = {
    'default': ['./lab4b', '--log', 'log.txt']
}

try:
    with subprocess.Popen(run_options['default'], universal_newlines=True, stdout=subprocess.DEVNULL, stdin=subprocess.PIPE) as p: # rely on logs for output
        p.stdin.write('LOG test\nSCALE=C\n')
        p.stdin.flush()
        time.sleep(3)
        p.stdin.write('OFF\n')
except FileNotFoundError:
    print("lab4b executable not found")
    sys.exit(1)
except:
    print("caught unexpected error during execution of lab4b binary")
    sys.exit(1)

try:
    with open('log.txt') as f:
        lines = f.readlines()
except FileNotFoundError:
    print("log not found")
    sys.exit(1)
except:
    print("caught unexpected error during log file processing")
    sys.exit(1)

if 'LOG test\n' not in lines:
    print("LOG test failed")
if 'SCALE=C\n' not in lines:
    print("SCALE=C not logged")

ftemp = None
ctemp = None

temp_re = re.compile(r'\d\d:\d\d:\d\d (\d\d.\d)')
for line in lines:
    match = temp_re.search(line)
    if match:
        ftemp = match.group(1) # get temperature
        break

for line in lines[::-1]: # search backwards to get celsius temperature
    match = temp_re.search(line) # get temperature
    if match:
        ctemp = match.group(1)
        break

if ftemp is None: # no match the whole time (this would also be the case for ctemp if ftemp didn't match)
    print("temperature listing not found in log file")
    sys.exit(1)

if not (32 < float(ftemp) < 120): # arbitrary limits for room temperature value
    print("warning: fahrenheit temperature found but has unusual value ({})".format(ftemp))

if not (0 < float(ctemp) < 50):
    print("warning: celsius temperature found but has unusual value ({})".format(ctemp))

os.remove('log.txt')
print('smoke tests passed')
