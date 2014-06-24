#ifndef __DICACHE_H__
#define __DICACHE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "bitvec.h"
#include "image.hh"
#include "cache.h"

/* Symbolic name to request the last version of an image */
#define DIC_Last  -1
/* Flags: Last version, Valid entry */
#define DIF_Last  1
#define DIF_Valid 2


/* These will reflect the entry's "state" */
typedef enum {
   DIC_Empty,      /* Just created the entry */
   DIC_SetParms,   /* Parameters set */
   DIC_SetCmap,    /* Color map set */
   DIC_Write,      /* Feeding the entry */
   DIC_Close,      /* Whole image got! */
   DIC_Abort       /* Image transfer aborted */
} DicEntryState;

typedef struct DICacheEntry {
   DilloUrl *url;          /* Image URL for this entry */
   DilloImgType type;      /* Image type */
   uint_t width, height;   /* As taken from image data */
   short Flags;            /* See Flags */
   short SurvCleanup;      /* Cleanup-pass survival for unused images */
   uchar_t *cmap;          /* Color map */
   void *v_imgbuf;         /* Void pointer to an Imgbuf object */
   uint_t TotalSize;       /* Amount of memory the image takes up */
   uint_t ScanNumber;      /* Current decoding scan */
   bitvec_t *BitVec;       /* Bit vector for decoded rows */
   DicEntryState State;    /* Current status for this entry */
   int RefCount;           /* Reference Counter */
   int version;            /* Version number, used for different
                              versions of the same URL image */

   uint_t DecodedSize;     /* Size of already decoded data */
   CA_Callback_t Decoder;  /* Client function */
   void *DecoderData;      /* Client function data */
} DICacheEntry;


void a_Dicache_init (void);

DICacheEntry *a_Dicache_get_entry(const DilloUrl *Url, int version);

void *a_Dicache_png_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data);
void *a_Dicache_gif_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data);
void *a_Dicache_jpeg_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                           void **Data);
void a_Dicache_callback(int Op, CacheClient_t *Client);

void a_Dicache_set_parms(DilloUrl *url, int version, DilloImage *Image,
                         uint_t width, uint_t height, DilloImgType type,
                         double gamma);
void a_Dicache_set_cmap(DilloUrl *url, int version, int bg_color,
                        const uchar_t *cmap, uint_t num_colors,
                        int num_colors_max, int bg_index);
void a_Dicache_new_scan(const DilloUrl *url, int version);
void a_Dicache_write(DilloUrl *url, int version, const uchar_t *buf, uint_t Y);
void a_Dicache_close(DilloUrl *url, int version, CacheClient_t *Client);

void a_Dicache_invalidate_entry(const DilloUrl *Url);
DICacheEntry* a_Dicache_ref(const DilloUrl *Url, int version);
void a_Dicache_unref(const DilloUrl *Url, int version);
void a_Dicache_cleanup(void);
void a_Dicache_freeall(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __DICACHE_H__ */
