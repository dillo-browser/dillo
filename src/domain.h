#ifndef __DOMAIN_H__
#define __DOMAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "url.h"

void a_Domain_parse(FILE *fp);
void a_Domain_freeall(void);
bool_t a_Domain_permit(const DilloUrl *source, const DilloUrl *dest);

#ifdef __cplusplus
}
#endif

#endif
