#ifndef __UNICODE_HH__
#define __UNICODE_HH__

namespace lout {

/**
 * \brief Stuff dealing with Unicode characters: UTF-8, character classes etc.
 *
 */
namespace unicode {

bool isAlpha (int ch);

int decodeUtf8 (char *s);

} // namespace lout

} // namespace unicode

#endif // __UNICODE_HH__
