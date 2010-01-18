#ifndef __DOCTREE_HH__
#define __DOCTREE_HH__

#include "lout/misc.hh"

class DoctreeNode {
   public:
      DoctreeNode *parent;
      int num; // unique ascending id
      int element;
      lout::misc::SimpleVector<char*> *klass;
      const char *pseudo;
      const char *id;

      DoctreeNode () {
         parent = NULL;
         klass = NULL;
         pseudo = NULL;
         id = NULL;
         element = 0;
      };
};

/**
 * \brief HTML document tree interface.
 *
 * The Doctree class defines the interface to the parsed HTML document tree
 * as it is used for CSS selector matching.
 * Currently the Doctree can be represented as stack, however to support
 * CSS adjacent siblings or for future JavaScript support it may have to
 * be extended to a real tree.
 */
class Doctree {
   private:
      DoctreeNode *topNode;
      int num;

   public:
      Doctree () {
         topNode = NULL;
         num = 0;
      };
      ~Doctree () { while (top ()) pop (); };
      DoctreeNode *push () {
         DoctreeNode *dn = new DoctreeNode ();
         dn->parent = topNode;
         dn->num = num++;
         topNode = dn;
         return dn;
      };
      void pop () {
         DoctreeNode *dn = topNode;
         if (dn) {
            dFree ((void*) dn->id);
            if (dn->klass) {
               for (int i = 0; i < dn->klass->size (); i++)
                  dFree (dn->klass->get(i));
               delete dn->klass;
            }
            topNode = dn->parent;
            delete dn;
         }
      };
      inline DoctreeNode *top () {
         return topNode;
      };
      inline DoctreeNode *parent (const DoctreeNode *node) {
         return node->parent;
      };
};

#endif
