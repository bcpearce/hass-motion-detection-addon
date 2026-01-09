#!/usr/bin/env python3
"""
TestLive555_Integration

Integration test for Live555
"""

import subprocess
import urllib.request
import sys
import re
import signal

SERVE_THIS_FILE = "test.264"


def run_test(executable_path: str, target_url: str) -> int:
    """Run GTest Executable"""
    result = subprocess.run([executable_path, f"--targetUrl={target_url}"])
    return result.returncode


if __name__ == "__main__":
    """Run test suite"""
    res = -1
    print(f"--- [INFO] Downloading test file {sys.argv[1]} ---")
    urllib.request.urlretrieve(sys.argv[1], f"./{SERVE_THIS_FILE}")
    try:
        rtsp_server_proc = subprocess.Popen(
            [sys.argv[2]],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,  # Line buffering
        )

        print(f"--- [INFO] Starting command: {sys.argv[2]} ---")

        line_contains_url = False
        for line in rtsp_server_proc.stdout:
            print(f"--- -- [RTSP SERVER] {line.strip()}")
            if line_contains_url:
                url_pattern = r"\"(rtsp://\S+)\""
                urls = re.findall(url_pattern, line.strip())
                line_contains_url = False
                print(f"--- -- -- [RUNNING TEST] {sys.argv[3]}")
                res = run_test(sys.argv[3], urls[0])
                break

            if line.strip().endswith(f'"{SERVE_THIS_FILE}"'):
                line_contains_url = True
    finally:
        rtsp_server_proc.send_signal(signal.SIGINT)
        rtsp_server_proc.wait(timeout=10)
        print(
            f"--- -- [RTSP SERVER] finished with return code: {rtsp_server_proc.returncode} ---"
        )

    exit(res)
