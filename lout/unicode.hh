#ifndef __UNICODE_HH__
#define __UNICODE_HH__

namespace lout {

/**
 * \brief Stuff dealing with Unicode characters: UTF-8, character classes etc.
 *
 */
namespace unicode {

bool isAlpha (int ch);

int decodeUtf8 (const char *s);

int decodeUtf8 (const char *s, int len);

const char *nextUtf8Char (const char *s);

const char *nextUtf8Char (const char *s, int len);

int numUtf8Chars (const char *s);

int numUtf8Chars (const char *s, int len);

} // namespace lout

} // namespace unicode

#endif // __UNICODE_HH__
