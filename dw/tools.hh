#ifndef __DW_TOOLS_HH__
#define __DW_TOOLS_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

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

   inline void debugPrint () {
      DBG_IF_RTFL {
         DBG_OBJ_SET_NUM ("numPos", numPos);
         for (int i = 0; i < numPos; i++) {
            DBG_OBJ_ARRSET_PTR ("references", i, references[i]);
            DBG_OBJ_ARRSET_NUM ("x", i, x[i]);
            DBG_OBJ_ARRSET_NUM ("y", i, y[i]);
         }
      }
   }
   
public:
   SizeParams ();
   SizeParams (int numPos, Widget **references, int *x, int *y);
   SizeParams (SizeParams &other);
   ~SizeParams ();

   void fill (int numPos, Widget **references, int *x, int *y);
   void forChild (Widget *parent, Widget *child, int xRel, int yRel,
                  SizeParams *childParams);
   bool findReference (Widget *reference, int *x, int *y);

   bool isEquivalent (SizeParams *other);

   inline int getNumPos () { return numPos; }
   inline Widget **getReferences () { return references; }
   inline int *getX () { return x; }
   inline int *getY () { return y; }
   inline Widget *getReference (int i) { return references[i]; }
   inline int getX (int i) { return x[i]; }
   inline int getY (int i) { return y[i]; }
};
 
} // namespace core
} // namespace dw

#endif // __DW_TOOLS_HH__
