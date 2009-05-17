#ifndef __UTF8_HH__
#define __UTF8_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "d_size.h"

uint_t a_Utf8_end_of_char(const char *str, uint_t i);
int a_Utf8_encode(unsigned int ucs, char *buf);
int a_Utf8_test(const char* src, unsigned int srclen);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UTF8_HH__ */

