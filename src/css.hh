#ifndef __CSS_HH__
#define __CSS_HH__

#include "dw/core.hh"

#include "doctree.hh"

class CssProperty {
   public:
      typedef union {
         int color;
         dw::core::style::Length length;
         dw::core::style::Cursor cursor;
         dw::core::style::BorderStyle borderStyle;
         dw::core::style::TextAlignType textAlignType;
         dw::core::style::VAlignType valignType;
         dw::core::style::DisplayType displayType;
         dw::core::style::ListStyleType listStyleType;
         dw::core::style::FontStyle fontStyle;
         dw::core::style::TextDecoration textDecoration;
         dw::core::style::WhiteSpace whiteSpace;
         const char *name; /* used for font family */
      } Value;

      typedef enum {
         BACKGROUND_ATTACHMENT,
         BACKGROUND_COLOR,
         BACKGROUND_IMAGE,
         BACKGROUND_POSITION,
         BACKGROUND_REPEAT,
         BORDER_BOTTOM_COLOR,
         BORDER_BOTTOM_STYLE,
         BORDER_BOTTOM_WIDTH,
         BORDER_COLLAPSE,
         BORDER_LEFT_COLOR,
         BORDER_LEFT_STYLE,
         BORDER_LEFT_WIDTH,
         BORDER_RIGHT_COLOR,
         BORDER_RIGHT_STYLE,
         BORDER_RIGHT_WIDTH,
         BORDER_SPACING,
         BORDER_TOP_COLOR,
         BORDER_TOP_STYLE,
         BORDER_TOP_WIDTH,
         BOTTOM,
         CAPTION_SIDE,
         CLEAR,
         CLIP,
         COLOR,
         CONTENT,
         COUNTER_INCREMENT,
         COUNTER_RESET,
         CURSOR,
         DIRECTION,
         DISPLAY,
         EMPTY_CELLS,
         FLOAT,
         FONT_FAMILY,
         FONT_SIZE,
         FONT_SIZE_ADJUST,
         FONT_STRETCH,
         FONT_STYLE,
         FONT_VARIANT,
         FONT_WEIGHT,
         HEIGHT,
         LEFT,
         LETTER_SPACING,
         LINE_HEIGHT,
         LIST_STYLE_IMAGE,
         LIST_STYLE_POSITION,
         LIST_STYLE_TYPE,
         MARGIN_BOTTOM,
         MARGIN_LEFT,
         MARGIN_RIGHT,
         MARGIN_TOP,
         MARKER_OFFSET,
         MARKS,
         MAX_HEIGHT,
         MAX_WIDTH,
         MIN_HEIGHT,
         MIN_WIDTH,
         OUTLINE_COLOR,
         OUTLINE_STYLE,
         OUTLINE_WIDTH,
         OVERFLOW,
         PADDING_BOTTOM,
         PADDING_LEFT,
         PADDING_RIGHT,
         PADDING_TOP,
         POSITION,
         QUOTES,
         RIGHT,
         TEXT_ALIGN,
         TEXT_DECORATION,
         TEXT_SHADOW,
         TEXT_TRANSFORM,
         TOP,
         UNICODE_BIDI,
         VERTICAL_ALIGN,
         VISIBILITY,
         WHITE_SPACE,
         TEXT_INDENT,
         WIDTH,
         WORD_SPACING,
         Z_INDEX,
         X_LINK,
         X_COLSPAN,
         X_ROWSPAN,
         LAST
      } Name;

      Name name;
      Value value;

      CssProperty ();
      ~CssProperty ();

      void apply (dw::core::style::StyleAttrs *styleAttr);
};

class CssPropertyList : public lout::misc::SimpleVector <CssProperty*> {
   public:
      CssPropertyList() : lout::misc::SimpleVector <CssProperty*> (1) {};
      void apply (dw::core::style::StyleAttrs *styleAttr);
};

/** \todo proper implementation */
class CssSelector {
   private:
      int tagIndex;
      const char *klass, *id;

   public:
      CssSelector (int tagIndex, const char *klass, const char *id) {
         this->tagIndex = tagIndex;
         this->klass = klass;
         this->id = id;
      }; 

      bool match (Doctree *dt);
};

class CssRule {
   private:
      CssSelector *selector;
      CssPropertyList *props;

   public:
      CssRule (CssSelector *selector, CssPropertyList *props) {
         this->selector = selector;
         this->props = props;
      };
      ~CssRule ();

      void apply (dw::core::style::StyleAttrs *styleAttr);
};

class CssStyleSheet : public lout::misc::SimpleVector <CssRule*> {
   public:
      CssStyleSheet() : lout::misc::SimpleVector <CssRule*> (1) {};
      void apply (dw::core::style::StyleAttrs *styleAttr);
};

typedef enum {
   CSS_PRIMARY_USER_IMPORTANT,
   CSS_PRIMARY_AUTHOR_IMPORTANT,
   CSS_PRIMARY_AUTHOR,
   CSS_PRIMARY_USER,
   CSS_PRIMARY_USER_AGENT
} CssPrimaryOrder;

class CssContext {
   public:
      typedef enum {
         USER_IMPORTANT,
         AUTHOR_IMPORTANT,
         AUTHOR,
         USER,
         USER_AGENT
      } PrimaryOrder;

   private:
      CssStyleSheet sheet[USER_AGENT + 1];

   public:
      void addRule (CssRule *rule, PrimaryOrder order) {
         sheet[order].increase ();
         sheet[order].set (sheet[order].size () - 1, rule);
      };

      void apply (dw::core::style::StyleAttrs *styleAttr);
};

#endif
