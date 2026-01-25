# /usr/bin/env python

import subprocess
import tempfile
import json
from pathlib import Path
import shutil
import signal
import time


def test_end_to_end(rtsp_server, motion_detection, resource_file):
    try:
        with tempfile.TemporaryDirectory() as td:
            modet_proc: subprocess.Popen | None = None
            rtsp_proc: subprocess.Popen | None = None
            shutil.copy(resource_file, Path(td))
            rtsp_proc = subprocess.Popen(
                [rtsp_server], stderr=subprocess.PIPE, cwd=Path(td)
            )
            # delay until the url is available
            on_next_line = False
            for line in rtsp_proc.stderr:
                line_utf8 = line.decode("utf-8").strip()
                if on_next_line:
                    url = line_utf8.split('"')[-2]
                    print(f"Feed available at {url}")
                    break

                on_next_line = line_utf8.endswith('"test.264"')

            # create the feed configuration
            feed_config = {"main": {"sourceUrl": url}}
            feed_path = Path(td) / "feeds.json"
            with open(feed_path, "w+") as fp:
                json.dump(feed_config, fp)
                print(f"Wrote config '{json.dumps(feed_config)}' to {feed_path}")

            modet_proc = subprocess.Popen(
                [motion_detection, "-c", feed_path.absolute()]
            )
            time.sleep(5)
            modet_proc.send_signal(signal.SIGINT)
            modet_proc.wait(10.0)
            assert modet_proc.returncode == 0

    finally:
        if modet_proc is not None:
            modet_proc.kill()
        if rtsp_proc is not None:
            rtsp_proc.kill()
