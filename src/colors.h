#ifndef __COLORS_H__
#define __COLORS_H__

#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int32_t a_Color_parse (const char *str, int32_t default_color, int *err);
int32_t a_Color_vc(int32_t candidate, int32_t c1, int32_t c2, int32_t c3);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __COLORS_H__ */
