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
         int size;
      } Value;

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
         CSS_PROPERTY_TEXT_SHADOW,
         CSS_PROPERTY_TEXT_TRANSFORM,
         CSS_PROPERTY_TOP,
         CSS_PROPERTY_UNICODE_BIDI,
         CSS_PROPERTY_VERTICAL_ALIGN,
         CSS_PROPERTY_VISIBILITY,
         CSS_PROPERTY_WHITE_SPACE,
         CSS_PROPERTY_TEXT_INDENT,
         CSS_PROPERTY_WIDTH,
         CSS_PROPERTY_WORD_SPACING,
         CSS_PROPERTY_Z_INDEX,
         CSS_PROPERTY_X_LINK,
         CSS_PROPERTY_X_COLSPAN,
         CSS_PROPERTY_X_ROWSPAN,
         CSS_PROPERTY_LAST
      } Name;

      Name name;
      Value value;
};

class CssPropertyList : public lout::misc::SimpleVector <CssProperty> {
   public:
      CssPropertyList() : lout::misc::SimpleVector <CssProperty> (1) {};

      static CssPropertyList *parse (const char *buf);
      void set (CssProperty::Name name, CssProperty::Value value);
      void apply (CssPropertyList *props);
};

/** \todo proper implementation */
class CssSelector {
   private:
      int tag;
      const char *klass, *id;

   public:
      CssSelector (int tag, const char *klass, const char *id) {
         this->tag = tag;
         this->klass = klass;
         this->id = id;
      };

      static CssSelector *parse (const char *buf);

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

      void apply (CssPropertyList *props, Doctree *docTree);
};

class CssStyleSheet : public lout::misc::SimpleVector <CssRule*> {
   public:
      CssStyleSheet() : lout::misc::SimpleVector <CssRule*> (1) {};
      void apply (CssPropertyList *props, Doctree *docTree);
};

class CssContext {
   public:
      typedef enum {
         CSS_PRIMARY_USER_AGENT,
         CSS_PRIMARY_USER,
         CSS_PRIMARY_AUTHOR,
         CSS_PRIMARY_AUTHOR_IMPORTANT,
         CSS_PRIMARY_USER_IMPORTANT
      } PrimaryOrder;

   private:
      CssStyleSheet sheet[CSS_PRIMARY_USER_IMPORTANT + 1];

   public:
      void addRule (CssRule *rule, PrimaryOrder order);
      void apply (CssPropertyList *props,
         Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCss);
};

#endif
