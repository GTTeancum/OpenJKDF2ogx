"""Exercise dashboard-selected Xbox video modes and test EEPROM settings."""

import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / "src/Platform/Xbox/fakeglx.cpp").read_text(encoding="utf-8")
start = source.index("static DWORD xboxVideo_Select480PresentFlags(")
end = source.index("static void xboxVideo_Log480Mode(", start)
selector = source[start:end]

fixture = r"""
#include <cassert>
#include <vector>
using DWORD = unsigned long;
using UINT = unsigned;
using HRESULT = long;
#define D3DADAPTER_DEFAULT 0
#define D3DPRESENTFLAG_WIDESCREEN 0x10
#define D3DPRESENTFLAG_INTERLACED 0x20
#define D3DPRESENTFLAG_PROGRESSIVE 0x40
#define XC_VIDEO_FLAGS_WIDESCREEN 0x01
#define XC_VIDEO_FLAGS_HDTV_720p 0x02
#define XC_VIDEO_FLAGS_HDTV_480p 0x08
#define SUCCEEDED(x) ((x) >= 0)
#define D3DFMT_X8R8G8B8 0x07
#define D3DFMT_D16 0x2c
#define D3DFMT_D24S8 0x2a
struct D3DPRESENT_PARAMETERS { DWORD BackBufferFormat, AutoDepthStencilFormat; };
struct D3DDISPLAYMODE { UINT Width, Height, RefreshRate; DWORD Flags; };
struct IDirect3D8 {
    std::vector<D3DDISPLAYMODE> modes;
    bool fail = false;
    UINT GetAdapterModeCount(UINT) { return (UINT)modes.size(); }
    HRESULT EnumAdapterModes(UINT, UINT i, D3DDISPLAYMODE *mode) {
        *mode = modes[i];
        return fail ? -1 : 0;
    }
};
SELECTOR
int main() {
    D3DPRESENT_PARAMETERS params = {};
    xboxVideo_ConfigureBuffers(&params, 720);
    assert(params.BackBufferFormat == D3DFMT_X8R8G8B8);
    assert(params.AutoDepthStencilFormat == D3DFMT_D16);
    xboxVideo_ConfigureBuffers(&params, 480);
    assert(params.BackBufferFormat == D3DFMT_X8R8G8B8);
    assert(params.AutoDepthStencilFormat == D3DFMT_D24S8);
    IDirect3D8 d;
    assert(xboxVideo_Select480PresentFlags(&d, 0) == D3DPRESENTFLAG_INTERLACED);
    assert(xboxVideo_Select480PresentFlags(&d, XC_VIDEO_FLAGS_WIDESCREEN) ==
           (D3DPRESENTFLAG_INTERLACED | D3DPRESENTFLAG_WIDESCREEN));
    d.modes = {{1280, 720, 60, D3DPRESENTFLAG_PROGRESSIVE},
               {640, 480, 50, D3DPRESENTFLAG_PROGRESSIVE}};
    assert(xboxVideo_Select480PresentFlags(&d, XC_VIDEO_FLAGS_HDTV_480p) ==
           D3DPRESENTFLAG_INTERLACED);
    d.modes.push_back({640, 480, 60, D3DPRESENTFLAG_PROGRESSIVE});
    assert(xboxVideo_Select480PresentFlags(&d, XC_VIDEO_FLAGS_HDTV_480p) ==
           D3DPRESENTFLAG_PROGRESSIVE);
    assert(xboxVideo_Select480PresentFlags(&d, XC_VIDEO_FLAGS_HDTV_480p |
                                               XC_VIDEO_FLAGS_WIDESCREEN) ==
           (D3DPRESENTFLAG_PROGRESSIVE | D3DPRESENTFLAG_WIDESCREEN));
    assert(xboxVideo_Select480PresentFlags(&d, 0) == D3DPRESENTFLAG_INTERLACED);
    d.fail = true;
    assert(xboxVideo_Select480PresentFlags(&d, XC_VIDEO_FLAGS_HDTV_480p) ==
           D3DPRESENTFLAG_INTERLACED);
    d.fail = false;
    assert(xboxVideo_CanUse720p(&d, XC_VIDEO_FLAGS_HDTV_720p));
    assert(xboxVideo_CanUse720p(&d, XC_VIDEO_FLAGS_HDTV_720p | XC_VIDEO_FLAGS_HDTV_480p));
    assert(!xboxVideo_CanUse720p(&d, 0));
    assert(!xboxVideo_CanUse720p(&d, XC_VIDEO_FLAGS_HDTV_480p));
    assert(!xboxVideo_CanUse720p(nullptr, XC_VIDEO_FLAGS_HDTV_720p));
    d.fail = true;
    assert(!xboxVideo_CanUse720p(&d, XC_VIDEO_FLAGS_HDTV_720p));
    d.fail = false;
    d.modes = {{640, 480, 60, D3DPRESENTFLAG_PROGRESSIVE}};
    assert(!xboxVideo_CanUse720p(&d, XC_VIDEO_FLAGS_HDTV_720p));
    assert(xboxVideo_Select480PresentFlags(&d, XC_VIDEO_FLAGS_HDTV_720p | XC_VIDEO_FLAGS_HDTV_480p) == D3DPRESENTFLAG_PROGRESSIVE);
    d.modes = {{1280, 720, 50, D3DPRESENTFLAG_PROGRESSIVE},
               {1280, 720, 60, D3DPRESENTFLAG_INTERLACED}};
    assert(!xboxVideo_CanUse720p(&d, XC_VIDEO_FLAGS_HDTV_720p));
}
""".replace("SELECTOR", selector)

compiler = shutil.which("g++") or "C:/msys64/mingw64/bin/g++.exe"
environment = os.environ.copy()
environment["PATH"] = str(Path(compiler).parent) + ";" + environment.get("PATH", "")
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / "video.cpp"
    exe = Path(directory) / "video.exe"
    cpp.write_text(fixture, encoding="utf-8")
    subprocess.run([compiler, "-std=c++11", "-static", str(cpp), "-o", str(exe)],
                   check=True, env=environment)
    subprocess.run([str(exe)], check=True)

spec = importlib.util.spec_from_file_location(
    "test_eeprom", ROOT / "scripts/xbox/set_test_eeprom_aspect.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "eeprom.bin"
    data = bytearray(256)
    struct.pack_into("<I", data, 0x60, 0xFFFFFFFF)
    path.write_bytes(data)
    module.set_video(path, "16:9", "480p")
    result = path.read_bytes()
    assert module.checksum(result) == 0xFFFFFFFF
    assert struct.unpack_from("<I", result, 0x94)[0] == 0x90000
    module.set_video(path, "16:9", "720p")
    result = path.read_bytes()
    assert module.checksum(result) == 0xFFFFFFFF
    assert struct.unpack_from("<I", result, 0x94)[0] == 0xB0000
    module.set_video(path, "4:3", "480i")
    result = path.read_bytes()
    assert module.checksum(result) == 0xFFFFFFFF
    assert struct.unpack_from("<I", result, 0x94)[0] == 0

print("PASS: automatic 720p/480-line fallback, 32-bit color with mode-specific depth, and disposable EEPROM modes")
