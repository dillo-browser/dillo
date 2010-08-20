/*
 * File: klist.c
 *
 * Copyright 2001-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * A simple ADT for Key-Data pairs
 *
 * NOTE: this ADT is not perfect. The possibility of holding a Key, after
 * its data has been removed, long enough for the key-counter to reset and
 * reuse the same key is very low, but EXISTS. So, the responsibility
 * remains with the caller.
 */

#include "klist.h"


/*
 * Compare function for searching data by its key
 */
static int Klist_node_by_key_cmp(const void *Node, const void *key)
{
   return ((KlistNode_t *)Node)->Key - VOIDP2INT(key);
}

/*
 * Compare function for searching data by node
 */
static int Klist_node_by_node_cmp(const void *Node1, const void *Node2)
{
   return ((KlistNode_t *)Node1)->Key - ((KlistNode_t *)Node2)->Key;
}

/*
 * Return the data pointer for a given Key (or NULL if not found)
 */
void *a_Klist_get_data(Klist_t *Klist, int Key)
{
   void *aux;

   if (!Klist)
      return NULL;
   aux = dList_find_sorted(Klist->List, INT2VOIDP(Key), Klist_node_by_key_cmp);
   return (aux) ? ((KlistNode_t*)aux)->Data : NULL;
}

/*
 * Insert a data pointer and return a key for it.
 */
int a_Klist_insert(Klist_t **Klist, void *Data)
{
   KlistNode_t *Node;

   if (!*Klist) {
      (*Klist) = dNew(Klist_t, 1);
      (*Klist)->List = dList_new(32);
      (*Klist)->Clean = 1;
      (*Klist)->Counter = 0;
   }

   /* This avoids repeated keys at the same time */
   do {
      if (++((*Klist)->Counter) == 0) {
         (*Klist)->Counter = 1;
         (*Klist)->Clean = 0;
      }
   } while (!((*Klist)->Clean) &&
            a_Klist_get_data((*Klist), (*Klist)->Counter));

   Node = dNew(KlistNode_t, 1);
   Node->Key  = (*Klist)->Counter;
   Node->Data = Data;
   dList_insert_sorted((*Klist)->List, Node, Klist_node_by_node_cmp);
   return (*Klist)->Counter;
}

/*
 * Remove data by Key
 */
void a_Klist_remove(Klist_t *Klist, int Key)
{
   void *data;

   data = dList_find_sorted(Klist->List, INT2VOIDP(Key),Klist_node_by_key_cmp);
   if (data) {
      dList_remove(Klist->List, data);
      dFree(data);
   }
   if (dList_length(Klist->List) == 0)
      Klist->Clean = 1;
}

/*
 * Return the number of elements in the Klist
 */
int a_Klist_length(Klist_t *Klist)
{
   return dList_length(Klist->List);
}

/*
 * Free a Klist
 */
void a_Klist_free(Klist_t **KlistPtr)
{
   void *node;
   Klist_t *Klist = *KlistPtr;

   if (!Klist)
      return;

   while (dList_length(Klist->List) > 0) {
      node = dList_nth_data(Klist->List, 0);
      dList_remove_fast(Klist->List, node);
      dFree(node);
   }
   dList_free(Klist->List);
   dFree(Klist);
   *KlistPtr = NULL;
}

