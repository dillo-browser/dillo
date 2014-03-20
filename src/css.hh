#ifndef __CSS_HH__
#define __CSS_HH__

#include "dw/core.hh"
#include "doctree.hh"

/* Origin and weight. Used only internally.*/
typedef enum {
   CSS_PRIMARY_USER_AGENT,
   CSS_PRIMARY_USER,
   CSS_PRIMARY_AUTHOR,
   CSS_PRIMARY_AUTHOR_IMPORTANT,
   CSS_PRIMARY_USER_IMPORTANT,
   CSS_PRIMARY_LAST,
} CssPrimaryOrder;

typedef enum {
   CSS_ORIGIN_USER_AGENT,
   CSS_ORIGIN_USER,
   CSS_ORIGIN_AUTHOR,
} CssOrigin;

typedef enum {
   CSS_TYPE_INTEGER,            /* This type is only used internally, for x-*
                                   properties. */
   CSS_TYPE_ENUM,               /* Value is i, if represented by
                                   enum_symbols[i]. */
   CSS_TYPE_MULTI_ENUM,         /* For all enum_symbols[i], 1 << i are
                                   combined. */
   CSS_TYPE_LENGTH_PERCENTAGE,  /* <length> or <percentage>. Represented by
                                   CssLength. */
   CSS_TYPE_LENGTH,             /* <length>, represented as CssLength.
                                   Note: In some cases, CSS_TYPE_LENGTH is used
                                   instead of CSS_TYPE_LENGTH_PERCENTAGE,
                                   only because Dw cannot handle percentages
                                   in this particular case (e.g.
                                   'margin-*-width'). */
   CSS_TYPE_SIGNED_LENGTH,      /* As CSS_TYPE_LENGTH but may be negative. */
   CSS_TYPE_LENGTH_PERCENTAGE_NUMBER,  /* <length> or <percentage>, or <number> */
   CSS_TYPE_AUTO,               /* Represented as CssLength of type
                                   CSS_LENGTH_TYPE_AUTO */
   CSS_TYPE_COLOR,              /* Represented as integer. */
   CSS_TYPE_FONT_WEIGHT,        /* this very special and only used by
                                   'font-weight' */
   CSS_TYPE_STRING,             /* <string> */
   CSS_TYPE_SYMBOL,             /* Symbols, which are directly copied (as
                                   opposed to CSS_TYPE_ENUM and
                                   CSS_TYPE_MULTI_ENUM). Used for
                                   'font-family'. */
   CSS_TYPE_URI,                /* <uri> */
   CSS_TYPE_BACKGROUND_POSITION,
   CSS_TYPE_UNUSED              /* Not yet used. Will itself get unused some
                                   day. */
} CssValueType;

/*
 * Lengths are represented as int in the following way:
 *
 *    | <------   integer value   ------> |
 *
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *    |          integer part             |   type    |
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *    | integer part  | decimal fraction  |   type    |
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *     n-1          15  14              3   2  1   0
 *
 *    | <------ fixed point value ------> |
 *
 * where type is one of the CSS_LENGTH_TYPE_* values.
 * CSS_LENGTH_TYPE_PX values are stored as
 * 29 bit signed integer, all other types as fixed point values.
 */

typedef int CssLength;

typedef enum {
   CSS_LENGTH_TYPE_NONE,
   CSS_LENGTH_TYPE_PX,
   CSS_LENGTH_TYPE_MM,         /* "cm", "in", "pt" and "pc" are converted into
                                  millimeters. */
   CSS_LENGTH_TYPE_EM,
   CSS_LENGTH_TYPE_EX,
   CSS_LENGTH_TYPE_PERCENTAGE,
   CSS_LENGTH_TYPE_RELATIVE,   /* This does not exist in CSS but
                                  is used in HTML */
   CSS_LENGTH_TYPE_AUTO        /* This can be used as a simple value. */
} CssLengthType;

