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

class Doctree {
   public:
      virtual ~Doctree () {};
      virtual const DoctreeNode *top () = 0;
      virtual const DoctreeNode *parent (const DoctreeNode *node) = 0;
};

#endif
