#ifndef __CHAIN_H__
#define __CHAIN_H__

/*
 * Concomitant control chain (CCC)
 * Theory and code by Jorge Arellano Cid <jcid@dillo.org>
 */


/*
 * Supported CCC operations
 */
#define OpStart  1
#define OpSend   2
#define OpStop   3
#define OpEnd    4
#define OpAbort  5

/*
 * CCC flags
 */
#define CCC_Stopped     (1 << 0)
#define CCC_Ended       (1 << 1)
#define CCC_Aborted     (1 << 2)

/*
 * Linking direction
 */
#define FWD 1
#define BCK 2

typedef struct ChainLink ChainLink;
typedef void (*ChainFunction_t)(int Op, int Branch, int Dir, ChainLink *Info,
                                void *Data1, void *Data2);

/* This is the main data structure for CCC nodes */
struct ChainLink {
   void *LocalKey;

   int Flags;

   ChainLink *FcbInfo;
   ChainFunction_t Fcb;
   int FcbBranch;

   ChainLink *BcbInfo;
   ChainFunction_t Bcb;
   int BcbBranch;
};

/* A convenience data structure for passing data chunks between nodes */
typedef struct {
   char *Buf;
   int Size;
   int Code;
} DataBuf;



/*
 * Function prototypes
 */
ChainLink *a_Chain_new(void);
ChainLink *a_Chain_link_new(ChainLink *AInfo, ChainFunction_t AFunc,
                            int Direction, ChainFunction_t BFunc,
                            int AtoB_branch, int BtoA_branch);
void a_Chain_unlink(ChainLink *Info, int Direction);
int a_Chain_fcb(int Op, ChainLink *Info, void *Data1, void *Data2);
int a_Chain_bcb(int Op, ChainLink *Info, void *Data1, void *Data2);
int a_Chain_bfcb(int Op, ChainLink *Info, void *Data1, void *Data2);
int a_Chain_check(char *FuncStr, int Op, int Branch, int Dir,
                  ChainLink *Info);

DataBuf *a_Chain_dbuf_new(void *buf, int size, int code);

#endif /* __CHAIN_H__ */
