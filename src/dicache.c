/*
 * File: dicache.c
 *
 * Copyright 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string.h>         /* for memset */
#include <stdlib.h>

#include "msg.h"
#include "image.hh"
#include "imgbuf.hh"
#include "web.hh"
#include "dicache.h"
#include "dpng.h"
#include "dgif.h"
#include "djpeg.h"

typedef struct _DICacheNode DICacheNode;

enum {
   DIC_Gif,
   DIC_Png,
   DIC_Jpeg
};

struct _DICacheNode {
   int valid;            /* flag */
   DilloUrl *url;        /* primary "Key" for this dicache entry */
   DICacheEntry *first;  /* pointer to the first dicache entry in this list */
};

/*
 * List of DICacheNode. One node per URL. Each node may have several
 * versions of the same image in a linked list.
 */
static Dlist *CachedIMGs = NULL;

static uint_t dicache_size_total; /* invariant: dicache_size_total is
                                   * the sum of the image sizes (3*w*h)
                                   * of all the images in the dicache. */

/*
 * Compare two dicache nodes
 */
static int Dicache_node_cmp(const void *v1, const void *v2)
{
   const DICacheNode *n1 = v1, *n2 = v2;

   return a_Url_cmp(n1->url, n2->url);
}

/*
 * Compare function for searching a node by Url
 */
static int Dicache_node_by_url_cmp(const void *v1, const void *v2)
{
   const DICacheNode *node = v1;
   const DilloUrl *url = v2;

   return a_Url_cmp(node->url, url);
}

/*
 * Initialize dicache data
 */
void a_Dicache_init(void)
{
   CachedIMGs = dList_new(256);
   dicache_size_total = 0;
}

/*
 * Create, and initialize a new, empty, dicache entry
 */
static DICacheEntry *Dicache_entry_new(void)
{
   DICacheEntry *entry = dNew(DICacheEntry, 1);

   entry->width = 0;
   entry->height = 0;
   entry->type = DILLO_IMG_TYPE_NOTSET;
   entry->cmap = NULL;
   entry->v_imgbuf = NULL;
   entry->RefCount = 1;
   entry->TotalSize = 0;
   entry->ScanNumber = 0;
   entry->BitVec = NULL;
   entry->State = DIC_Empty;
   entry->version = 1;

   entry->Decoder = NULL;
   entry->DecoderData = NULL;
   entry->DecodedSize = 0;

   entry->next = NULL;

   return entry;
}

/*
 * Add a new entry in the dicache
 * (a single node (URL) may have several entries)
 */
static DICacheEntry *Dicache_add_entry(const DilloUrl *Url)
{
   DICacheEntry *entry;
   DICacheNode *node;

   entry = Dicache_entry_new();

   if ((node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp))) {
      /* this URL is already in CachedIMGs, add entry at the END of the list */
      DICacheEntry *ptr = node->first;

      node->valid = 1;
      for (  ; ptr->next; ptr = ptr->next);
      ptr->next = entry;
      entry->version = ptr->version+1;
      entry->url = node->url;

   } else { /* no node yet, so create one */
      DICacheNode *node = dNew(DICacheNode, 1);

      node->url = a_Url_dup(Url);
      entry->url = node->url;
      node->first = entry;
      node->valid = 1;
      dList_insert_sorted(CachedIMGs, node, Dicache_node_cmp);
   }

   return entry;
}

/*
 * Search a particular version of a URL in the Dicache.
 * Return value: a pointer to the entry if found; NULL otherwise.
 *
 * Notes: DIC_Last means last version of the image.
 *        version zero is not allowed.
 */
DICacheEntry *a_Dicache_get_entry(const DilloUrl *Url, int version)
{
   DICacheNode *node;
   DICacheEntry *entry = NULL;

   dReturn_val_if_fail(version != 0, NULL);

   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);
   if (node) {
      if (version == DIC_Last) {
         if (node->valid) {
            entry = node->first;
            for ( ;  (entry && entry->next); entry = entry->next);
         }
      } else {
         entry = node->first;
         for ( ; entry && entry->version != version; entry = entry->next) ;
      }
   }
   return entry;
}

/*
 * Actually free a dicache entry, given the URL and the version number.
 */
