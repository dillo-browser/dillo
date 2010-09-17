#ifndef __COOKIES_H__
#define __COOKIES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifdef DISABLE_COOKIES
# define a_Cookies_get_query(url)  dStrdup("")
# define a_Cookies_set()     ;
# define a_Cookies_init()    ;
# define a_Cookies_freeall() ;
#else
  char *a_Cookies_get_query(const DilloUrl *request_url);
  void  a_Cookies_set(Dlist *cookie_string, const DilloUrl *set_url,
                      const char *server_date);
  void  a_Cookies_init( void );
  void  a_Cookies_freeall( void );
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__COOKIES_H__ */
