#ifndef __DW_HYPHENATOR_HH__
#define __DW_HYPHENATOR_HH__

#include "../lout/object.hh"
#include "../lout/container.hh"
#include "../dw/core.hh"

namespace dw {

class Hyphenator: public lout::object::Object
{
private:
   static lout::container::typed::HashTable
   <lout::object::TypedPair <lout::object::TypedPointer <core::Platform>,
                             lout::object::ConstString>,
    Hyphenator> *hyphenators;

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

   lout::container::typed::Vector <lout::object::String>
   *_hyphenateWord(const char *word);

public:
   Hyphenator (core::Platform *platform, const char *filename);
   ~Hyphenator();

   static Hyphenator *getHyphenator (core::Platform *platform,
                                     const char *language);
   static bool isHyphenationCandidate (const char *word);
   
   int *hyphenateWord(const char *word, int *numBreaks);
};

} // namespace dw

#endif // __DW_HYPHENATOR_HH__
