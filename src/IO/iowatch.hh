#ifndef __IO_WATCH_H__
#define __IO_WATCH_H__

/*
 * BUG: enum {READ = 1, WRITE = 4, EXCEPT = 8} borrowed from FL/Enumerations.H
 */
#define DIO_READ    1
#define DIO_WRITE   4
#define DIO_EXCEPT  8

typedef void (*CbFunction_t)(int fd, void *data);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_IOwatch_add_fd(int fd,int when,CbFunction_t Callback,void *usr_data);
void a_IOwatch_remove_fd(int fd,int when);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __IO_WATCH_H__ */

