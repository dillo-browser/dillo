
#include <zlib.h>
#include <iconv.h>
#include <errno.h>

#include "decode.h"
#include "msg.h"


static const int bufsize = 8*1024;

/*
 * null ("identity") decoding
 */
static Dstr *Decode_null(Decode *dc, Dstr *input)
{
   return input;
}

static void Decode_null_free(Decode *dc)
{
}

/*
 * Decode gzipped data
 */
static Dstr *Decode_gzip(Decode *dc, Dstr *input)
{
   int rc = Z_OK;

   z_stream *zs = (z_stream *)dc->state;

   int inputConsumed = 0;
   Dstr *output = dStr_new("");

   while ((rc == Z_OK) && (inputConsumed < input->len)) {
      zs->next_in = (Bytef *)input->str + inputConsumed;
      zs->avail_in = input->len - inputConsumed;

      zs->next_out = (Bytef *)dc->buffer;
      zs->avail_out = bufsize;

      rc = inflate(zs, Z_SYNC_FLUSH);

      if ((rc == Z_OK) || (rc == Z_STREAM_END)) {
         // Z_STREAM_END at end of file

         inputConsumed += zs->total_in;

         dStr_append_l(output, dc->buffer, zs->total_out);

         zs->total_out = 0;
         zs->total_in = 0;
      }
   }

   dStr_free(input, 1);
   return output;
}

static void Decode_gzip_free(Decode *dc)
{
   (void)inflateEnd((z_stream *)dc->state);

   dFree(dc->buffer);
}

/*
 * Translate to desired character set (UTF-8)
 */
static Dstr *Decode_charset(Decode *dc, Dstr *input)
{
   int rc = 0;

   Dstr *output;
   char *inPtr, *outPtr;
   size_t inLeft, outRoom;

   output = dStr_new("");

   dStr_append_l(dc->leftover, input->str, input->len);
   dStr_free(input, 1);
   input = dc->leftover;

   inPtr = input->str;
   inLeft = input->len;


   while ((rc != EINVAL) && (inLeft > 0)) {

      outPtr = dc->buffer;
      outRoom = bufsize;

      rc = iconv((iconv_t)dc->state, &inPtr, &inLeft, &outPtr, &outRoom);

      // iconv() on success, number of bytes converted
      //         -1, errno == EILSEQ illegal byte sequence found
      //                      EINVAL partial character ends source buffer
      //                      E2BIG  destination buffer is full

      dStr_append_l(output, dc->buffer, bufsize - outRoom);

      if (rc == -1)
         rc = errno;
      if (rc == EILSEQ){
         inPtr++;
         inLeft--;
         /*
          * U+FFFD: "used to replace an incoming character whose value is
          *        unknown or unrepresentable in Unicode."
          */
          //dStr_append(output, "\ufffd");
          // \uxxxx is C99. UTF-8-specific:
          dStr_append_c(output, 0xEF);
          dStr_append_c(output, 0xBF);
          dStr_append_c(output, 0xBD);
      }
   }

   dc->leftover = input;
   dStr_erase(dc->leftover, 0, dc->leftover->len - inLeft);

   return output;
}

static void Decode_charset_free(Decode *dc)
{
   (void)iconv_close((iconv_t)(dc->state));

   dFree(dc->buffer);
   dStr_free(dc->leftover, 1);
}

/*
 * Initialize content decoder. Currently handles gzip.
 *
 * zlib is also capable of handling "deflate"/zlib-encoded data, but web
 * servers have not standardized on whether to send such data with a header.
 */
Decode *a_Decode_content_init(const char *format)
{
   Decode *dc = (Decode *)dMalloc(sizeof(Decode));

   dc->buffer = NULL;
   dc->state = NULL;

   /* not used */
   dc->leftover = NULL;

   if (format && !dStrcasecmp(format, "gzip")) {

      MSG("compressed data! : %s\n", format);
      
      z_stream *zs;
      dc->buffer = (char *)dMalloc(bufsize);
      dc->state = zs = (z_stream *)dMalloc(sizeof(z_stream));
      zs->zalloc = NULL;
      zs->zfree = NULL;
      zs->next_in = NULL;
      zs->avail_in = 0;

      /* 16 is a magic number for gzip decoding */
      inflateInit2(zs, MAX_WBITS+16);

      dc->decode = Decode_gzip;
      dc->free = Decode_gzip_free;
   } else {
      dc->decode = Decode_null;
      dc->free = Decode_null_free;
   }
   return dc;      
}

/*
 * Legal names for the ASCII character set
 */
static int Decode_is_ascii(const char *str)
{
   return (!(dStrcasecmp(str, "ASCII") ||
             dStrcasecmp(str, "US-ASCII") ||
             dStrcasecmp(str, "us") ||
             dStrcasecmp(str, "IBM367") ||
             dStrcasecmp(str, "cp367") ||
             dStrcasecmp(str, "csASCII") ||
             dStrcasecmp(str, "ANSI_X3.4-1968") ||
             dStrcasecmp(str, "iso-ir-6") ||
             dStrcasecmp(str, "ANSI_X3.4-1986") ||
             dStrcasecmp(str, "ISO_646.irv:1991") ||
             dStrcasecmp(str, "ISO646-US")));
}

/*
 * Initialize decoder to translate from any character set known to iconv()
 * to UTF-8.
 *
 * GNU iconv(1) will provide a list of known character sets if invoked with
 * the "--list" flag.
 */
Decode *a_Decode_charset_init(const char *format)
{
   Decode *dc = (Decode *)dMalloc(sizeof(Decode));

   if (format &&
       strlen(format) &&
       dStrcasecmp(format,"UTF-8") &&
       !Decode_is_ascii(format)) {

      iconv_t ic;
      dc->state = ic = iconv_open("UTF-8", format);
      if (ic != (iconv_t) -1) {
           dc->buffer = (char *)dMalloc(bufsize);
           dc->leftover = dStr_new("");

           dc->decode = Decode_charset;
           dc->free = Decode_charset_free;
           return dc;
      } else {
         MSG("Unable to convert from character encoding: '%s'\n", format);
      }
   }
   dc->leftover = NULL;
   dc->buffer = NULL;
   dc->decode = Decode_null;
   dc->free = Decode_null_free;
   return dc;      
}

/*
 * Filter data using our decoder.
 *
 * The input string should not be used after this call. The decoder will
 * free it if necessary.
 */
Dstr *a_Decode_process(Decode *dc, Dstr *input)
{
   return dc->decode(dc, input);
}

/*
 * free our decoder
 */
void a_Decode_free(Decode *dc)
{
   dc->free(dc);
   dFree(dc);
}
