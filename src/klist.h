#ifndef __KLIST_H__
#define __KLIST_H__

#include "../dlib/dlib.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
   int Key;        /* primary key */
   void *Data;     /* data reference */
} KlistNode_t;

typedef struct {
   Dlist *List;
   int Clean;      /* check flag */
   int Counter;    /* counter (for making keys) */
} Klist_t;


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
