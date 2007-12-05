/*
 * File: IO.c
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo's event driven IO engine
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include "../msg.h"
#include "../chain.h"
#include "../klist.h"
#include "../list.h"
#include "IO.h"
#include "iowatch.hh"

#define DEBUG_LEVEL 5
//#define DEBUG_LEVEL 1
#include "../debug.h"

/*
 * Symbolic defines for shutdown() function
 * (Not defined in the same header file, for all distros --Jcid)
 */
#define IO_StopRd   0
#define IO_StopWr   1
#define IO_StopRdWr 2


typedef struct {
   int Key;               /* Primary Key (for klist) */
   int Op;                /* IORead | IOWrite */
   int FD;                /* Current File Descriptor */
   int Flags;             /* Flag array (look definitions above) */
   int Status;            /* errno code */
   Dstr *Buf;             /* Internal buffer */

   void *Info;            /* CCC Info structure for this IO */
   int events;            /* FLTK events for this IO */
} IOData_t;


/*
 * Local data
 */
static Klist_t *ValidIOs = NULL; /* Active IOs list. It holds pointers to
                                  * IOData_t structures. */

/*
 *  Forward declarations
 */
void a_IO_ccc(int Op, int Branch, int Dir, ChainLink *Info,
              void *Data1, void *Data2);


/* IO API  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * Return a newly created, and initialized, 'io' struct
 */
static IOData_t *IO_new(int op, int fd)
{
   IOData_t *io = dNew0(IOData_t, 1);
   io->Op = op;
   io->FD = fd;
   io->Flags = 0;
   io->Key = 0;
   io->Buf = dStr_sized_new(IOBufLen);

   return io;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * Register an IO in ValidIOs
 */
static void IO_ins(IOData_t *io)
{
   if (io->Key == 0) {
      io->Key = a_Klist_insert(&ValidIOs, io);
   }
   _MSG("IO_ins: io->Key=%d, Klist_length=%d\n",
       io->Key, a_Klist_length(ValidIOs));
}

/*
 * Remove an IO from ValidIOs
 */
static void IO_del(IOData_t *io)
{
   if (io->Key != 0) {
      a_Klist_remove(ValidIOs, io->Key);
   }
   io->Key = 0;
   _MSG(" -->ValidIOs: %d\n", a_Klist_length(ValidIOs));
}

/*
 * Return a io by its Key (NULL if not found)
 */
static IOData_t *IO_get(int Key)
{
   return (IOData_t *)a_Klist_get_data(ValidIOs, Key);
}

/*
 * Free an 'io' struct
 */
static void IO_free(IOData_t *io)
{
   dStr_free(io->Buf, 1);
   dFree(io);
}

/*
 * Close an open FD, and remove io controls.
 * (This function can be used for Close and Abort operations)
 */
static void IO_close_fd(IOData_t *io, int CloseCode)
{
   int st;

   /* With HTTP, if we close the writing part, the reading one also gets
    * closed! (other clients may set 'IOFlag_ForceClose') */
   if ((io->Flags & IOFlag_ForceClose) || (CloseCode == IO_StopRdWr)) {
      do
         st = close(io->FD);
      while (st < 0 && errno == EINTR);
   }
   /* Remove this IOData_t reference, from our ValidIOs list
    * We don't deallocate it here, just remove from the list.*/
   IO_del(io);

   /* Stop the polling on this FD */
   a_IOwatch_remove_fd(io->FD, io->events);
}

/*
 * Read data from a file descriptor into a specific buffer
 */
static bool_t IO_read(IOData_t *io)
{
   char Buf[IOBufLen];
   ssize_t St;
   bool_t ret = FALSE;
   int io_key = io->Key;

   DEBUG_MSG(3, "  IO_read\n");

   /* this is a new read-buffer */
   dStr_truncate(io->Buf, 0);
   io->Status = 0;

   while (1) {
      St = read(io->FD, Buf, IOBufLen);
      if (St > 0) {
         dStr_append_l(io->Buf, Buf, St);
         continue;
      } else if (St < 0) {
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN) {
            ret = TRUE;
            break;
         } else {
            io->Status = errno;
            break;
         }
      } else { /* St == 0 */
         break;
      }
   }

   if (io->Buf->len > 0) {
      /* send what we've got so far */
      a_IO_ccc(OpSend, 2, FWD, io->Info, io, NULL);
   }
   if (St == 0) {
      /* TODO: design a general way to avoid reentrancy problems with CCC. */

      /* The following check is necessary because the above CCC operation
       * may abort the whole Chain. */
      if ((io = IO_get(io_key))) {
         /* All data read (EOF) */
         a_IO_ccc(OpEnd, 2, FWD, io->Info, io, NULL);
      }
   }
   return ret;
}

/*
 * Write data, from a specific buffer, into a file descriptor
 */
static bool_t IO_write(IOData_t *io)
{
   ssize_t St;
   bool_t ret = FALSE;

   DEBUG_MSG(3, "  IO_write\n");
   io->Status = 0;

   while (1) {
      St = write(io->FD, io->Buf->str, io->Buf->len);
      if (St < 0) {
         /* Error */
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN) {
            ret = TRUE;
            break;
         } else {
            io->Status = errno;
            break;
         }
      } else if (St < io->Buf->len) {
         /* Not all data written */
         dStr_erase (io->Buf, 0, St);
      } else {
         /* All data in buffer written */
         dStr_truncate(io->Buf, 0);
         break;
      }
   }

   return ret;
}

