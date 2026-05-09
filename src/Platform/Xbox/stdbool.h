/* stdbool.h shim for VS2005/Xbox (C89 has no stdbool.h) */
#ifndef _STDBOOL_H_XBOX
#define _STDBOOL_H_XBOX

#ifndef __cplusplus
#ifndef bool
typedef int bool;
#endif
#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif
#endif

#define __bool_true_false_are_defined 1

#endif /* _STDBOOL_H_XBOX */
