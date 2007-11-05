/*
 * File: chain.c
 * Concomitant control chain (CCC)
 * Theory and code by Jorge Arellano Cid
 *
 * Copyright 2001-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "msg.h"
#include "chain.h"
#include "../dlib/dlib.h"

#define VERBOSE 0

/*
 * Create and initialize a new chain-link
 */
ChainLink *a_Chain_new(void)
{
   return dNew0(ChainLink, 1);
}

/*
 * Create a new link from module A to module B.
 * 'Direction' tells whether to make a forward or backward link.
 * => The link from 'A' to 'B' has 'Direction' direction.
 * => The main flow of information names the FWD direction.
 * => AtoB_branch: branch on which 'B' receives communications from 'A'
 * => BtoA_branch: branch on which 'A' receives communications from 'B'
 */
ChainLink *a_Chain_link_new(ChainLink *AInfo, ChainFunction_t AFunc,
                            int Direction, ChainFunction_t BFunc,
                            int AtoB_branch, int BtoA_branch)
{
   ChainLink *NewLink = a_Chain_new();
   ChainLink *OldLink = AInfo;

   if (Direction == BCK) {
      NewLink->Fcb       = AFunc;
      NewLink->FcbInfo   = AInfo;
      NewLink->FcbBranch = BtoA_branch;
      OldLink->Bcb       = BFunc;
      OldLink->BcbInfo   = NewLink;
      OldLink->BcbBranch = AtoB_branch;

   } else { /* FWD */
      NewLink->Bcb       = AFunc;
      NewLink->BcbInfo   = AInfo;
      NewLink->BcbBranch = BtoA_branch;
      OldLink->Fcb       = BFunc;
      OldLink->FcbInfo   = NewLink;
      OldLink->FcbBranch = AtoB_branch;
   }

   return NewLink;
}

/*
 * Unlink a previously used link.
 * 'Direction' tells whether to unlink the forward or backward link.
 */
void a_Chain_unlink(ChainLink *Info, int Direction)
{
   if (Direction == FWD) {
      Info->Fcb = NULL;
      Info->FcbInfo = NULL;
      Info->FcbBranch = 0;
   } else {      /* BCK */
      Info->Bcb = NULL;
      Info->BcbInfo = NULL;
      Info->BcbBranch = 0;
   }
}

/*
 * Issue the forward callback of the 'Info' link
 */
int a_Chain_fcb(int Op, ChainLink *Info, void *Data1, void *Data2)
{
   if (Info->Fcb) {
      Info->Fcb(Op, Info->FcbBranch, FWD, Info->FcbInfo, Data1, Data2);
      return 1;
   }
   return 0;
}

/*
 * Issue the backward callback of the 'Info' link
 */
int a_Chain_bcb(int Op, ChainLink *Info, void *Data1, void *Data2)
{
   if (Info->Bcb) {
      Info->Bcb(Op, Info->BcbBranch, BCK, Info->BcbInfo, Data1, Data2);
      return 1;
   }
   return 0;
}


/*
 * Allocate and initialize a new DataBuf structure
 */
DataBuf *a_Chain_dbuf_new(void *buf, int size, int code)
{
   DataBuf *dbuf = dNew(DataBuf, 1);
   dbuf->Buf = buf;
   dbuf->Size = size;
   dbuf->Code = code;
   return dbuf;
}

/*
 * Show some debugging info
 */
void a_Chain_debug_msg(char *FuncStr, int Op, int Branch, int Dir)
{
#if VERBOSE
   const char *StrOps[] = {"", "OpStart", "OpSend",
                            "OpStop", "OpEnd", "OpAbort"};
   MSG("%-*s: %-*s [%d%s]\n",
       12, FuncStr, 7, StrOps[Op], Branch, (Dir == 1) ? "F" : "B");
#endif
}
