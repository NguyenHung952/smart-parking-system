from pathlib import Path
import subprocess
import sys
import time
import urllib.request
import os
import signal

# When packaged by PyInstaller, __file__ points inside the temporary _MEI folder.
# The real project folder is the folder containing SmartParking.exe.
if getattr(sys, "frozen", False):
    ROOT = Path(sys.executable).resolve().parent
else:
    ROOT = Path(__file__).resolve().parent
PYTHON_DIR = ROOT / "Core" / "Python"
WEB_DIR = PYTHON_DIR / "web"
ANPR_DIR = PYTHON_DIR / "ANPR"

VENV_PYTHON = ROOT / ".venv" / "Scripts" / "python.exe"
SYSTEM_PYTHON = Path(r"C:\Python314\python.exe")

# main.py + ANPR: keep the tested project .venv
# Web: use the system Python that has Flask installed.
if not VENV_PYTHON.exists():
    raise FileNotFoundError(f"Missing project Python: {VENV_PYTHON}")

if not SYSTEM_PYTHON.exists():
    raise FileNotFoundError(f"Missing Web Python: {SYSTEM_PYTHON}")

MAIN_APP = PYTHON_DIR / "main.py"
WEB_APP = WEB_DIR / "app.py"
ANPR_APP = ANPR_DIR / "anpr_service.py"

LOG_DIR = ROOT / "logs"
LOG_DIR.mkdir(exist_ok=True)

STOP_FILE = ROOT / "SmartParking.stop"
LAUNCH_LOG = LOG_DIR / "SmartParking_launcher.log"

CREATE_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)

children = []
log_handles = []

def log(msg):
    text = f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {msg}"
    try:
        with LAUNCH_LOG.open("a", encoding="utf-8") as f:
            f.write(text + "\n")
    except Exception:
        pass

def start_process(name, python_exe, script, cwd, extra_args=None, log_name=None):
    args = [str(python_exe), str(script)]
    if extra_args:
        args.extend(extra_args)

    if log_name is None:
        log_name = f"{name}.log"

    handle = (LOG_DIR / log_name).open("a", encoding="utf-8", errors="replace")
    proc = subprocess.Popen(
        args,
        cwd=str(cwd),
        stdin=subprocess.DEVNULL,
        stdout=handle,
        stderr=subprocess.STDOUT,
        creationflags=CREATE_NO_WINDOW,
    )
    children.append(proc)
    log_handles.append(handle)
    log(f"START {name}: PID={proc.pid}, PYTHON={python_exe}")
    return proc

def port_ready(url, timeout=20):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                return response.status == 200
        except Exception:
            time.sleep(0.5)
    return False

def stop_children():
    for proc in reversed(children):
        if proc.poll() is None:
            try:
                proc.terminate()
            except Exception:
                pass

    deadline = time.time() + 5
    for proc in children:
        if proc.poll() is None:
            remaining = max(0.1, deadline - time.time())
            try:
                proc.wait(timeout=remaining)
            except subprocess.TimeoutExpired:
                try:
                    proc.kill()
                except Exception:
                    pass

    for handle in log_handles:
        try:
            handle.close()
        except Exception:
            pass

def cleanup_stop_file():
    try:
        STOP_FILE.unlink(missing_ok=True)
    except Exception:
        pass

def main():
    cleanup_stop_file()
    log("SmartParking V5 launcher starting.")
    log(f"VENV_PYTHON={VENV_PYTHON}")
    log(f"SYSTEM_PYTHON={SYSTEM_PYTHON}")
    log(f"FROZEN={getattr(sys, 'frozen', False)} | ROOT={ROOT}")

    try:
        # Keep ARM KIT / ANPR on the known-good .venv.
        start_process(
            "main",
            VENV_PYTHON,
            MAIN_APP,
            PYTHON_DIR,
            ["--background"],
            "main.log",
        )

        # Web uses C:\Python314 because it contains Flask 3.1.3.
        start_process(
            "web",
            SYSTEM_PYTHON,
            WEB_APP,
            WEB_DIR,
            None,
            "web.log",
        )

        start_process(
            "anpr",
            VENV_PYTHON,
            ANPR_APP,
            ANPR_DIR,
            None,
            "anpr.log",
        )

        if port_ready("http://127.0.0.1:5000/", timeout=20):
            log("Web Dashboard is ready.")
            try:
                os.startfile("http://127.0.0.1:5000/")
            except Exception as exc:
                log(f"Browser open failed: {exc}")
        else:
            log("Web Dashboard did not become ready within 20 seconds.")

        while True:
            if STOP_FILE.exists():
                log("Stop file detected.")
                break

            # Launcher stays alive so the EXE owns the application session.
            alive = [p for p in children if p.poll() is None]
            if not alive:
                log("All child processes exited.")
                break

            time.sleep(1)

    except Exception as exc:
        log(f"Launcher error: {exc}")
    finally:
        stop_children()
        cleanup_stop_file()
        log("SmartParking V5 launcher stopped.")

if __name__ == "__main__":
    main()
