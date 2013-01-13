#ifndef __DW_FINDTEXT_STATE_H__
#define __DW_FINDTEXT_STATE_H__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

#include <ctype.h>

namespace dw {
namespace core {

class FindtextState
{
public:
   typedef enum {
      /** \brief The next occurrence of the pattern has been found. */
      SUCCESS,

      /**
       * \brief There is no further occurrence of the pattern, instead, the
       *    first occurrence has been selected.
       */
      RESTART,

      /** \brief The patten does not at all occur in the text. */
      NOT_FOUND
   } Result;

private:
   /**
    * \brief The key used for the last search.
    *
    * If dw::core::Findtext::search is called with the same key, the search
    * is continued, otherwise it is restarted.
    */
   char *key;

   /** \brief Whether the last search was case sensitive. */
   bool caseSens;

   /** \brief The table used for KMP search. */
   int *nexttab;

   /** \brief The top of the widget tree, in which the search is done.
    *
    * From this, the iterator will be constructed. Set by
    * dw::core::Findtext::widget
    */
   Widget *widget;

   /** \brief The position from where the next search will start. */
   CharIterator *iterator;

   /**
    * \brief The position from where the characters are highlighted.
    *
    * NULL, when no text is highlighted.
    */
   CharIterator *hlIterator;

   static const char* rev(const char* _str); /* reverse a C string */

   static int *createNexttab (const char *needle,bool caseSens,bool backwards);
   bool unhighlight ();
   bool search0 (bool backwards, bool firstTrial);

   inline static bool charsEqual (char c1, char c2, bool caseSens)
   { return caseSens ? c1 == c2 : tolower (c1) == tolower (c2) ||
      (isspace (c1) && isspace (c2)); }

public:
   FindtextState ();
   ~FindtextState ();

   void setWidget (Widget *widget);
   Result search (const char *key, bool caseSens, bool backwards);
   void resetSearch ();
};

} // namespace core
} // namespace dw

#endif // __DW_FINDTEXT_STATE_H__
