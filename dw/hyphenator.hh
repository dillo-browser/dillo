#ifndef __DW_HYPHENATOR_HH__
#define __DW_HYPHENATOR_HH__

#include "../lout/object.hh"
#include "../lout/container.hh"
#include "../dw/core.hh"

namespace dw {

class Hyphenator
{
private:
   /*
    * Actually, only one method in Platform is needed:
    * textToLower(). And, IMO, this method is actually not platform
    * independant, but based on UTF-8. Clarify? Change?
    */
   core::Platform *platform;
   lout::container::typed::HashTable <lout::object::Integer,
                                      lout::container::typed::Collection
                                      <lout::object::Integer> > *tree;
   void insertPattern (char *s);

public:
   Hyphenator (core::Platform *platform, const char *filename);
   
   lout::container::typed::Vector <lout::object::String>
   *hyphenateWord(const char *word);
};

} // namespace dw

#endif // __DW_HYPHENATOR_HH__
