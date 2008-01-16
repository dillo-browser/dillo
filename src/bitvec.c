/*
 * File: bitvec.c
 *
 * Copyright 2001 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * A simple ADT for bit arrays
 */

#include "../dlib/dlib.h"
#include "bitvec.h"


/*
 * Create a new bitvec with 'num_bits' size
 */
bitvec_t *a_Bitvec_new(int num_bits)
{
   bitvec_t *bvec = dNew(bitvec_t, 1);

   bvec->vec = dNew0(uchar_t, num_bits/BVEC_SIZE + 1);
   bvec->len = num_bits;
   return bvec;
}

/*
 * Clear a bitvec
 */
void a_Bitvec_clear(bitvec_t *bvec)
{
   memset(bvec->vec, 0, sizeof(uchar_t) * bvec->len/BVEC_SIZE + 1);
}

/*
 * Free a bitvec
 */
void a_Bitvec_free(bitvec_t *bvec)
{
   if (bvec) {
      dFree(bvec->vec);
      dFree(bvec);
   }
}

/*
 * Get a bit
 */
int a_Bitvec_get_bit(bitvec_t *bvec, int pos)
{
   dReturn_val_if_fail (pos < bvec->len, 0);
   return (bvec->vec[pos/BVEC_SIZE] & 1 << pos % BVEC_SIZE);
}

/*
 * Set a bit
 */
void a_Bitvec_set_bit(bitvec_t *bvec, int pos)
{
   dReturn_if_fail (pos < bvec->len);
   bvec->vec[pos/BVEC_SIZE] |= 1 << (pos % BVEC_SIZE);
}