inline CssLength CSS_CREATE_LENGTH (float v, CssLengthType t) {
   static const int CSS_LENGTH_FRAC_MAX = (1 << (32 - 15 - 1)) - 1;
   static const int CSS_LENGTH_INT_MAX = (1 << (32 - 4)) - 1;
   int iv;

   switch (t) {
   case CSS_LENGTH_TYPE_PX:
      iv = lout::misc::roundInt(v);
      if (iv > CSS_LENGTH_INT_MAX)
         iv = CSS_LENGTH_INT_MAX;
      else if (iv < -CSS_LENGTH_INT_MAX)
         iv = -CSS_LENGTH_INT_MAX;
      return iv << 3 | t;
   case CSS_LENGTH_TYPE_NONE:
   case CSS_LENGTH_TYPE_MM:
   case CSS_LENGTH_TYPE_EM:
   case CSS_LENGTH_TYPE_EX:
   case CSS_LENGTH_TYPE_PERCENTAGE:
   case CSS_LENGTH_TYPE_RELATIVE:
      if (v > CSS_LENGTH_FRAC_MAX)
         v = CSS_LENGTH_FRAC_MAX;
      else if (v < -CSS_LENGTH_FRAC_MAX)
         v = -CSS_LENGTH_FRAC_MAX;
      return ((int) (v * (1 << 15)) & ~7 ) | t;
   case CSS_LENGTH_TYPE_AUTO:
      return t;
   default:
      assert(false);
      return CSS_LENGTH_TYPE_AUTO;
   }
}

inline CssLengthType CSS_LENGTH_TYPE (CssLength l) {
   return (CssLengthType) (l & 7);
}

inline float CSS_LENGTH_VALUE (CssLength l) {
   switch (CSS_LENGTH_TYPE(l)) {
   case CSS_LENGTH_TYPE_PX:
      return (float) (l >> 3);
   case CSS_LENGTH_TYPE_NONE:
   case CSS_LENGTH_TYPE_MM:
   case CSS_LENGTH_TYPE_EM:
   case CSS_LENGTH_TYPE_EX:
   case CSS_LENGTH_TYPE_PERCENTAGE:
   case CSS_LENGTH_TYPE_RELATIVE:
      return  ((float)(l & ~7)) / (1 << 15);
   case CSS_LENGTH_TYPE_AUTO:
      return 0.0;
   default:
      assert(false);
      return 0.0;
   }
}

