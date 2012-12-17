#ifndef __DW_HYPHENATOR_HH__
#define __DW_HYPHENATOR_HH__

#include "../lout/object.hh"
#include "../lout/container.hh"
#include "../dw/core.hh"

namespace dw {

class Trie {
   public:
      struct TrieNode {
         unsigned char c;
         uint16_t next;
         const char *data;
      };

   private:
      TrieNode *array;
      int size;
      bool freeArray;
      lout::misc::ZoneAllocator *dataZone;

   public:
      Trie (TrieNode *array = NULL, int size = 0, bool freeArray = false,
            lout::misc::ZoneAllocator *dataZone = NULL);
      ~Trie ();

      static const int root = 0;
      inline bool validState (int state) { return state >= 0 && state < size; };
      inline const char *getData (unsigned char c, int *state)
      {
         if (!validState (*state))
            return NULL;

         TrieNode *tn = array + *state + c;

         if (tn->c == c) {
            *state = tn->next > 0 ? tn->next : -1;
            return tn->data;
         } else {
            *state = -1;
            return NULL;
         }
      };
      void save (FILE *file);
      int load (FILE *file);
};

class TrieBuilder {
   private:
      struct StackEntry {
         unsigned char c;
         int count;
         int next[256];
         const char *data[256];
         const char *data1;
      };

      struct DataEntry {
         unsigned char *key;
         const char *value;
      };

      int pack;
      static Trie::TrieNode trieNodeNull;
      lout::misc::SimpleVector <Trie::TrieNode> *tree;
      lout::misc::SimpleVector <DataEntry> *dataList;
      lout::misc::SimpleVector <StackEntry> *stateStack;
      lout::misc::ZoneAllocator *dataZone;

      static int keyCompare (const void *p1, const void *p2);
      void stateStackPush (unsigned char c);
      int stateStackPop ();
      int insertState (StackEntry *state, bool root);
      void insertSorted (unsigned char *key, const char *value);

   public:
      TrieBuilder (int pack);
      ~TrieBuilder ();

      void insert (const char *key, const char *value);
      Trie *createTrie();
};

class Hyphenator: public lout::object::Object
{
   static lout::container::typed::HashTable
      <lout::object::String, Hyphenator> *hyphenators;
   Trie *trie;

   lout::container::typed::HashTable <lout::object::ConstString,
                                      lout::container::typed::Vector
                                      <lout::object::Integer> > *exceptions;

   void insertPattern (TrieBuilder *trieBuilder, char *s);
   void insertException (char *s);

   void hyphenateSingleWord(core::Platform *platform, char *wordLc, int offset,
                            lout::misc::SimpleVector <int> *breakPos);
   bool isCharPartOfActualWord (char *s);

public:
   Hyphenator (const char *patFile, const char *excFile, int pack = 256);
   ~Hyphenator();

   static Hyphenator *getHyphenator (const char *language);
   static bool isHyphenationCandidate (const char *word);
   int *hyphenateWord(core::Platform *platform, const char *word, int *numBreaks);
   void saveTrie (FILE *fp) { trie->save (fp); };
};

} // namespace dw

#endif // __DW_HYPHENATOR_HH__
