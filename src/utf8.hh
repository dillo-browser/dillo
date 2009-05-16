#ifndef __UTF8_HH__
#define __UTF8_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int a_Utf8_encode(unsigned int ucs, char *buf);
int a_Utf8_test(const char* src, unsigned int srclen);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UTF8_HH__ */

