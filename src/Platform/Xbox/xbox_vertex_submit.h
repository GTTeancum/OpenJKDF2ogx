#ifndef XBOX_VERTEX_SUBMIT_H
#define XBOX_VERTEX_SUBMIT_H

/* Engine D3DVERTEX_ext layout copied into the Xbox adapter's render list. */
typedef struct XboxEngineVertex
{
    float x, y, z;
    float nx, ny, nz;
    float tu, tv;
    unsigned int color;
    float lightLevel;
} XboxEngineVertex;

typedef char XboxEngineVertex_size_check[sizeof(XboxEngineVertex) == 40 ? 1 : -1];

#endif