static void Dicache_remove(const DilloUrl *Url, int version)
{
   DICacheNode *node;
   DICacheEntry *entry, *prev;
   _MSG("Dicache_remove url=%s\n", URL_STR(Url));
   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);
   prev = entry = (node) ? node->first : NULL;

   while (entry && (entry->version != version) ) {
      prev = entry;
      entry = entry->next;
   }

   if (entry) {
      _MSG("Dicache_remove Decoder=%p DecoderData=%p\n",
          entry->Decoder, entry->DecoderData);
      /* Eliminate this dicache entry */
      dFree(entry->cmap);
      a_Bitvec_free(entry->BitVec);
      a_Imgbuf_unref(entry->v_imgbuf);
      if (entry->Decoder) {
         entry->Decoder(CA_Abort, entry->DecoderData);
      }
      dicache_size_total -= entry->TotalSize;

      if (node->first == entry) {
         if (!entry->next) {
            /* last entry with this URL. Remove the node as well */
            dList_remove(CachedIMGs, node);
            a_Url_free(node->url);
            dFree(node);
         } else
            node->first = entry->next;
      } else {
         prev->next = entry->next;
      }
      dFree(entry);
   }
}

/*
 * Unrefs the counter of a dicache entry, and _if_ no DwImage is acessing
 * this buffer, then we call Dicache_remove() to do the job.
 */
void a_Dicache_unref(const DilloUrl *Url, int version)
{
   DICacheEntry *entry;

   _MSG("a_Dicache_unref\n");
   if ((entry = a_Dicache_get_entry(Url, version))) {
      if (--entry->RefCount == 0) {
         Dicache_remove(Url, version);
      }
   }
}

/*
 * Refs the counter of a dicache entry.
 */
DICacheEntry* a_Dicache_ref(const DilloUrl *Url, int version)
{
   DICacheEntry *entry;

   if ((entry = a_Dicache_get_entry(Url, version))) {
      ++entry->RefCount;
   }
   return entry;
}

/*
 * Invalidate this entry. This is used for the reloading mechanism.
 * Can't erase current versions, but a_Dicache_get_entry(url, DIC_Last)
 * must return NULL.
 */
void a_Dicache_invalidate_entry(const DilloUrl *Url)
{
   DICacheNode *node;

   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);
   if (node)
      node->valid = 0;
}


/* ------------------------------------------------------------------------- */

/*
 * Set image's width, height & type
 * (By now, we'll use the image information despite the html tags --Jcid)
 */
