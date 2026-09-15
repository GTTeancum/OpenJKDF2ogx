#ifndef XBOX_VIDEO_H
#define XBOX_VIDEO_H
#ifdef __cplusplus
extern "C" {
#endif
/* Physical width / height of a framebuffer pixel; cached at device creation. */
float xboxVideo_GetPixelAspectRatio(void);
/* Camera's logical pixel aspect; pillarboxed 3P/4P views use a 4:3 canvas. */
float xboxVideo_GetProjectionPixelAspectRatio(void);
#ifdef __cplusplus
}
#endif
#endif