typedef enum {
   CSS_PROPERTY_END = -1, // used as terminator in CssShorthandInfo
   CSS_PROPERTY_BACKGROUND_ATTACHMENT,
   CSS_PROPERTY_BACKGROUND_COLOR,
   CSS_PROPERTY_BACKGROUND_IMAGE,
   CSS_PROPERTY_BACKGROUND_POSITION,
   CSS_PROPERTY_BACKGROUND_REPEAT,
   CSS_PROPERTY_BORDER_BOTTOM_COLOR,
   CSS_PROPERTY_BORDER_BOTTOM_STYLE,
   CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
   CSS_PROPERTY_BORDER_COLLAPSE,
   CSS_PROPERTY_BORDER_LEFT_COLOR,
   CSS_PROPERTY_BORDER_LEFT_STYLE,
   CSS_PROPERTY_BORDER_LEFT_WIDTH,
   CSS_PROPERTY_BORDER_RIGHT_COLOR,
   CSS_PROPERTY_BORDER_RIGHT_STYLE,
   CSS_PROPERTY_BORDER_RIGHT_WIDTH,
   CSS_PROPERTY_BORDER_SPACING,
   CSS_PROPERTY_BORDER_TOP_COLOR,
   CSS_PROPERTY_BORDER_TOP_STYLE,
   CSS_PROPERTY_BORDER_TOP_WIDTH,
   CSS_PROPERTY_BOTTOM,
   CSS_PROPERTY_CAPTION_SIDE,
   CSS_PROPERTY_CLEAR,
   CSS_PROPERTY_CLIP,
   CSS_PROPERTY_COLOR,
   CSS_PROPERTY_CONTENT,
   CSS_PROPERTY_COUNTER_INCREMENT,
   CSS_PROPERTY_COUNTER_RESET,
   CSS_PROPERTY_CURSOR,
   CSS_PROPERTY_DIRECTION,
   CSS_PROPERTY_DISPLAY,
   CSS_PROPERTY_EMPTY_CELLS,
   CSS_PROPERTY_FLOAT,
   CSS_PROPERTY_FONT_FAMILY,
   CSS_PROPERTY_FONT_SIZE,
   CSS_PROPERTY_FONT_SIZE_ADJUST,
   CSS_PROPERTY_FONT_STRETCH,
   CSS_PROPERTY_FONT_STYLE,
   CSS_PROPERTY_FONT_VARIANT,
   CSS_PROPERTY_FONT_WEIGHT,
   CSS_PROPERTY_HEIGHT,
   CSS_PROPERTY_LEFT,
   CSS_PROPERTY_LETTER_SPACING,
   CSS_PROPERTY_LINE_HEIGHT,
   CSS_PROPERTY_LIST_STYLE_IMAGE,
   CSS_PROPERTY_LIST_STYLE_POSITION,
   CSS_PROPERTY_LIST_STYLE_TYPE,
   CSS_PROPERTY_MARGIN_BOTTOM,
   CSS_PROPERTY_MARGIN_LEFT,
   CSS_PROPERTY_MARGIN_RIGHT,
   CSS_PROPERTY_MARGIN_TOP,
   CSS_PROPERTY_MARKER_OFFSET,
   CSS_PROPERTY_MARKS,
   CSS_PROPERTY_MAX_HEIGHT,
   CSS_PROPERTY_MAX_WIDTH,
   CSS_PROPERTY_MIN_HEIGHT,
   CSS_PROPERTY_MIN_WIDTH,
   CSS_PROPERTY_OUTLINE_COLOR,
   CSS_PROPERTY_OUTLINE_STYLE,
   CSS_PROPERTY_OUTLINE_WIDTH,
   CSS_PROPERTY_OVERFLOW,
   CSS_PROPERTY_PADDING_BOTTOM,
   CSS_PROPERTY_PADDING_LEFT,
   CSS_PROPERTY_PADDING_RIGHT,
   CSS_PROPERTY_PADDING_TOP,
   CSS_PROPERTY_POSITION,
   CSS_PROPERTY_QUOTES,
   CSS_PROPERTY_RIGHT,
   CSS_PROPERTY_TEXT_ALIGN,
   CSS_PROPERTY_TEXT_DECORATION,
   CSS_PROPERTY_TEXT_INDENT,
   CSS_PROPERTY_TEXT_SHADOW,
   CSS_PROPERTY_TEXT_TRANSFORM,
   CSS_PROPERTY_TOP,
   CSS_PROPERTY_UNICODE_BIDI,
   CSS_PROPERTY_VERTICAL_ALIGN,
   CSS_PROPERTY_VISIBILITY,
   CSS_PROPERTY_WHITE_SPACE,
   CSS_PROPERTY_WIDTH,
   CSS_PROPERTY_WORD_SPACING,
   CSS_PROPERTY_Z_INDEX,
   CSS_PROPERTY_X_LINK,
   CSS_PROPERTY_X_COLSPAN,
   CSS_PROPERTY_X_ROWSPAN,
   PROPERTY_X_LINK,
   PROPERTY_X_LANG,
   PROPERTY_X_IMG,
   PROPERTY_X_TOOLTIP,
   CSS_PROPERTY_LAST
} CssPropertyName;

typedef struct {
   int32_t posX;
   int32_t posY;
} CssBackgroundPosition;

typedef union {
   int32_t intVal;
   char *strVal;
   CssBackgroundPosition *posVal;
} CssPropertyValue;

typedef enum {
   CSS_BORDER_WIDTH_THIN,
   CSS_BORDER_WIDTH_MEDIUM,
   CSS_BORDER_WIDTH_THICK,
} CssBorderWidthExtensions;