void a_Dicache_set_parms(DilloUrl *url, int version, DilloImage *Image,
                         uint_t width, uint_t height, DilloImgType type)
{
   DICacheEntry *DicEntry;

   _MSG("a_Dicache_set_parms (%s)\n", URL_STR(url));
   dReturn_if_fail ( Image != NULL && width && height );
   /* Find the DicEntry for this Image */
   DicEntry = a_Dicache_get_entry(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   /* Parameters already set? */
   dReturn_if_fail ( DicEntry->State < DIC_SetParms );

   _MSG("  RefCount=%d version=%d\n", DicEntry->RefCount, DicEntry->version);

   /* BUG: there's just one image-type now */
   #define I_RGB 0
   DicEntry->v_imgbuf = a_Imgbuf_new(Image->dw, I_RGB, width, height);

   DicEntry->TotalSize = width * height * 3;
   DicEntry->width = width;
   DicEntry->height = height;
   DicEntry->type = type;
   DicEntry->BitVec = a_Bitvec_new((int)height);
   DicEntry->State = DIC_SetParms;

   dicache_size_total += DicEntry->TotalSize;
}

/*
 * Implement the set_cmap method for the Image
 */
void a_Dicache_set_cmap(DilloUrl *url, int version, DilloImage *Image,
                        const uchar_t *cmap, uint_t num_colors,
                        int num_colors_max, int bg_index)
{
   DICacheEntry *DicEntry = a_Dicache_get_entry(url, version);

   _MSG("a_Dicache_set_cmap\n");
   dReturn_if_fail ( DicEntry != NULL );

   dFree(DicEntry->cmap);
   DicEntry->cmap = dNew0(uchar_t, 3 * num_colors_max);
   memcpy(DicEntry->cmap, cmap, 3 * num_colors);
   if (bg_index >= 0 && (uint_t)bg_index < num_colors) {
      DicEntry->cmap[bg_index * 3]     = (Image->bg_color >> 16) & 0xff;
      DicEntry->cmap[bg_index * 3 + 1] = (Image->bg_color >> 8) & 0xff;
      DicEntry->cmap[bg_index * 3 + 2] = (Image->bg_color) & 0xff;
   }

   DicEntry->State = DIC_SetCmap;
}

/*
 * Reset for a new scan from a multiple-scan image.
 */
void a_Dicache_new_scan(const DilloUrl *url, int version)
{
   DICacheEntry *DicEntry;

   _MSG("a_Dicache_new_scan\n");
   dReturn_if_fail ( url != NULL );
   DicEntry = a_Dicache_get_entry(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   if (DicEntry->State < DIC_SetParms) {
      MSG("a_Dicache_new_scan before DIC_SetParms\n");
      exit(1);
   }
   a_Bitvec_clear(DicEntry->BitVec);
   DicEntry->ScanNumber++;
   a_Imgbuf_new_scan(DicEntry->v_imgbuf);
}

/*
 * Implement the write method
 * (Write a scan line into the Dicache entry)
 * buf: row buffer
 * Y  : row number
 */
void a_Dicache_write(DilloUrl *url, int version, const uchar_t *buf, uint_t Y)
{
   DICacheEntry *DicEntry;

   _MSG("a_Dicache_write\n");
   DicEntry = a_Dicache_get_entry(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   dReturn_if_fail ( DicEntry->width > 0 && DicEntry->height > 0 );

   /* update the common buffer in the imgbuf */
   a_Imgbuf_update(DicEntry->v_imgbuf, buf, DicEntry->type,
                   DicEntry->cmap, DicEntry->width, DicEntry->height, Y);

   a_Bitvec_set_bit(DicEntry->BitVec, (int)Y);
   DicEntry->State = DIC_Write;
}

/*
 * Implement the close method of the decoding process
 */
void a_Dicache_close(DilloUrl *url, int version, CacheClient_t *Client)
{
   DilloWeb *Web = Client->Web;
   DICacheEntry *DicEntry = a_Dicache_get_entry(url, version);

   dReturn_if_fail ( DicEntry != NULL );

   /* a_Dicache_unref() may free DicEntry */
   _MSG("a_Dicache_close RefCount=%d\n", DicEntry->RefCount - 1);

   if (DicEntry->State < DIC_Close) {
      DicEntry->State = DIC_Close;
      dFree(DicEntry->cmap);
      DicEntry->cmap = NULL;
      DicEntry->Decoder = NULL;
      DicEntry->DecoderData = NULL;
   }
   a_Dicache_unref(url, version);

   a_Bw_close_client(Web->bw, Client->Key);
}

/* ------------------------------------------------------------------------- */

/*
 * Generic MIME handler for GIF, JPEG and PNG.
 * Sets a_Dicache_callback as the cache-client,
 * and also sets the image decoder.
 *
 * Parameters:
 *   Type: MIME type
 *   Ptr:  points to a Web structure
 *   Call: Dillo calls this with more data/eod
 *   Data: Decoding data structure
 */
static void *Dicache_image(int ImgType, const char *MimeType, void *Ptr,
                           CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = Ptr;
   DICacheEntry *DicEntry;

   dReturn_val_if_fail(MimeType && Ptr, NULL);

   if (!web->Image) {
      web->Image = a_Image_new(NULL, web->bgColor);
      a_Image_ref(web->Image);
   }

   DicEntry = a_Dicache_get_entry(web->url, DIC_Last);
   if (!DicEntry) {
      /* Let's create an entry for this image... */
      DicEntry = Dicache_add_entry(web->url);
      DicEntry->DecoderData =
         (ImgType == DIC_Png) ?
            a_Png_new(web->Image, DicEntry->url, DicEntry->version) :
         (ImgType == DIC_Gif) ?
            a_Gif_new(web->Image, DicEntry->url, DicEntry->version) :
         (ImgType == DIC_Jpeg) ?
            a_Jpeg_new(web->Image, DicEntry->url, DicEntry->version) :
            NULL;
   } else {
      /* Repeated image */
      a_Dicache_ref(DicEntry->url, DicEntry->version);
   }
   DicEntry->Decoder = (ImgType == DIC_Png)  ? (CA_Callback_t)a_Png_callback :
                       (ImgType == DIC_Gif)  ? (CA_Callback_t)a_Gif_callback :
                       (ImgType == DIC_Jpeg) ? (CA_Callback_t)a_Jpeg_callback:
                                               NULL;
   *Data = DicEntry->DecoderData;
   *Call = (CA_Callback_t) a_Dicache_callback;

   return (web->Image->dw);
}

/*
 * PNG wrapper for Dicache_image()
 */
void *a_Dicache_png_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data)
{
   return Dicache_image(DIC_Png, Type, Ptr, Call, Data);
}

/*
 * GIF wrapper for Dicache_image()
 */
void *a_Dicache_gif_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data)
{
   return Dicache_image(DIC_Gif, Type, Ptr, Call, Data);
}

/*
 * JPEG wrapper for Dicache_image()
 */
void *a_Dicache_jpeg_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                           void **Data)
{
   return Dicache_image(DIC_Jpeg, Type, Ptr, Call, Data);
}

