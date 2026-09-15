"""Verify the production D3D transform submission cache with a fake device."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/Platform/Xbox/fakeglx.cpp').read_text()
method = source[source.index('\tvoid SubmitTransform('):source.index('\tvoid EnsureDriverInfo()')]
program = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
struct D3DMATRIX { float m[16]; };
typedef int D3DTRANSFORMSTATETYPE;
#define SUCCEEDED(x) ((x)>=0)
struct Device {
    int calls=0; bool fail=false;
    D3DMATRIX actual[4]{};
    int SetTransform(int type,const D3DMATRIX* m) {
        ++calls; if(fail) return -1;
        actual[type]=*m; return 0;
    }
};
struct Cache {
    Device* m_pD3DDev;
    D3DMATRIX m_submittedTransforms[4];
    unsigned int m_submittedTransformMask=0;
    METHOD
};
int main() {
    Device d; Cache c{&d}; D3DMATRIX identity{};
    for(int i=0;i<16;i+=5) identity.m[i]=1;
    for(int n=0;n<1000;++n) for(int slot=0;slot<4;++slot)
        c.SubmitTransform(slot,slot,&identity);
    assert(d.calls==4);
    for(int n=0;n<100;++n) {
        D3DMATRIX camera=identity; camera.m[12]=n*.125f;
        c.SubmitTransform(0,0,&camera);
        assert(!memcmp(&d.actual[0],&camera,sizeof(camera)));
        c.SubmitTransform(2,2,&camera);
        c.SubmitTransform(2,2,&identity); // HUD/world transition
        assert(!memcmp(&d.actual[2],&identity,sizeof(identity)));
    }
    D3DMATRIX changed=identity; changed.m[0]=2;
    d.fail=true; int before=d.calls;
    c.SubmitTransform(3,3,&changed); c.SubmitTransform(3,3,&changed);
    assert(d.calls==before+2);
    d.fail=false; c.SubmitTransform(3,3,&changed);
    assert(!memcmp(&d.actual[3],&changed,sizeof(changed)));
    c.m_submittedTransformMask=0; before=d.calls;
    c.SubmitTransform(3,3,&changed); assert(d.calls==before+1);
    puts("PASS: redundant calls removed; per-slot state, camera/HUD changes, failed calls, invalidation preserved");
}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test.exe'
    cpp.write_text(program)
    subprocess.run([shutil.which('clang++') or r'C:\Program Files\LLVM\bin\clang++.exe',
                    '-std=c++14', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