typedef enum {
   CSS_FONT_WEIGHT_BOLD,
   CSS_FONT_WEIGHT_BOLDER,
   CSS_FONT_WEIGHT_LIGHT,
   CSS_FONT_WEIGHT_LIGHTER,
   CSS_FONT_WEIGHT_NORMAL,
} CssFontWeightExtensions;

typedef enum {
   CSS_FONT_SIZE_LARGE,
   CSS_FONT_SIZE_LARGER,
   CSS_FONT_SIZE_MEDIUM,
   CSS_FONT_SIZE_SMALL,
   CSS_FONT_SIZE_SMALLER,
   CSS_FONT_SIZE_XX_LARGE,
   CSS_FONT_SIZE_XX_SMALL,
   CSS_FONT_SIZE_X_LARGE,
   CSS_FONT_SIZE_X_SMALL,
} CssFontSizeExtensions;

typedef enum {
   CSS_LETTER_SPACING_NORMAL
} CssLetterSpacingExtensions;

typedef enum {
   CSS_WORD_SPACING_NORMAL
} CssWordSpacingExtensions;


/**
 * \brief This class holds a CSS property and value pair.
 */
class CssProperty {
   public:

      short name;
      short type;
      CssPropertyValue value;

      inline void free () {
         switch (type) {
            case CSS_TYPE_STRING:
            case CSS_TYPE_SYMBOL:
            case CSS_TYPE_URI:
               dFree (value.strVal);
               break;
            case CSS_TYPE_BACKGROUND_POSITION:
               dFree (value.posVal);
            default:
               break;
         }
      }
      void print ();
};

/**
 * \brief A list of CssProperty objects.
 */
class CssPropertyList : public lout::misc::SimpleVector <CssProperty> {
   int refCount;
   bool ownerOfStrings;
   bool safe;

   public:
      inline CssPropertyList(bool ownerOfStrings = false) :
                  lout::misc::SimpleVector <CssProperty> (1) {
         refCount = 0;
         safe = true;
         this->ownerOfStrings = ownerOfStrings;
      };
      CssPropertyList(const CssPropertyList &p, bool deep = false);
      ~CssPropertyList ();

      void set (CssPropertyName name, CssValueType type,
                CssPropertyValue value);
      void apply (CssPropertyList *props);
      bool isSafe () { return safe; };
      void print ();
      inline void ref () { refCount++; }
      inline void unref () { if (--refCount == 0) delete this; }
};

class CssSimpleSelector {
   private:
      int element;
      char *pseudo, *id;
      lout::misc::SimpleVector <char *> klass;

   public:
      enum {
         ELEMENT_NONE = -1,
         ELEMENT_ANY = -2,
      };

      typedef enum {
         SELECT_NONE,
         SELECT_CLASS,
         SELECT_PSEUDO_CLASS,
         SELECT_ID,
      } SelectType;

      CssSimpleSelector ();
      ~CssSimpleSelector ();
      inline void setElement (int e) { element = e; };
      void setSelect (SelectType t, const char *v);
      inline lout::misc::SimpleVector <char *> *getClass () { return &klass; };
      inline const char *getPseudoClass () { return pseudo; };
      inline const char *getId () { return id; };
      inline int getElement () { return element; };
      bool match (const DoctreeNode *node);
      int specificity ();
      void print ();
};

class MatchCache : public lout::misc::SimpleVector <int> {
   public:
      MatchCache() : lout::misc::SimpleVector <int> (0) {};
};

/**
 * \brief CSS selector class.
 *
 * \todo Implement missing selector options.
 */
class CssSelector {
   public:
      typedef enum {
         COMB_NONE,
         COMB_DESCENDANT,
         COMB_CHILD,
         COMB_ADJACENT_SIBLING,
      } Combinator;

   private:
      struct CombinatorAndSelector {
         Combinator combinator;
         CssSimpleSelector *selector;
      };

