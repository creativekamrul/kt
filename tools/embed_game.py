"""Attach KT's portable runtime files as Windows PE resources."""
import ctypes
from ctypes import wintypes
import hashlib
import shutil
import struct

def embed_game(program, destination, files):
    entries = sorted(files.items())
    digest = hashlib.sha256()
    for name, data in entries:
        digest.update(name.encode("utf-8") + b"\0" + hashlib.sha256(data).digest())
    index = digest.hexdigest() + "\n" + "".join(name + "\n" for name, _ in entries)
    shutil.copyfile(program, destination)
    dll = ctypes.WinDLL("kernel32", use_last_error=True)
    begin = dll.BeginUpdateResourceW
    begin.argtypes = (wintypes.LPCWSTR, wintypes.BOOL)
    begin.restype = wintypes.HANDLE
    update = dll.UpdateResourceW
    update.argtypes = (wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wintypes.WORD,
                       ctypes.c_void_p, wintypes.DWORD)
    finish = dll.EndUpdateResourceW
    finish.argtypes = (wintypes.HANDLE, wintypes.BOOL)
    handle = begin(str(destination), False)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())

    def add(kind, number, data):
        if not update(handle, kind, number, 0, data, len(data)):
            raise ctypes.WinError(ctypes.get_last_error())

    try:
        add(10, 900, index.encode("utf-8"))
        for number, (_, data) in enumerate(entries, 1000):
            add(10, number, data)
        png = files["shell/assets/kt-icon.png"]
        width, height = struct.unpack_from(">II", png, 16)
        add(3, 1, png)
        add(14, 1, struct.pack("<HHHBBBBHHIH", 0, 1, 1, width % 256, height % 256,
                               0, 0, 1, 32, len(png), 1))
    except BaseException:
        finish(handle, True)
        raise
    if not finish(handle, False):
        raise ctypes.WinError(ctypes.get_last_error())
