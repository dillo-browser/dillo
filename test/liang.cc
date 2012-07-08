#include "../dw/fltkcore.hh"
#include "../dw/hyphenator.hh"

void hyphenateWord (dw::core::Platform *p, const char *word)
{
   dw::Hyphenator *h = dw::Hyphenator::getHyphenator (p, "de");
   
   int numBreaks;
   int *breakPos = h->hyphenateWord (word, &numBreaks);
   for (int i = 0; i < numBreaks + 1; i++) {
      if (i != 0)
         printf ("\xc2\xad");
      int start = (i == 0 ? 0 : breakPos[i - 1]);
      int end = (i == numBreaks ? strlen (word) : breakPos[i]);
      for (int j = start; j < end; j++)
         putchar (word[j]);
   }
   putchar ('\n');
   if (breakPos)
      delete breakPos;
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
   hyphenateWord (&p, "Grundstücksverkehrsgenehmigungszuständigkeits"
                  "übertragungsverordnung");
   hyphenateWord (&p, "„Grundstücksverkehrsgenehmigungszuständigkeits"
                  "übertragungsverordnung“");
   hyphenateWord (&p, "Grundstücksverkehrsgenehmigungszuständigkeit");

   return 0;
}
