"""Exercise the standalone Windows download in an empty directory with an isolated user profile."""
from contextlib import closing
from ctypes import byref, c_ulong, create_unicode_buffer, windll, WINFUNCTYPE
from ctypes.wintypes import BOOL, HWND, LPARAM
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time

user32 = windll.user32

def window_for(process):
    found = []
    @WINFUNCTYPE(BOOL, HWND, LPARAM)
    def check(window, unused):
        pid = c_ulong()
        user32.GetWindowThreadProcessId(window, byref(pid))
        if pid.value == process.pid and user32.IsWindowVisible(window):
            title = create_unicode_buffer(64)
            user32.GetWindowTextW(window, title, len(title))
            if title.value == "KT":
                found.append(window)
        return True
    for _ in range(180):
        user32.EnumWindows(check, 0)
        if found: return found[0]
        if process.poll() is not None: break
        time.sleep(.1)
    raise RuntimeError("The standalone game window did not open")

def launch(exe, working_dir, home, save):
    process = subprocess.Popen([str(exe), "--windowed"], cwd=working_dir,
                               env={**os.environ, "LOCALAPPDATA":str(home)})
    try:
        window = window_for(process)
        for _ in range(100):
            if save.is_file():
                with closing(sqlite3.connect(save)) as db:
                    if db.execute("SELECT count(*) FROM sqlite_master WHERE name='children'").fetchone()[0]:
                        if db.execute("SELECT count(*) FROM children").fetchone()[0]: break
            time.sleep(.1)
        else:
            raise RuntimeError("KT failed to initialize the player database")
        time.sleep(.3)
        user32.PostMessageW(window, 0x10, 0, 0)
        if process.wait(timeout=15) != 0: raise RuntimeError("KT failed to close normally")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)

def main():
    binary=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="KT standalone ") as tmp:
        root=Path(tmp)
        folder=root/"download"
        cwd=root/"unrelated"
        home=root/"profile"
        for p in (folder,cwd,home): p.mkdir()
        exe=folder/"KT.exe"
        shutil.copyfile(binary,exe)
        save=home/"KT/sdp100.db"
        launch(exe,cwd,home,save)
        assets,=(home/"KT/assets").iterdir()
        image=assets/"shell/assets/glasswind.png"
        original=image.read_bytes()
        if not original.startswith(b"\x89PNG"): raise RuntimeError("Missing bundled scenery")
        if not (assets/"shell/assets/willow-theme.wav").read_bytes().startswith(b"RIFF"):
            raise RuntimeError("Missing bundled music")
        if sorted(folder.iterdir())!=[exe] or list(cwd.iterdir()):
            raise RuntimeError("Game required or wrote files beside the executable")
        image.write_bytes(b"x"*len(original))
        launch(exe,cwd,home,save)
        if image.read_bytes()!=original: raise RuntimeError("Bundled scenery was not restored")
        exe=exe.rename(folder/"KT renamed.exe")
        portable=folder/"sdp100.db"
        shutil.copyfile(save,portable)
        with closing(sqlite3.connect(portable)) as db:
            db.execute("UPDATE children SET name='Portable tester'");db.commit()
        launch(exe,cwd,home,portable)
        with closing(sqlite3.connect(portable)) as db:
            if not db.execute("SELECT 1 FROM children WHERE name='Player'").fetchone():
                raise RuntimeError("Existing portable save was not used")
    print("Single-file launch, cache repair, user-profile save and legacy portable save passed.")

if __name__=="__main__":
    main()
