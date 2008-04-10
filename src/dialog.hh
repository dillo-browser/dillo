#ifndef __DIALOG_HH__
#define __DIALOG_HH__

#include "bw.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Dialog_msg(const char *msg);
int a_Dialog_choice3(const char *msg,
                     const char *b0, const char *b1, const char *b2);
int a_Dialog_choice5(const char *QuestionTxt,
                     const char *alt1, const char *alt2, const char *alt3,
                     const char *alt4, const char *alt5);
const char *a_Dialog_input(const char *msg);
const char *a_Dialog_save_file(const char *msg,
                               const char *pattern, const char *fname);
char *a_Dialog_open_file(const char *msg,
                         const char *pattern, const char *fname);
void a_Dialog_text_window(const char *txt, const char *title);
void a_Dialog_findtext(BrowserWindow *bw);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __DIALOG_HH__
