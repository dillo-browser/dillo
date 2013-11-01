
#include "../dw/fltkcore.hh"
#include "../dw/hyphenator.hh"

void hyphenateWord (dw::core::Platform *p, const char *word)
{
   dw::Hyphenator *h = dw::Hyphenator::getHyphenator ("de");

   int numBreaks;
   int *breakPos = h->hyphenateWord (p, word, &numBreaks);
   for (int i = 0; i < numBreaks + 1; i++) {
      if (i != 0)
         printf (" \xc2\xad ");
      int start = (i == 0 ? 0 : breakPos[i - 1]);
      int end = (i == numBreaks ? strlen (word) : breakPos[i]);
      for (int j = start; j < end; j++)
         putchar (word[j]);
   }
   putchar ('\n');
   if (breakPos)
      free (breakPos);
}

int main (int argc, char *argv[])
{
   dw::fltk::FltkPlatform p;

   if (argc > 1) {
      hyphenateWord (&p, argv[1]);
   } else {
      hyphenateWord (&p, "...");
      hyphenateWord (&p, "Jahrhundertroman");
      hyphenateWord (&p, "JAHRHUNDERTROMAN");
      hyphenateWord (&p, "„Jahrhundertroman“");
      hyphenateWord (&p, "währenddessen");
      hyphenateWord (&p, "„währenddessen“");
      hyphenateWord (&p, "Ückendorf");
      hyphenateWord (&p, "über");
      hyphenateWord (&p, "aber");
      hyphenateWord (&p, "Ackermann");
      hyphenateWord (&p, "„Ackermann“");
      hyphenateWord (&p, "entscheidet.");
      hyphenateWord (&p, "Grundstücksverkehrsgenehmigungszuständigkeits"
                     "übertragungsverordnung");
      hyphenateWord (&p, "„Grundstücksverkehrsgenehmigungszuständigkeits"
                     "übertragungsverordnung“");
      hyphenateWord (&p, "Grundstücksverkehrsgenehmigungszuständigkeit");
      hyphenateWord (&p, "„Grundstücksverkehrsgenehmigungszuständigkeit“");
      hyphenateWord (&p, "(6R,7R)-7-[2-(2-Amino-4-thiazolyl)-glyoxylamido]-3-"
                     "(2,5-dihydro-6-hydroxy-2-methyl-5-oxo-1,2,4-triazin-3-yl-"
                     "thiomethyl)-8-oxo-5-thia-1-azabicyclo[4.2.0]oct-2-en-2-"
                     "carbonsäure-7²-(Z)-(O-methyloxim)");
      hyphenateWord (&p, "Abtei-Stadt");
      hyphenateWord (&p, "Nordrhein-Westfalen");
      hyphenateWord (&p, "kurz\xc2\xa0und\xc2\xa0knapp");
      hyphenateWord (&p, "weiß");
      hyphenateWord (&p, "www.dillo.org");
   }

   return 0;
}
