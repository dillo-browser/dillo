#include "../dw/fltkcore.hh"
#include "../dw/hyphenator.hh"

int main (int argc, char *argv[])
{
   dw::fltk::FltkPlatform p;
   
   if (argc < 2) {
      fprintf(stderr, "Usage: trie <pattern file>\n");
      exit (1);
   }

   /* Use pack = 1024 to create a really small trie - can take a while.
    */
   dw::Hyphenator hyphenator (&p, argv[1], NULL, 1024);
   hyphenator.saveTrie (stdout);
}
