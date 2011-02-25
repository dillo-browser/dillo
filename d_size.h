#ifndef __D_SIZE_H__
#define __D_SIZE_H__


#include "config.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
/* config.h defines {int,uint}*_t */
#endif /* HAVE_INTTYPES_H */
#endif /*HAVE_STDINT_H */

typedef unsigned char   uchar_t;
typedef unsigned short  ushort_t;
typedef unsigned long   ulong_t;
typedef unsigned int    uint_t;
typedef unsigned char   bool_t;


#endif /* __D_SIZE_H__ */
