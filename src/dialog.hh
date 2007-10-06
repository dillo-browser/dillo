#ifndef __DIALOG_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Dialog_msg(const char *msg);
int a_Dialog_choice3(const char *msg,
                     const char *b0, const char *b1, const char *b2);
const char *a_Dialog_input(const char *msg);
const char *a_Dialog_save_file(const char *msg,
                               const char *pattern, const char *fname);
char *a_Dialog_open_file(const char *msg,
                         const char *pattern, const char *fname);
void a_Dialog_text_window(const char *txt, const char *title);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __DIALOG_HH__
