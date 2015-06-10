#include "tools.hh"

namespace dw {
namespace core {

SizeParams::SizeParams ()
{
   DBG_OBJ_CREATE ("dw::core::SizeParams");
   init ();
   debugPrint ();
}
   
SizeParams::~SizeParams ()
{
   cleanup ();
   DBG_OBJ_DELETE ();
}

void SizeParams::init ()
{
   numPos = 0;
   references = NULL;
   x = y = NULL;
}

void SizeParams::cleanup ()
{
   if (references)
      delete[] references;
   if (x)
      delete[] x;
   if (y)
      delete[] y;

   init ();
}

void SizeParams::fill (int numPos, Widget **references, int *x, int *y)
{
   cleanup ();
   
   this->numPos = numPos;

   this->references = new Widget*[numPos];
   this->x = new int[numPos];
   this->y = new int[numPos];
   
   for (int i = 0; i < numPos; i++) {
      this->references[i] = references[i];
      this->x[i] = x[i];
      this->y[i] = y[i];
   }

   debugPrint ();
}

void SizeParams::forChild (Widget *parent, Widget *child, int xRel, int yRel,
                           SizeParams *childParams)
{
   int numChildReferences = child->numSizeRequestReferences ();

   childParams->numPos = 0;
   childParams->references = new Widget*[numChildReferences];
   childParams->x = new int[numChildReferences];
   childParams->y = new int[numChildReferences];

   for (int i = 0; i < numChildReferences; i++) {
      Widget *childReference = child->sizeRequestReference (i);
      if (childReference == parent) {
         childParams->references[numPos] = childReference;
         childParams->x[numPos] = xRel;
         childParams->y[numPos] = yRel;
         numPos++;
      } else {
         bool found = false;
         for (int j = 0; !found && j < numPos; j++) {
            if (childReference == references[j]) {
               found = true;
               references[numPos] = childReference;
               childParams->x[numPos] = x[j] + xRel;
               childParams->y[numPos] = y[j] + yRel;
               numPos++;
            } 
         }
      }
   }

   childParams->debugPrint ();
}

bool SizeParams::findReference (Widget *reference, int *x, int *y)
{
   for (int i = 0; i < numPos; i++) {
      if (reference == references[i]) {
         if (x)
            *x = this->x[i];
         if (y)
            *y = this->y[i];
         return true;
      }
   }

   return false;
}
  

} // namespace core
} // namespace dw
