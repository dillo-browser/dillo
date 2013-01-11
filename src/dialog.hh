#ifndef __DIALOG_HH__
#define __DIALOG_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*UserPasswordCB)(const char *user, const char *password,
                               void *vp);

void a_Dialog_msg(const char *title, const char *msg);
int a_Dialog_choice(const char *title, const char *msg, ...);
int a_Dialog_user_password(const char *title, const char *msg,
                           UserPasswordCB cb, void *vp);
const char *a_Dialog_input(const char *title, const char *msg);
const char *a_Dialog_passwd(const char *title, const char *msg);
const char *a_Dialog_save_file(const char *title,
                               const char *pattern, const char *fname);
const char *a_Dialog_select_file(const char *title,
                                 const char *pattern, const char *fname);
char *a_Dialog_open_file(const char *title,
                         const char *pattern, const char *fname);
void a_Dialog_text_window(const char *title, const char *txt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __DIALOG_HH__
