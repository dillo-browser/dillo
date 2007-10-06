#ifndef __KLIST_H__
#define __KLIST_H__

#include "../dlib/dlib.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _KlistNode KlistNode_t;
typedef struct _Klist Klist_t;

struct _KlistNode {
   int Key;        /* primary key */
   void *Data;     /* data reference */
};

struct _Klist {
   Dlist *List;
   int Clean;      /* check flag */
   int Counter;    /* counter (for making keys) */
};


/*
 * Function prototypes
 */
void*     a_Klist_get_data(Klist_t *Klist, int Key);
int       a_Klist_insert(Klist_t **Klist, void *Data);
void      a_Klist_remove(Klist_t *Klist, int Key);
int       a_Klist_length(Klist_t *Klist);
void      a_Klist_free(Klist_t **Klist);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __KLIST_H__ */
