#ifndef XBOX_VERTEX_SUBMIT_H
#define XBOX_VERTEX_SUBMIT_H

/* Textured RGB lighting uses packed color; ordinary scalar lighting is >= 0.
 * Keep the existing 40-byte vertex ABI and scalar fast path. */
#define XBOX_VERTEX_RGB_LIGHT (-1.0f)
#define XBOX_VERTEX_RGBA_EFFECT (-2.0f)

/* Legacy faces use zero as unspecified alpha. Procedural effects use zero
 * as real transparency at their feather; never turn that edge opaque. */
static float xboxVertex_EffectAlpha(unsigned int color, int explicitOpacity)
{
    unsigned int alpha = color >> 24;
    return (float)((explicitOpacity || alpha) ? alpha : 255) / 255.0f;
}

static unsigned int xboxVertex_PackLightRGB(float r, float g, float b, unsigned int alpha)
{
    unsigned int ri = (unsigned int)(r < 0 ? 0 : (r > 255 ? 255 : r));
    unsigned int gi = (unsigned int)(g < 0 ? 0 : (g > 255 ? 255 : g));
    unsigned int bi = (unsigned int)(b < 0 ? 0 : (b > 255 ? 255 : b));
    return (alpha & 0xff000000u) | (ri << 16) | (gi << 8) | bi;
}

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
