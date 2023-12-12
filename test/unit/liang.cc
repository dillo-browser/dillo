#include <unistd.h>

#include "../dw/fltkcore.hh"
#include "../dw/hyphenator.hh"

void hyphenateWord (dw::core::Platform *p, const char *lang, const char *word)
{
   dw::Hyphenator *h = dw::Hyphenator::getHyphenator (lang);

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
      // Usage: liang [-l LANG] WORD ...
      
      const char *lang = "de";
      char opt;
      
      while ((opt = getopt(argc, argv, "l:")) != -1) {
         switch (opt) {
         case 'l':
            lang = optarg;
            break;            
         }
      }

      for (int i = optind; i < argc; i++) 
         hyphenateWord (&p, lang, argv[i]);

   } else {
      hyphenateWord (&p, "de", "...");
      hyphenateWord (&p, "de", "Jahrhundertroman");
      hyphenateWord (&p, "de", "JAHRHUNDERTROMAN");
      hyphenateWord (&p, "de", "„Jahrhundertroman“");
      hyphenateWord (&p, "de", "währenddessen");
      hyphenateWord (&p, "de", "„währenddessen“");
      hyphenateWord (&p, "de", "Ückendorf");
      hyphenateWord (&p, "de", "über");
      hyphenateWord (&p, "de", "aber");
      hyphenateWord (&p, "de", "Ackermann");
      hyphenateWord (&p, "de", "„Ackermann“");
      hyphenateWord (&p, "de", "entscheidet.");
      hyphenateWord (&p, "de", "Grundstücksverkehrsgenehmigungszuständigkeits"
                     "übertragungsverordnung");
      hyphenateWord (&p, "de", "„Grundstücksverkehrsgenehmigungszuständigkeits"
                     "übertragungsverordnung“");
      hyphenateWord (&p, "de", "Grundstücksverkehrsgenehmigungszuständigkeit");
      hyphenateWord (&p, "de",
                     "„Grundstücksverkehrsgenehmigungszuständigkeit“");
      hyphenateWord (&p, "de",
                     "(6R,7R)-7-[2-(2-Amino-4-thiazolyl)-glyoxylamido]-3-"
                     "(2,5-dihydro-6-hydroxy-2-methyl-5-oxo-1,2,4-triazin-3-yl-"
                     "thiomethyl)-8-oxo-5-thia-1-azabicyclo[4.2.0]oct-2-en-2-"
                     "carbonsäure-7²-(Z)-(O-methyloxim)");
      hyphenateWord (&p, "de", "Abtei-Stadt");
      hyphenateWord (&p, "de", "Nordrhein-Westfalen");
      hyphenateWord (&p, "de", "kurz\xc2\xa0und\xc2\xa0knapp");
      hyphenateWord (&p, "de", "weiß");
      hyphenateWord (&p, "de", "www.dillo.org");
   }

   return 0;
}
