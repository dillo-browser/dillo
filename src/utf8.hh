#ifndef __UTF8_HH__
#define __UTF8_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "d_size.h"

/*
 * Unicode replacement character U+FFFD
 * "used to replace an incoming character whose value is unknown or otherwise
 * unrepresentable in Unicode"
 */
static const char utf8_replacement_char[] = "\xEF\xBF\xBD";

/* Unicode zero width space U+200B */
static const char utf8_zero_width_space[] = "\xE2\x80\x8B";

uint_t a_Utf8_end_of_char(const char *str, uint_t i);
uint_t a_Utf8_decode(const char*, const char* end, int* len);
int a_Utf8_encode(unsigned int ucs, char *buf);
int a_Utf8_test(const char* src, unsigned int srclen);
bool_t a_Utf8_ideographic(const char *s, const char *end, int *len);
bool_t a_Utf8_combining_char(int unicode);
int a_Utf8_char_count(const char *str, int len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UTF8_HH__ */

