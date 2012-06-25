#include "../dw/fltkcore.hh"
#include "../dw/hyphenator.hh"

using namespace lout::object;
using namespace lout::container::typed;

void hyphenateWord (dw::Hyphenator *h, const char *word)
{
   Vector <String> *pieces = h->hyphenateWord (word);
   for (int i = 0; i < pieces->size (); i++) {
      if (i != 0)
         putchar ('-');
      printf ("%s", pieces->get(i)->chars ());
   }
   putchar ('\n');
}

int main (int argc, char *argv[])
{
   dw::fltk::FltkPlatform p;
   dw::Hyphenator h (&p, "test/hyph-de-1996.pat");

   hyphenateWord (&h, "Jahrhundertroman");
   hyphenateWord (&h, "JAHRHUNDERTROMAN");
   hyphenateWord (&h, "währenddessen");
   hyphenateWord (&h, "Ückendorf");
   hyphenateWord (&h, "über");
   hyphenateWord (&h, "aber");
   hyphenateWord (&h, "Ackermann");

   return 0;
}
