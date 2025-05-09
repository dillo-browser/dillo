/*
 * Dillo Widget
 *
 * Copyright 2005-2013 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DW_PLATFORM_HH__
#define __DW_PLATFORM_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief An interface to encapsulate some platform dependencies.
 *
 * \sa\ref dw-overview
 */
class Platform: public lout::object::Object
{
public:
   /*
    * -----------------------------------
    *    General
    * -----------------------------------
    */

   /**
    * \brief This methods notifies the platform, that it has been attached to
    *    a layout.
    */
   virtual void setLayout (Layout *layout) = 0;

   /*
    * -------------------------
    *    Operations on views
    * -------------------------
    */

   /**
    * \brief This methods notifies the platform, that a view has been attached
    *    to the related layout.
    */
   virtual void attachView (View *view) = 0;

   /**
    * \brief This methods notifies the platform, that a view has been detached
    *    from the related layout.
    */
   virtual void detachView (View *view) = 0;

   /*
    * -----------------------------------
    *    Platform dependent properties
    * -----------------------------------
    */

   /**
    * \brief Return the width of a text, with a given length and font.
    */
   virtual int textWidth (style::Font *font, const char *text, int len) = 0;

   /**
    * \brief Return the string resulting from transforming text to uppercase.
    */
   virtual char *textToUpper (const char *text, int len) = 0;

   /**
    * \brief Return the string resulting from transforming text to lowercase.
    */
   virtual char *textToLower (const char *text, int len) = 0;

   /**
    * \brief Return the index of the next glyph in string text.
    */
   virtual int nextGlyph (const char *text, int idx) = 0;

   /**
    * \brief Return the index of the previous glyph in string text.
    */
   virtual int prevGlyph (const char *text, int idx) = 0;

   /**
    * \brief Return screen resolution in x-direction.
    */
   virtual float dpiX () = 0;

   /**
    * \brief Return screen resolution in y-direction.
    */
   virtual float dpiY () = 0;

   /*
    * ---------------------------------------------------------
    *    These are to encapsulate some platform dependencies
    * ---------------------------------------------------------
    */

   /**
    * \brief Add an idle function.
    *
    * An idle function is called once, when no other
    * tasks are to be done (e.g. there are no events to process), and then
    * removed from the queue. The return value is a number, which can be
    * used in removeIdle below.
    */
   virtual int addIdle (void (Layout::*func) ()) = 0;

   /**
    * \brief Remove an idle function, which has not been processed yet.
    */
   virtual void removeIdle (int idleId) = 0;

   /*
    * ---------------------
    *    Style Resources
    * ---------------------
    */

   /**
    * \brief Create a (platform dependent) font.
    *
    * Typically, within a platform, a sub class of dw::core::style::Font
    * is defined, which holds more platform dependent data.
    *
    * Also, this method must fill the attributes "font" (when needed),
    * "ascent", "descent", "spaceSidth", "zeroWidth" and "xHeight". If
    * "tryEverything" is true, several methods should be used to use
    * another font, when the requested font is not available. Passing
    * false is typically done, if the caller wants to test different
    * variations.
    */
   virtual style::Font *createFont (style::FontAttrs *attrs,
                                    bool tryEverything) = 0;

   virtual bool fontExists (const char *name) = 0;

   /**
    * \brief Create a color resource for a given 0xrrggbb value.
    */
   virtual style::Color *createColor (int color) = 0;

   /**
    * \brief Create a tooltip
    */
   virtual style::Tooltip *createTooltip (const char *text) = 0;

   /**
    * \brief Cancel a tooltip (either shown or requested)
    */
   virtual void cancelTooltip () = 0;

   /**
    * \brief Create a (platform specific) image buffer.
    *
    * "gamma" is the value by which the image data is gamma-encoded.
    */
   virtual Imgbuf *createImgbuf (Imgbuf::Type type, int width, int height,
                                 double gamma) = 0;

   /**
    * \brief Copy selected text (0-terminated).
    */
   virtual void copySelection(const char *text, int destination) = 0;

   /**
    * ...
    */
   virtual ui::ResourceFactory *getResourceFactory () = 0;
};

} // namespace core
} // namespace dw

#endif // __DW_PLATFORM_HH__
