#ifndef __DILLO_MISC_H__
#define __DILLO_MISC_H__

#include <stddef.h>     /* for size_t */
#include <ctype.h>      /* iscntrl, isascii */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define d_isascii(c)  (((c) & ~0x7f) == 0) 

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

/**
 * Parse Content-Disposition string, e.g., "attachment; filename="file name.jpg"".
 * Content-Disposition is defined in RFC 6266
 */
static inline void a_Misc_parse_content_disposition(const char *disposition, char **type, char **filename)
{
   static const char tspecials_space[] = "()<>@,;:\\\"/[]?= ";
   const char terminators[] = " ;\t";
   const char *str, *s;

   if (type)
      *type = NULL;
   if (filename)
      *filename = NULL;
   if (!(str = disposition))
      return;

   for (s = str; *s && d_isascii((uchar_t)*s) && !iscntrl((uchar_t)*s) &&
      !strchr(tspecials_space, *s); s++) ;
   if (type && !(s == str))
      *type = dStrndup(str, s - str);

   if (!*type) {
      return;
   }

   if (!strchr(terminators, *s)) {
      *type = NULL;
      return;
   }

   if (*s == ';') {
      bool_t quoted = FALSE;
      const char key[] = "filename";

      if ((s = dStriAsciiStr(str, key)) &&
          (s == str || strchr(terminators, s[-1]))) {
         s += sizeof(key) - 1;
         for ( ; *s == ' ' || *s == '\t'; ++s);
         if (*s == '=') {
            size_t len = 0;
            for (++s; *s == ' ' || *s == '\t'; ++s);
            if (*s == '"') {
               quoted = TRUE;
               s++;
               for ( ; *s == '.'; ++s);
               bool_t escaped = FALSE;
               const char *c;
               unsigned int maxlen = strlen(s);
               for (c = s; !(*c == '"' && !escaped); c++) {
                  if ((len = c - s + 1) > maxlen) {
                     return;
                  }
                  escaped = *c == '\\';
               }
               *filename = dStrndup(s, len);
            } else {
               for ( ; *s == '.'; ++s);
               if ((len = strcspn(s, terminators))) {
                  *filename = dStrndup(s, len);
               }
            }

            const char invalid_characters[] = "/\\|~";
            char *s = *filename, *d = *filename;

            for ( ; s < *filename + len; s++) {
               if (strchr(invalid_characters, *s)) {
                  // If this is a backslash preceding a quote, we want to just
                  // skip past it without advancing the destination pointer or
                  // copying anything.
                  if (!(*s == '\\' && *(s+1) == '"')) {
                     *d = '_';
                     d++;
                  }
               } else if (!quoted && (!d_isascii((uchar_t)*s) || *s == '=')) {
                  *filename = NULL;
                  return;
               } else {
                  *d = *s;
                  d++;
               }
            }

            // Truncate filename to deal with the string being shorter if we
            // skipped over any backslash characters in the above loop
            if (s != d) {
               *d = '\0';
            }
         }
      }
   }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DILLO_MISC_H__ */

