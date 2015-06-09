#ifndef __DW_TOOLS_HH__
#define __DW_TOOLS_HH__

#include "core.hh"
#include "../lout/debug.hh"

namespace dw {
namespace core {

/**
 * \brief Hold arguments passed to dw::core::Widget::sizeRequest and
 * dw::core::Widget::getExtremes, as described in \ref dw-size-request-pos.
 */
class SizeParams
{
private:
   int numPos;
   Widget **references;
   int *x, *y;

   void init ();
   void cleanup ();
   
public:
   SizeParams ();
   ~SizeParams ();

   void fill (int numPos, Widget **references, int *x, int *y);
   void forChild (Widget *parent, Widget *child, int xRel, int yRel,
                  SizeParams *childParams);
   bool findReference (Widget *reference, int *x, int *y);
   
   inline int getNumPos () { return numPos; }
   inline Widget **getReferences () { return references; }
   inline int *getX () { return x; }
   inline int *getY () { return y; }
   inline Widget *getReference (int i) { return references[i]; }
   inline int getX (int i) { return x[i]; }
   inline int getY (int i) { return y[i]; }
};

#define DBG_SET_SIZE_PARAMS(prefix, params) \
   D_STMT_START { \
      DBG_IF_RTFL { \
         DBG_OBJ_SET_NUM (prefix ".numPos", params.getNumPos ()); \
         for (int i = 0; i < params.getNumPos (); i++) { \
            DBG_OBJ_ARRSET_PTR (prefix ".references", i, \
                                params.getReference (i)); \
            DBG_OBJ_ARRSET_NUM (prefix ".x", i, params.getX (i)); \
            DBG_OBJ_ARRSET_NUM (prefix ".y", i, params.getY (i)); \
         } \
      } \
   } D_STMT_END

   
} // namespace core
} // namespace dw

#endif // __DW_TOOLS_HH__
