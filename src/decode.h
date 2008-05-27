#ifndef __DECODE_H__
#define __DECODE_H__

#include "../dlib/dlib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _Decode    Decode;

struct _Decode {
   char *buffer;
   Dstr *leftover;
   void *state;
   Dstr *(*decode) (Decode *dc, const char *instr, int inlen);
   void (*free) (Decode *dc);
};

Decode *a_Decode_transfer_init(const char *format);
Decode *a_Decode_content_init(const char *format);
Decode *a_Decode_charset_init(const char *format);
Dstr *a_Decode_process(Decode *dc, const char *instr, int inlen);
void a_Decode_free(Decode *dc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DECODE_H__ */
