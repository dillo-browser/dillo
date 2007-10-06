#ifndef MISC_NEW_H
#define MISC_NEW_H

#include "../dlib/dlib.h"


int a_Misc_close_fd(int fd);
Dstr *a_Misc_rdtag(int socket);
char *a_Misc_readtag(int sock);
char *a_Misc_mkdtemp(char *template);

#endif
