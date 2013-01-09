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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "../msg.h"
#include "../chain.h"
#include "../klist.h"
#include "IO.h"
#include "iowatch.hh"

/*
 * Symbolic defines for shutdown() function
 * (Not defined in the same header file, for all distros --Jcid)
 */
#define IO_StopRd   1
#define IO_StopWr   2
#define IO_StopRdWr (IO_StopRd | IO_StopWr)


typedef struct {
   int Key;               /* Primary Key (for klist) */
   int Op;                /* IORead | IOWrite */
   int FD;                /* Current File Descriptor */
   int Flags;             /* Flag array (look definitions above) */
   int Status;            /* errno code */
   Dstr *Buf;             /* Internal buffer */

   void *Info;            /* CCC Info structure for this IO */
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
 * Return a new, initialized, 'io' struct
 */
static IOData_t *IO_new(int op)
{
   IOData_t *io = dNew0(IOData_t, 1);
   io->Op = op;
   io->FD = -1;
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
 * BUG: there's a race condition for Abort. The file descriptor is closed
 * twice, and it could be reused for something else in between. It's simple
 * to fix, but it'd be better to design a canonical way to Abort the CCC.
 */
static void IO_close_fd(IOData_t *io, int CloseCode)
{
   int events = 0;

   _MSG("====> begin IO_close_fd (%d) Key=%d CloseCode=%d Flags=%d ",
       io->FD, io->Key, CloseCode, io->Flags);

   /* With HTTP, if we close the writing part, the reading one also gets
    * closed! (other clients may set 'IOFlag_ForceClose') */
   if (((io->Flags & IOFlag_ForceClose) || (CloseCode == IO_StopRdWr)) &&
       io->FD != -1) {
      dClose(io->FD);
   } else {
      _MSG(" NOT CLOSING ");
   }
   /* Remove this IOData_t reference, from our ValidIOs list
    * We don't deallocate it here, just remove from the list.*/
   IO_del(io);

   /* Stop the polling on this FD */
   if (CloseCode & IO_StopRd) {
     events |= DIO_READ;
   }
   if (CloseCode & IO_StopWr) {
     events |= DIO_WRITE;
   }
   a_IOwatch_remove_fd(io->FD, events);
   _MSG(" end IO close (%d) <=====\n", io->FD);
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

   _MSG("  IO_read\n");

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
         _MSG("IO_read: io->Key=%d io_key=%d\n", io->Key, io_key);
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

   _MSG("  IO_write\n");
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
static int IO_callback(IOData_t *io)
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
      if (IO_callback(io) == 0)
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
      if (IO_callback(io) == 0)
         a_IOwatch_remove_fd(fd, DIO_WRITE);
   }
}

/*
 * Receive an IO request (IORead | IOWrite),
 * Set a watch for it, and let it flow!
 */
static void IO_submit(IOData_t *r_io)
{
   if (r_io->FD < 0) {
      MSG_ERR("IO_submit: FD not initialized\n");
      return;
   }

   /* Insert this IO in ValidIOs */
   IO_ins(r_io);

   _MSG("IO_submit:: (%s) FD = %d\n",
        (r_io->Op == IORead) ? "IORead" : "IOWrite", r_io->FD);

   /* Set FD to background and to close on exec. */
   fcntl(r_io->FD, F_SETFL, O_NONBLOCK | fcntl(r_io->FD, F_GETFL));
   fcntl(r_io->FD, F_SETFD, FD_CLOEXEC | fcntl(r_io->FD, F_GETFD));

   if (r_io->Op == IORead) {
      a_IOwatch_add_fd(r_io->FD, DIO_READ,
                       IO_fd_read_cb, INT2VOIDP(r_io->Key));

   } else if (r_io->Op == IOWrite) {
      a_IOwatch_add_fd(r_io->FD, DIO_WRITE,
                       IO_fd_write_cb, INT2VOIDP(r_io->Key));
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

   dReturn_if_fail( a_Chain_check("a_IO_ccc", Op, Branch, Dir, Info) );

   if (Branch == 1) {
      if (Dir == BCK) {
         /* Write data using select */
         switch (Op) {
         case OpStart:
            io = IO_new(IOWrite);
            Info->LocalKey = io;
            break;
         case OpSend:
            io = Info->LocalKey;
            if (Data2 && !strcmp(Data2, "FD")) {
               io->FD = *(int*)Data1; /* SockFD */
            } else {
               dbuf = Data1;
               dStr_append_l(io->Buf, dbuf->Buf, dbuf->Size);
               IO_submit(io);
            }
            break;
         case OpEnd:
         case OpAbort:
            io = Info->LocalKey;
            if (io->Buf->len > 0) {
               char *newline = memchr(io->Buf->str, '\n', io->Buf->len);
               int msglen = newline ? newline - io->Buf->str : 2048;

               MSG_WARN("IO_write, closing with pending data not sent: "
                        "\"%s\"\n", dStr_printable(io->Buf, msglen));
            }
            /* close FD, remove from ValidIOs and remove its watch */
            IO_close_fd(io, Op == OpEnd ? IO_StopWr : IO_StopRdWr);
            IO_free(io);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* 1 FWD */
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
            io = IO_new(IORead);
            Info->LocalKey = io;
            io->Info = Info;
            break;
         case OpSend:
            io = Info->LocalKey;
            if (Data2 && !strcmp(Data2, "FD")) {
               io->FD = *(int*)Data1; /* SockFD */
               IO_submit(io);
            }
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
      } else {  /* 2 FWD */
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
            IO_close_fd(io, IO_StopRdWr); /* IO_StopRd would leak FDs */
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