/*
 * This function is a cache client; (but feeds its clients from dicache)
 */
void a_Dicache_callback(int Op, CacheClient_t *Client)
{
   uint_t i;
   DilloWeb *Web = Client->Web;
   DilloImage *Image = Web->Image;
   DICacheEntry *DicEntry = a_Dicache_get_entry(Web->url, DIC_Last);

   dReturn_if_fail ( DicEntry != NULL );

   /* Copy the version number in the Client */
   if (Client->Version == 0)
      Client->Version = DicEntry->version;

   /* Only call the decoder when necessary */
   if (Op == CA_Send && DicEntry->State < DIC_Close &&
       DicEntry->DecodedSize < Client->BufSize) {
      DicEntry->Decoder(Op, Client);
      DicEntry->DecodedSize = Client->BufSize;
   } else if (Op == CA_Close || Op == CA_Abort) {
      if (DicEntry->State < DIC_Close) {
         DicEntry->Decoder(Op, Client);
      } else {
         a_Dicache_close(DicEntry->url, DicEntry->version, Client);
      }
   }

   /* when the data stream is not an image 'v_imgbuf' remains NULL */
   if (Op == CA_Send && DicEntry->v_imgbuf) {
      if (Image->height == 0 && DicEntry->State >= DIC_SetParms) {
         /* Set parms */
         a_Image_set_parms(
            Image, DicEntry->v_imgbuf, DicEntry->url,
            DicEntry->version, DicEntry->width, DicEntry->height,
            DicEntry->type);
      }
      if (DicEntry->State == DIC_Write) {
         if (DicEntry->ScanNumber == Image->ScanNumber) {
            for (i = 0; i < DicEntry->height; ++i)
               if (a_Bitvec_get_bit(DicEntry->BitVec, (int)i) &&
                   !a_Bitvec_get_bit(Image->BitVec, (int)i) )
                  a_Image_write(Image, i);
         } else {
            for (i = 0; i < DicEntry->height; ++i) {
               if (a_Bitvec_get_bit(DicEntry->BitVec, (int)i) ||
                   !a_Bitvec_get_bit(Image->BitVec, (int)i)   ||
                   DicEntry->ScanNumber > Image->ScanNumber + 1) {
                  a_Image_write(Image, i);
               }
               if (!a_Bitvec_get_bit(DicEntry->BitVec, (int)i))
                  a_Bitvec_clear_bit(Image->BitVec, (int)i);
            }
            Image->ScanNumber = DicEntry->ScanNumber;
         }
      }
   } else if (Op == CA_Close || Op == CA_Abort) {
      a_Image_close(Image);
      a_Bw_close_client(Web->bw, Client->Key);
   }
}

/* ------------------------------------------------------------------------- */

/*
 * Free the imgbuf (RGB data) of unused entries.
 */
void a_Dicache_cleanup(void)
{
   int i;
   DICacheNode *node;
   DICacheEntry *entry;

   _MSG("a_Dicache_cleanup\n");
   for (i = 0; i < dList_length(CachedIMGs); ++i) {
      node = dList_nth_data(CachedIMGs, i);
      /* iterate each entry of this node */
      for (entry = node->first; entry; entry = entry->next) {
         if (entry->v_imgbuf &&
             a_Imgbuf_last_reference(entry->v_imgbuf)) {
            /* free this unused entry */
            if (entry->next) {
               Dicache_remove(node->url, entry->version);
            } else {
               Dicache_remove(node->url, entry->version);
               --i;
               break;
            }
         }
      }
   }
}

/* ------------------------------------------------------------------------- */

/*
 * Deallocate memory used by dicache module
 * (Call this one at exit time)
 */
void a_Dicache_freeall(void)
{
   DICacheNode *node;
   DICacheEntry *entry;

   /* Remove every dicache node and its entries */
   while ((node = dList_nth_data(CachedIMGs, 0))) {
      while ((entry = node->first)) {
         node->first = entry->next;
         dFree(entry->cmap);
         a_Bitvec_free(entry->BitVec);
         a_Imgbuf_unref(entry->v_imgbuf);
         dicache_size_total -= entry->TotalSize;
      }
      dList_remove_fast(CachedIMGs, node);
      a_Url_free(node->url);
      dFree(node);
   }
   dList_free(CachedIMGs);
}
