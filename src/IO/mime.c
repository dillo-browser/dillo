/*
 * File: mime.c
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "mime.h"
#include "../list.h"


typedef struct {
   const char *Name;   /* MIME type name */
   Viewer_t Data;      /* Pointer to a function */
} MimeItem_t;


/*
 *  Local data
 */
static int MimeMinItemsSize = 0, MimeMinItemsMax = 8;
static MimeItem_t *MimeMinItems = NULL;

static int MimeMajItemsSize = 0, MimeMajItemsMax = 8;
static MimeItem_t *MimeMajItems = NULL;


/*
 * Add a specific MIME type (as "image/png") to our viewer list
 * 'Key' is the content-type string that identifies the MIME type
 * 'Method' is the function that handles it
 */
static int Mime_add_minor_type(const char *Key, Viewer_t Method)
{
   a_List_add(MimeMinItems, MimeMinItemsSize, MimeMinItemsMax);
   MimeMinItems[MimeMinItemsSize].Name = Key;
   MimeMinItems[MimeMinItemsSize].Data = Method;
   MimeMinItemsSize++;
   return 0;
}

/*
 * Add a major MIME type (as "text") to our viewer list
 * 'Key' is the content-type string that identifies the MIME type
 * 'Method' is the function that handles it
 */
static int Mime_add_major_type(const char *Key, Viewer_t Method)
{
   a_List_add(MimeMajItems, MimeMajItemsSize, MimeMajItemsMax);
   MimeMajItems[MimeMajItemsSize].Name = Key;
   MimeMajItems[MimeMajItemsSize].Data = Method;
   MimeMajItemsSize++;
   return 0;
}

/*
 * Search the list of specific MIME viewers, for a Method that matches 'Key'
 * 'Key' is the content-type string that identifies the MIME type
 */
static Viewer_t Mime_minor_type_fetch(const char *Key, uint_t Size)
{
   int i;

   if (Size) {
      for ( i = 0; i < MimeMinItemsSize; ++i )
         if (dStrnAsciiCasecmp(Key, MimeMinItems[i].Name, Size) == 0)
            return MimeMinItems[i].Data;
   }
   return NULL;
}

/*
 * Search the list of major MIME viewers, for a Method that matches 'Key'
 * 'Key' is the content-type string that identifies the MIME type
 */
static Viewer_t Mime_major_type_fetch(const char *Key, uint_t Size)
{
   int i;

   if (Size) {
      for ( i = 0; i < MimeMajItemsSize; ++i )
         if (dStrnAsciiCasecmp(Key, MimeMajItems[i].Name, Size) == 0)
            return MimeMajItems[i].Data;
   }
   return NULL;
}


/*
 * Initializes Mime module and, sets the supported Mime types.
 */
void a_Mime_init()
{
#ifdef ENABLE_GIF
   Mime_add_minor_type("image/gif", a_Dicache_gif_image);
#endif
#ifdef ENABLE_JPEG
   Mime_add_minor_type("image/jpeg", a_Dicache_jpeg_image);
   Mime_add_minor_type("image/pjpeg", a_Dicache_jpeg_image);
   Mime_add_minor_type("image/jpg", a_Dicache_jpeg_image);
#endif
#ifdef ENABLE_PNG
   Mime_add_minor_type("image/png", a_Dicache_png_image);
   Mime_add_minor_type("image/x-png", a_Dicache_png_image);    /* deprecated */
#endif
   Mime_add_minor_type("text/html", a_Html_text);
   Mime_add_minor_type("application/xhtml+xml", a_Html_text);

   /* Add a major type to handle all the text stuff */
   Mime_add_major_type("text", a_Plain_text);
}


/*
 * Get the handler for the MIME type.
 *
 * Return Value:
 *   On success: viewer
 *   On failure: NULL
 */
Viewer_t a_Mime_get_viewer(const char *content_type)
{
   Viewer_t viewer;
   uint_t MinSize, MajSize, i;
   const char *str = content_type;

   MajSize = 0;
   for (i = 0; str[i] && str[i] != ' ' && str[i] != ';'; ++i) {
      if (str[i] == '/' && !MajSize)
         MajSize = i;
   }
   MinSize = i;

   viewer = Mime_minor_type_fetch(content_type, MinSize);
   if (!viewer)
      viewer = Mime_major_type_fetch(content_type, MajSize);

   return viewer;
}
