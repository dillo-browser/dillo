#ifndef __IO_H__
#define __IO_H__

#include "d_size.h"
#include "../../dlib/dlib.h"
#include "../chain.h"

/*
 * IO Operations
 */
#define IORead   0
#define IOWrite  1
#define IOClose  2
#define IOAbort  3

/*
 * IO Flags
 */
#define IOFlag_ForceClose  (1 << 1)
#define IOFlag_SingleWrite (1 << 2)

/*
 * IO constants
 */
#define IOBufLen         8192


/*
 * Exported functions
 */
/* Note: a_IO_ccc() is defined in Url.h together with the *_ccc() set */


/*
 * Exported data
 */
extern const char *AboutSplash;


#endif /* __IO_H__ */

