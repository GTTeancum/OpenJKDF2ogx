"""Compile the production binding code; reproduce delete/reuse across stages."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/Platform/Xbox/fakeglx.cpp').read_text()

def block(text, start):
    begin = text.index(start)
    opening = text.index('{', begin)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]

classes = source[source.index('class TextureEntry {'):source.index('#if 1\n#define Clamp')]
stages = source[source.index('#define MAXSTATES'):source.index('// This class buffers')]
stages = stages.replace(block(stages, 'void SetTextureStageState('), '')
methods = '\n'.join(block(source, name) for name in (
    'void glBindTexture(', 'void glDeleteTextures('))
code = '''
#include <cassert>
#include <cstring>
typedef unsigned int GLuint, DWORD, GLenum;
typedef int GLint, GLsizei, D3DFORMAT;
typedef float GLfloat;
enum { D3DFMT_UNKNOWN, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT,
       GL_MODULATE, GL_TEXTURE_2D };
struct IDirect3DTexture8 { void Release() {} };
#define RELEASENULL(p) if (p) { (p)->Release(); (p)=0; }
void LocalDebugBreak() { assert(false); }
''' + classes + stages + '''
struct BindingHarness {
    TextureTable m_textures;
    TextureState m_textureState;
    void internalEnd() {}
    void SetRenderStateDirty() {}
''' + methods + '''
};
int main() {
    BindingHarness gl;
    gl.m_textureState.SetMaxStages(2);
    IDirect3DTexture8 original, replacement, unrelated;
    GLuint id=14, other=15;
    gl.glBindTexture(GL_TEXTURE_2D,id);
    gl.m_textures.SetTexture(&original,1,4);
    gl.m_textureState.SetCurrentStage(1);
    gl.glBindTexture(GL_TEXTURE_2D,id);
    gl.glDeleteTextures(1,&id);
    assert(gl.m_textureState.GetCurrentTexture()==0);
    gl.m_textureState.SetCurrentStage(0);
    assert(gl.m_textureState.GetCurrentTexture()==0);
    gl.glBindTexture(GL_TEXTURE_2D,id);
    gl.m_textures.SetTexture(&replacement,1,4);
    assert(gl.m_textures.GetCurrentID()==id);
    assert(gl.m_textures.GetMipMap(id)==&replacement);
    assert(gl.m_textures.GetMipMap(0)==0);
    gl.m_textureState.SetCurrentStage(1);
    gl.glBindTexture(GL_TEXTURE_2D,other);
    gl.m_textures.SetTexture(&unrelated,1,4);
    gl.glDeleteTextures(1,&id);
    assert(gl.m_textureState.GetCurrentTexture()==other);
    assert(gl.m_textures.GetCurrentID()==other);
    assert(gl.m_textures.GetMipMap(other)==&unrelated);
    gl.glDeleteTextures(1,&id); // deleting an already deleted name is harmless
    GLuint zero=0;
    gl.glDeleteTextures(1,&zero);
    gl.glDeleteTextures(0,0);
    assert(gl.m_textures.GetMipMap(other)==&unrelated);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test.exe'
    cpp.write_text(code)
    subprocess.run([shutil.which('clang++') or r'C:\Program Files\LLVM\bin\clang++.exe',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: deleted bindings reset on every stage; recycled IDs upload to the correct texture')
