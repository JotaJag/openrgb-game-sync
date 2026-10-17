"""Bound subprocess work and clean up its process group on timeout."""
import os
import signal
import subprocess


def bounded_run(command, *, timeout, env=None):
    process = subprocess.Popen(command,env=env,stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL,start_new_session=True)
    try:
        return process.wait(timeout=timeout)
    except BaseException:
        try: os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError: pass
        process.wait()
        raise
