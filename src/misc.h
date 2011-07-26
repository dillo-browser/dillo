#ifndef __DILLO_MISC_H__
#define __DILLO_MISC_H__

#include <stddef.h>     /* for size_t */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


char *a_Misc_escape_chars(const char *str, const char *esc_set);
int a_Misc_expand_tabs(char **start, char *end, char *buf, int buflen);
int a_Misc_get_content_type_from_data(void *Data, size_t Size,const char **PT);
int a_Misc_content_type_check(const char *EntryType, const char *DetectedType);
void a_Misc_parse_content_type(const char *str, char **major, char **minor,
                               char **charset);
int a_Misc_content_type_cmp(const char* ct1, const char *ct2);
int a_Misc_parse_geometry(char *geom, int *x, int *y, int *w, int *h);
int a_Misc_parse_search_url(char *source, char **label, char **urlstr);
char *a_Misc_encode_base64(const char *in);
Dstr *a_Misc_file2dstr(const char *filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DILLO_MISC_H__ */