/*
 * Handle background IO for a given FD (reads | writes)
 * (This function gets called when there's activity in the FD)
 */
static int IO_callback(int fd, IOData_t *io)
{
   bool_t ret = FALSE;

   _MSG("IO_callback:: (%s) FD = %d\n",
        (io->Op == IORead) ? "IORead" : "IOWrite", io->FD);

   if (io->Op == IORead) {          /* Read */
      ret = IO_read(io);
   } else if (io->Op == IOWrite) {  /* Write */
      ret = IO_write(io);
   }
   return (ret) ? 1 : 0;
}

/*
 * Handle the READ event of a FD.
 */
static void IO_fd_read_cb(int fd, void *data)
{
   int io_key = VOIDP2INT(data);
   IOData_t *io = IO_get(io_key);

   /* There should be no more events on already closed FDs  --Jcid */
   if (io == NULL) {
      MSG_ERR("IO_fd_read_cb: call on already closed io!\n");
      a_IOwatch_remove_fd(fd, DIO_READ);

   } else {
      if (IO_callback(fd, io) == 0)
         a_IOwatch_remove_fd(fd, DIO_READ);
   }
}

/*
 * Handle the WRITE event of a FD.
 */
static void IO_fd_write_cb(int fd, void *data)
{
   int io_key = VOIDP2INT(data);
   IOData_t *io = IO_get(io_key);

   if (io == NULL) {
      /* There must be no more events on already closed FDs  --Jcid */
      MSG_ERR("IO_fd_write_cb: call on already closed io!\n");
      a_IOwatch_remove_fd(fd, DIO_WRITE);

   } else {
      if (IO_callback(fd, io) == 0)
         a_IOwatch_remove_fd(fd, DIO_WRITE);
   }
}

/*
 * Receive an IO request (IORead | IOWrite),
 * Set a watch for it, and let it flow!
 */
static void IO_submit(IOData_t *r_io)
{
   /* Insert this IO in ValidIOs */
   IO_ins(r_io);

   _MSG("IO_submit:: (%s) FD = %d\n",
        (r_io->Op == IORead) ? "IORead" : "IOWrite", r_io->FD);

   /* Set FD to background and to close on exec. */
   fcntl(r_io->FD, F_SETFL, O_NONBLOCK | fcntl(r_io->FD, F_GETFL));
   fcntl(r_io->FD, F_SETFD, FD_CLOEXEC | fcntl(r_io->FD, F_GETFD));

   if (r_io->Op == IORead) {
      r_io->events = DIO_READ;
      a_IOwatch_add_fd(r_io->FD, r_io->events,
                       IO_fd_read_cb, (void*)(r_io->Key));

   } else if (r_io->Op == IOWrite) {
      r_io->events = DIO_WRITE;
      a_IOwatch_add_fd(r_io->FD, r_io->events,
                       IO_fd_write_cb, (void*)(r_io->Key));
   }
}

/*
 * CCC function for the IO module
 * ( Data1 = IOData_t* ; Data2 = NULL )
 */
void a_IO_ccc(int Op, int Branch, int Dir, ChainLink *Info,
              void *Data1, void *Data2)
{
   IOData_t *io;
   DataBuf *dbuf;

   a_Chain_debug_msg("a_IO_ccc", Op, Branch, Dir);

   if (Branch == 1) {
      if (Dir == BCK) {
         /* Write data using select */
         switch (Op) {
         case OpStart:
            io = IO_new(IOWrite, *(int*)Data1); /* SockFD */
            Info->LocalKey = io;
            break;
         case OpSend:
            io = Info->LocalKey;
            dbuf = Data1;
            dStr_append_l(io->Buf, dbuf->Buf, dbuf->Size);
            IO_submit(io);
            break;
         case OpEnd:
         case OpAbort:
            io = Info->LocalKey;
            if (io->Buf->len > 0) {
               MSG_WARN("IO_write, closing with pending data not sent\n");
               MSG_WARN(" \"%s\"\n", io->Buf->str);
            }
            /* close FD, remove from ValidIOs and remove its watch */
            IO_close_fd(io, IO_StopRdWr);
            IO_free(io);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* FWD */
         /* Write-data status */
         switch (Op) {
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }

   } else if (Branch == 2) {
      if (Dir == BCK) {
         /* This part catches the reader's messages */
         switch (Op) {
         case OpStart:
            io = IO_new(IORead, *(int*)Data2); /* SockFD */
            Info->LocalKey = io;
            io->Info = Info;
            IO_submit(io);
            break;
         case OpAbort:
            io = Info->LocalKey;
            IO_close_fd(io, IO_StopRdWr);
            IO_free(io);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* FWD */
         /* Send read-data */
         io = Data1;
         switch (Op) {
         case OpSend:
            dbuf = a_Chain_dbuf_new(io->Buf->str, io->Buf->len, 0);
            a_Chain_fcb(OpSend, Info, dbuf, NULL);
            dFree(dbuf);
            break;
         case OpEnd:
            a_Chain_fcb(OpEnd, Info, NULL, NULL);
            IO_close_fd(io, IO_StopRdWr);
            IO_free(io);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }
   }
}

