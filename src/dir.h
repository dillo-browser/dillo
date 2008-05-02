#ifndef __DIR_H__
#define __DIR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void a_Dir_init(void);
char *a_Dir_get_owd(void);
void a_Dir_free(void);
void a_Dir_check_dillorc_directory(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DIR_H__ */

