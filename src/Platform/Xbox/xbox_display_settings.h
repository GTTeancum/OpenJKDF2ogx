#ifndef XBOX_DISPLAY_SETTINGS_H
#define XBOX_DISPLAY_SETTINGS_H

#include <math.h>

static int xboxDisplay_Clamp(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

/* Identity at 100/100; contrast is centered on mid-gray, then gamma.
 * Clamp before pow so the endpoints never produce invalid values. */
static unsigned char xboxDisplay_RampValue(int input, int gamma, int contrast)
{
    double value = ((double)input / 255.0 - 0.5) * contrast / 100.0 + 0.5;
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    value = pow(value, 100.0 / gamma);
    return (unsigned char)(value * 255.0 + 0.5);
}

/* Round shared viewport edges identically so split-screen has no seams. */
static int xboxDisplay_SafeEdge(int edge, int extent, int percent)
{
    return (extent * (100 - percent) + 2 * edge * percent + 100) / 200;
}

#ifdef __cplusplus
extern "C" {
#endif
void xboxVideo_SetDisplaySettings(int gamma, int contrast, int safeX, int safeY);
void xboxVideo_ShowSafeZoneMarkers(int visible);
#ifdef __cplusplus
}
#endif
#endif
