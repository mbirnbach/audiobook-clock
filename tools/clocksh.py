#!/usr/bin/env python3
"""Run a shell command on the clock over its USB serial console (CDC-ACM).

usage: clocksh.py [-t timeout] '<command>'
       clocksh.py --wait [-t timeout]     wait until the console answers
       clocksh.py --push <local> <remote> copy a (small) file over the console

Finds /dev/cu.usbmodem*, wraps the command in unique markers and prints the
output between them; exits with the command's exit status.
"""
import argparse
import glob
import os
import select
import sys
import termios
import time
import uuid


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    return ports[0] if ports else None


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0                                  # iflag
    attrs[1] = 0                                  # oflag
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0                                  # lflag: raw
    attrs[4] = attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def read_until(fd, needle, timeout):
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            try:
                chunk = os.read(fd, 65536)
            except BlockingIOError:
                continue
            except OSError:  # port went away (clock rebooted)
                break
            if not chunk:
                break
            buf += chunk
            if needle in buf:
                return buf
    return buf


def run(fd, cmd, timeout):
    tag = uuid.uuid4().hex[:8]
    start, done = f"@@S{tag}@@", f"@@E{tag}@@"
    # The tty echoes our command line back; quoting part of each marker keeps
    # the echoed copy from matching the markers the shell actually prints.
    line = (f"stty -echo 2>/dev/null; echo @@S\"{tag}\"@@; {cmd}; "
            f"echo @@E\"{tag}\"@@$?@@\n")
    os.write(fd, line.encode())
    buf = read_until(fd, done.encode(), timeout)
    buf += read_until(fd, b"@@\n", 1.0) if done.encode() in buf else b""
    text = buf.decode(errors="replace").replace("\r", "")
    if start not in text:
        return None, text
    body = text.split(start, 1)[1]
    if done not in body:
        return None, body
    out, rest = body.split(done, 1)
    status = int(rest.split("@@", 1)[0] or 1)
    return status, out.lstrip("\n")


def push(fd, local, remote):
    import base64
    data = base64.b64encode(open(local, "rb").read()).decode()
    tmp = remote + ".b64"
    status, out = run(fd, f"rm -f '{tmp}'", 10)
    step = 700  # busybox ash line editing caps input lines at 1024 bytes
    for i in range(0, len(data), step):
        status, out = run(fd, f"printf %s '{data[i:i + step]}' >> '{tmp}'", 10)
        if status != 0:
            print(f"clocksh: push failed at {i}: {out}", file=sys.stderr)
            return 1
    status, out = run(fd, f"base64 -d '{tmp}' > '{remote}' && rm '{tmp}' && ls -l '{remote}'", 30)
    sys.stdout.write(out)
    return status


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-t", "--timeout", type=float, default=30)
    ap.add_argument("--wait", action="store_true")
    ap.add_argument("--push", nargs=2, metavar=("LOCAL", "REMOTE"))
    ap.add_argument("command", nargs="?")
    a = ap.parse_args()

    end = time.time() + a.timeout
    if a.wait:
        # right after a reboot the old port can linger and vanish under us
        while time.time() < end:
            try:
                port = find_port()
                if port:
                    fd = open_port(port)
                    os.write(fd, b"\x03\n")
                    read_until(fd, b"#", 2)
                    status, out = run(fd, "true", 5)
                    if status == 0:
                        print(f"clocksh: {port} ready")
                        return 0
                    os.close(fd)
            except OSError:
                pass
            time.sleep(1)
        print("clocksh: no answer", file=sys.stderr)
        return 1
    port = None
    while time.time() < end:
        port = find_port()
        if port:
            break
        time.sleep(0.5)
    if not port:
        print("clocksh: no /dev/cu.usbmodem* found", file=sys.stderr)
        return 2

    fd = open_port(port)
    os.write(fd, b"\x03\n")  # Ctrl-C: leave any half-typed command behind
    read_until(fd, b"#", 2)
    if a.wait:
        status, out = run(fd, "true", max(5, end - time.time()))
        print(f"clocksh: {port} {'ready' if status == 0 else 'no answer'}")
        return 0 if status == 0 else 1
    if a.push:
        return push(fd, *a.push)
    status, out = run(fd, a.command, a.timeout)
    sys.stdout.write(out)
    if status is None:
        print("\nclocksh: timed out", file=sys.stderr)
        return 124
    return status


if __name__ == "__main__":
    sys.exit(main())
