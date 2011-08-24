#ifndef __DOCTREE_HH__
#define __DOCTREE_HH__

#include "lout/misc.hh"

class DoctreeNode {
   public:
      DoctreeNode *parent;
      DoctreeNode *sibling;
      DoctreeNode *lastChild;
      int num; // unique ascending id
      int element;
      lout::misc::SimpleVector<char*> *klass;
      const char *pseudo;
      const char *id;

      DoctreeNode () {
         parent = NULL;
         sibling = NULL;
         lastChild = NULL;
         klass = NULL;
         pseudo = NULL;
         id = NULL;
         element = 0;
      };

      ~DoctreeNode () {
         dFree ((void*) id);
         while (lastChild) {
            DoctreeNode *n = lastChild;
            lastChild = lastChild->sibling;
            delete n;
         }
         if (klass) {
            for (int i = 0; i < klass->size (); i++)
               dFree (klass->get(i));
            delete klass;
         }
      }
};

/**
 * \brief HTML document tree interface.
 *
 * The Doctree class defines the interface to the parsed HTML document tree
 * as it is used for CSS selector matching.
 */
class Doctree {
   private:
      DoctreeNode *topNode;
      DoctreeNode *rootNode;
      int num;

   public:
      Doctree () {
         topNode = NULL;
         rootNode = NULL;
         num = 0;
      };

      ~Doctree () { 
         if (rootNode)
            delete rootNode;
      };

      DoctreeNode *push () {
         DoctreeNode *dn = new DoctreeNode ();
         dn->parent = topNode;
         if (dn->parent) {
            dn->sibling = dn->parent->lastChild;
            dn->parent->lastChild = dn;
         }
         dn->num = num++;
         if (!rootNode)
            rootNode = dn;
         topNode = dn;
         return dn;
      };

      void pop () {
         if (topNode)
            topNode = topNode->parent;
      };

      inline DoctreeNode *top () {
         return topNode;
      };
};

#endif
