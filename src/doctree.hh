#ifndef __DOCTREE_HH__
#define __DOCTREE_HH__

class DoctreeNode {
   private:
      int index;

   public:
      int tagIndex;
      const char *klass;
      const char *id;
};

class Doctree {
   public:
      virtual ~Doctree () = 0;
      virtual const DoctreeNode *top () = 0;
      virtual const DoctreeNode *parent (const DoctreeNode *node) = 0;
};

#endif
