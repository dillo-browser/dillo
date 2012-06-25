#include "../dw/fltkcore.hh"
#include "../dw/hyphenator.hh"

using namespace lout::object;
using namespace lout::container::typed;

void hyphenateWord (dw::core::Platform *p, const char *word)
{
   dw::Hyphenator *h = dw::Hyphenator::getHyphenator (p, "de");

   Vector <String> *pieces = h->hyphenateWord (word);
   for (int i = 0; i < pieces->size (); i++) {
      if (i != 0)
         putchar ('-');
      printf ("%s", pieces->get(i)->chars ());
   }
   putchar ('\n');
   delete pieces;
}

int main (int argc, char *argv[])
{
   dw::fltk::FltkPlatform p;

   hyphenateWord (&p, "Jahrhundertroman");
   hyphenateWord (&p, "JAHRHUNDERTROMAN");
   hyphenateWord (&p, "währenddessen");
   hyphenateWord (&p, "Ückendorf");
   hyphenateWord (&p, "über");
   hyphenateWord (&p, "aber");
   hyphenateWord (&p, "Ackermann");

   return 0;
}
