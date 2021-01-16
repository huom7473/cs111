#!/usr/bin/python3

# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

import subprocess
import signal
import sys
import os

run_options = {
    "default": ['./lab0'],
    "segfault": ['./lab0', '--segfault'],
    "catch": ['./lab0', '--catch', '--segfault'],
    "redirect" : ['./lab0', '--input', 'input.txt', '--output', 'out.txt'],
    "ferror": ['./lab0', '--input', 'bogus'],
    "aerror": ['./lab0', '--bogus']
}
INPUT_STRING = "test input\ntest input line 2"
    
def test(setting, stdin=""):
    try:
        return subprocess.run(run_options[setting], text=True, input=stdin, capture_output=True)
    except FileNotFoundError:
        print("lab0 executable not found")
        sys.exit(0)

try:
    process = test("default", INPUT_STRING)
    assert process.returncode == 0
    assert process.stdout == INPUT_STRING
    assert process.stderr == ""
except AssertionError:
    print("no-args test failed")
    sys.exit(0)

try:
    process = test("segfault")
    assert process.returncode == -signal.SIGSEGV # this should be true if exit was due to to segfault sig
    assert process.stdout == ""
    assert process.stderr == ""
except AssertionError:
    print("segfault test failed")
    sys.exit(0)

try:
    process = test("catch")
    assert process.returncode == 4
    assert process.stdout == ""
    assert process.stderr == "Caught segfault\n"
except AssertionError:
    print("segfault catch failed")
    sys.exit(0)

try:
    with open('input.txt', 'w') as f:
        f.write(INPUT_STRING)
    
    process = test("redirect")
    assert process.returncode == 0
    assert process.stdout == ""
    assert process.stderr == ""
    with open('out.txt') as o:
        with open('input.txt') as i:
            assert o.read() == i.read()

    os.system('rm *.txt') # clean up after we're done
except AssertionError:
    print("redirection test failed")
    sys.exit(0)


try:
    process = test("ferror")
    assert process.returncode == 2
    assert process.stdout == ""
    assert process.stderr # just make sure some error message is printed
except AssertionError:
    print("input file error test failed")
    sys.exit(0)

try:
    process = test("aerror")
    assert process.returncode == 1
    assert process.stdout == ""
    assert process.stderr # just make sure some error message is printed
except AssertionError:
    print("invalid argument error test failed")
    sys.exit(0)
    
print("smoke test passed")
