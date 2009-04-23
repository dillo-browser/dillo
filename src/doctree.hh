#ifndef __DOCTREE_HH__
#define __DOCTREE_HH__

class DoctreeNode {
   public:
      int num; // unique ascending id
      int depth;
      int element;
      const char *klass;
      const char *pseudo;
      const char *id;
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
   public:
      virtual ~Doctree () {};
      virtual const DoctreeNode *top () = 0;
      virtual const DoctreeNode *parent (const DoctreeNode *node) = 0;
};

#endif
