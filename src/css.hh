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
   CSS_TYPE_COLOR,              /* Represented as integer. */
   CSS_TYPE_FONT_WEIGHT,        /* this very special and only used by
                                   'font-weight' */
   CSS_TYPE_STRING,             /* <string> */
   CSS_TYPE_SYMBOL,             /* Symbols, which are directly copied (as
                                   opposed to CSS_TYPE_ENUM and 
                                   CSS_TYPE_MULTI_ENUM). Used for
                                   'font-family'. */
   CSS_TYPE_UNUSED              /* Not yet used. Will itself get unused some
                                   day. */
} CssValueType;

/*
 * Lengths are represented as int in the following way:
 * 
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *    | integer part  | decimal fraction  |   type    |
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *     n-1          19  18              3   2  1   0
 *
 *    | <------ fixed point value ------> |
 *
 * where type is one of the CSS_LENGTH_TYPE_* values.
 */

typedef int CssLength;

enum {
   CSS_LENGTH_TYPE_PX,
   CSS_LENGTH_TYPE_MM,         /* "cm", "in", "pt" and "pc" are converted into
                                  millimeters. */
   CSS_LENGTH_TYPE_EM,
   CSS_LENGTH_TYPE_EX,
   CSS_LENGTH_TYPE_PERCENTAGE,
   CSS_LENGTH_TYPE_RELATIVE,   /* This does not exist in CSS but
                                  is used in HTML */
   CSS_LENGTH_TYPE_AUTO        /* This can be used as a simple value. */
};

#define CSS_CREATE_LENGTH(v, t) ( ( (int)((v) * (1 << 19)) & ~7 ) | (t) )
#define CSS_LENGTH_VALUE(l)     ( ( (float)((l) & ~7) ) / (1 << 19) )
#define CSS_LENGTH_TYPE(l)      ((l) & 7)

typedef enum {
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
   PROPERTY_X_IMG,
   PROPERTY_X_TOOLTIP,
   CSS_PROPERTY_LAST
} CssPropertyName;

typedef union {
   int intVal;
   char *strVal;
} CssPropertyValue;

typedef enum {
   CSS_FONT_WEIGHT_LIGHTER = -1,
   CSS_FONT_WEIGHT_BOLDER = -2,
   CSS_FONT_WEIGHT_STEP = 300,
   /* Some special font weights. */
   CSS_FONT_WEIGHT_LIGHT = 100,
   CSS_FONT_WEIGHT_NORMAL = 400,
   CSS_FONT_WEIGHT_BOLD = 700,
   CSS_FONT_WEIGHT_MIN = 100,
   CSS_FONT_WEIGHT_MAX = 900,
} CssFontWeightExtensions;

/**
 * \brief This class holds a CSS property and value pair.
 */
class CssProperty {
   public:

      short name;
      short type;
      CssPropertyValue value;

      inline void free () {
         switch (name) {
            case CSS_PROPERTY_CONTENT:
            case CSS_PROPERTY_FONT_FAMILY:
               dFree (value.strVal);
               break;
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

   public:
      inline CssPropertyList(bool ownerOfStrings = false) :
                  lout::misc::SimpleVector <CssProperty> (1) {
         refCount = 0;
         this->ownerOfStrings = ownerOfStrings;
      };
      inline CssPropertyList(const CssPropertyList &p) :
         lout::misc::SimpleVector <CssProperty> (p) {
         refCount = 0;
         ownerOfStrings = false;
      };
      ~CssPropertyList ();

      void set (CssPropertyName name, CssValueType type,
                CssPropertyValue value);
      inline void set (CssPropertyName name, CssValueType type, char *value) {
         CssPropertyValue v;
         v.strVal = value;
         set (name, type, v);
      };
      inline void set (CssPropertyName name, CssValueType type, int value) {
         CssPropertyValue v;
         v.intVal = value;
         set (name, type, v);
      };
      void apply (CssPropertyList *props);
      void print ();
      inline void ref () { refCount++; }
      inline void unref () { if(--refCount == 0) delete this; }
};

class CssSimpleSelector {
   public:
      enum {
         ELEMENT_NONE = -1,
         ELEMENT_ANY = -2,
      };

      int element;
      char *klass, *pseudo, *id;

      CssSimpleSelector ();
      ~CssSimpleSelector ();
      bool match (const DoctreeNode *node);
      int specificity ();
      void print ();
};

/**
 * \brief CSS selector class.
 * \todo Implement missing selector options.
 */
class CssSelector {
   public:
      typedef enum {
         DESCENDANT,
         CHILD,
         ADJACENT_SIBLING,
      } Combinator;

   private:
      struct CombinatorAndSelector {
         int notMatchingBefore; // used for optimizing CSS selector matching
         Combinator combinator;
         CssSimpleSelector *selector;
      };

      int refCount;
      lout::misc::SimpleVector <struct CombinatorAndSelector> *selectorList;

   public:
      CssSelector ();
      ~CssSelector ();
      void addSimpleSelector (Combinator c);
      inline CssSimpleSelector *top () {
         return selectorList->getRef (selectorList->size () - 1)->selector;
      };
      inline int size () { return selectorList->size (); };
      bool match (Doctree *dt, const DoctreeNode *node);
      int specificity ();
      void print ();
      inline void ref () { refCount++; }
      inline void unref () { if(--refCount == 0) delete this; }
};

/**
 * \brief A CssSelector CssPropertyList pair.
 *  The CssPropertyList is applied if the CssSelector matches.
 */
class CssRule {
   private:
      CssPropertyList *props;
      int spec;

   public:
      CssSelector *selector;

      CssRule (CssSelector *selector, CssPropertyList *props);
      ~CssRule ();

      void apply (CssPropertyList *props,
                  Doctree *docTree, const DoctreeNode *node);
      inline int specificity () { return spec; };
      void print ();
};

/**
 * \brief A list of CssRules.
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
            inline bool equals (lout::object::Object *other) { return this == other; };
            inline int hashValue () { return (intptr_t) this; };
      };

      class RuleMap : public lout::container::typed::HashTable
                             <lout::object::ConstString, RuleList > {
         public:
            RuleMap () : lout::container::typed::HashTable
                    <lout::object::ConstString, RuleList > (true, true, 256) {};
      };

      static const int ntags = 90; // \todo replace 90
      RuleList *elementTable[ntags];

      RuleMap *idTable;
      RuleMap *classTable;
      RuleList *anyTable;

   public:
      CssStyleSheet();
      ~CssStyleSheet();
      void addRule (CssRule *rule);
      void apply (CssPropertyList *props,
                  Doctree *docTree, const DoctreeNode *node);
};

/**
 * \brief A set of CssStyleSheets.
 */
class CssContext {
   private:
      static CssStyleSheet *userAgentStyle;
      static CssStyleSheet *userStyle;
      static CssStyleSheet *userImportantStyle;
      CssStyleSheet *sheet[CSS_PRIMARY_USER_IMPORTANT + 1];

      void buildUserAgentStyle ();
      void buildUserStyle ();

   public:
      CssContext ();
      ~CssContext ();

      void addRule (CssSelector *sel, CssPropertyList *props,
                    CssPrimaryOrder order);
      void apply (CssPropertyList *props,
         Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCssHints);
};

#endif
