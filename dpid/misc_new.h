#ifndef MISC_NEW_H
#define MISC_NEW_H

Dstr *a_Misc_rdtag(int socket);
char *a_Misc_readtag(int sock);
char *a_Misc_mkdtemp(char *template);
char *a_Misc_mkfname(char *template);
char *a_Misc_mksecret(int nchar);

#endif