      int refCount, matchCacheOffset;
      lout::misc::SimpleVector <struct CombinatorAndSelector> selectorList;

      bool match (Doctree *dt, const DoctreeNode *node, int i, Combinator comb,
                  MatchCache *matchCache);

   public:
      CssSelector ();
      ~CssSelector ();
      void addSimpleSelector (Combinator c);
      inline CssSimpleSelector *top () {
         return selectorList.getRef (selectorList.size () - 1)->selector;
      }
      inline int size () { return selectorList.size (); };
      inline bool match (Doctree *dt, const DoctreeNode *node,
                         MatchCache *matchCache) {
         return match (dt, node, selectorList.size () - 1, COMB_NONE,
                       matchCache);
      }
      inline void setMatchCacheOffset (int mo) {
         if (matchCacheOffset == -1)
            matchCacheOffset = mo;
      }
      inline int getRequiredMatchCache () {
         return matchCacheOffset + size ();
      }
      int specificity ();
      bool checksPseudoClass ();
      void print ();
      inline void ref () { refCount++; }
      inline void unref () { if (--refCount == 0) delete this; }
};

/**
 * \brief A CssSelector CssPropertyList pair.
 *
 *  The CssPropertyList is applied if the CssSelector matches.
 */
class CssRule {
   private:
      CssPropertyList *props;
      int spec, pos;

   public:
      CssSelector *selector;

      CssRule (CssSelector *selector, CssPropertyList *props, int pos);
      ~CssRule ();

      void apply (CssPropertyList *props, Doctree *docTree,
                  const DoctreeNode *node, MatchCache *matchCache) const;
      inline bool isSafe () {
         return !selector->checksPseudoClass () || props->isSafe ();
      };
      inline int specificity () { return spec; };
      inline int position () { return pos; };
      void print ();
};

/**
 * \brief A list of CssRules.
 *
 * In apply () all matching rules are applied.
 */
class CssStyleSheet {
   private:
      class RuleList : public lout::misc::SimpleVector <CssRule*>,
                       public lout::object::Object {
         public:
            RuleList () : lout::misc::SimpleVector <CssRule*> (1) {};
            ~RuleList () {
               for (int i = 0; i < size (); i++)
                  delete get (i);
            };

            void insert (CssRule *rule);
            inline bool equals (lout::object::Object *other) {
               return this == other;
            };
            inline int hashValue () { return (intptr_t) this; };
      };

      class RuleMap : public lout::container::typed::HashTable
                             <lout::object::ConstString, RuleList > {
         public:
            RuleMap () : lout::container::typed::HashTable
               <lout::object::ConstString, RuleList > (true, true, 256) {};
      };

      static const int ntags = 90 + 14; // \todo don't hardcode
      /* 90 is the full number of html4 elements, including those which we have
       * implemented. From html5, let's add: article, header, footer, mark,
       * nav, section, aside, figure, figcaption, wbr, audio, video, source,
       * embed.
       */

      RuleList elementTable[ntags], anyTable;
      RuleMap idTable, classTable;
      int requiredMatchCache;

   public:
      CssStyleSheet () { requiredMatchCache = 0; }
      void addRule (CssRule *rule);
      void apply (CssPropertyList *props, Doctree *docTree,
                  const DoctreeNode *node, MatchCache *matchCache) const;
      int getRequiredMatchCache () { return requiredMatchCache; }
};

/**
 * \brief A set of CssStyleSheets.
 */
class CssContext {
   private:
      static CssStyleSheet userAgentSheet;
      CssStyleSheet sheet[CSS_PRIMARY_USER_IMPORTANT + 1];
      MatchCache matchCache;
      int pos;

   public:
      CssContext ();

      void addRule (CssSelector *sel, CssPropertyList *props,
                    CssPrimaryOrder order);
      void apply (CssPropertyList *props,
         Doctree *docTree, DoctreeNode *node,
         CssPropertyList *tagStyle, CssPropertyList *tagStyleImportant,
         CssPropertyList *nonCssHints);
};

#endif
