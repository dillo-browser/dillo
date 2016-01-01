#include "core.hh"

namespace dw {
namespace core {

using namespace lout::misc;
   
SizeParams::SizeParams ()
{
   DBG_OBJ_CREATE ("dw::core::SizeParams");
   init ();
   debugPrint ();
}

SizeParams::SizeParams (int numPos, Widget **references, int *x, int *y)
{
   DBG_OBJ_CREATE ("dw::core::SizeParams");
   init ();
   fill (numPos, references, x, y);
   debugPrint ();
}

SizeParams::SizeParams (const SizeParams &other)
{
   DBG_OBJ_CREATE ("dw::core::SizeParams");
   init ();
   fill (other.numPos, other.references, other.x, other.y);
   debugPrint ();
}
   
SizeParams::~SizeParams ()
{
   cleanup ();
   DBG_OBJ_DELETE ();
}

SizeParams &SizeParams::operator=(const SizeParams &other)
{
   cleanup ();
   init ();
   fill (other.numPos, other.references, other.x, other.y);
   debugPrint ();
   return *this;
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
   DBG_OBJ_ENTER ("resize", 0, "fill", "%d, ...", numPos);

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

   DBG_OBJ_LEAVE ();
}

void SizeParams::forChild (Widget *parent, Widget *child, int xRel, int yRel,
                           SizeParams *childParams)
{
   DBG_OBJ_ENTER ("resize", 0, "forChild", "%p, %p, %d, %d, %p",
                  parent, child, xRel, yRel, childParams);
   
   int numChildReferences = child->numSizeRequestReferences ();

   childParams->numPos = 0;
   childParams->references = new Widget*[numChildReferences];
   childParams->x = new int[numChildReferences];
   childParams->y = new int[numChildReferences];

   for (int i = 0; i < numChildReferences; i++) {
      Widget *childReference = child->sizeRequestReference (i);
      
      if (childReference == parent) {
         childParams->references[childParams->numPos] = childReference;
         childParams->x[childParams->numPos] = xRel;
         childParams->y[childParams->numPos] = yRel;
         childParams->numPos++;
      } else {
         bool found = false;
         for (int j = 0; !found && j < numPos; j++) {
            if (childReference == references[j]) {
               found = true;
               childParams->references[childParams->numPos] = childReference;
               childParams->x[childParams->numPos] = x[j] + xRel;
               childParams->y[childParams->numPos] = y[j] + yRel;
               childParams->numPos++;
            } 
         }
      }
   }

   childParams->debugPrint ();

   DBG_OBJ_LEAVE ();
}

bool SizeParams::findReference (Widget *reference, int *x, int *y)
{
   DBG_OBJ_ENTER ("resize", 0, "findReference", "%p", reference);

   bool found = false;
   for (int i = 0; i < numPos && !found; i++) {
      if (reference == references[i]) {
         if (x)
            *x = this->x[i];
         if (y)
            *y = this->y[i];
         found = true;
      }
   }

   if (found) {
      if (x && y)
         DBG_OBJ_LEAVE_VAL ("true, x = %d, y = %d", *x, *y);
      else if (x)
         DBG_OBJ_LEAVE_VAL ("true, x = %d", *x);
      else if (y)
         DBG_OBJ_LEAVE_VAL ("true, y = %d", *y);
      else
         DBG_OBJ_LEAVE_VAL ("%s", "true");
   } else
      DBG_OBJ_LEAVE_VAL ("%s", "false");

   return found;
}

/**
 * Compares two instances, but considers a change in the order of the reference
 * widgets as equivalent.
 */
bool SizeParams::isEquivalent (SizeParams *other)
{
   DBG_OBJ_ENTER ("resize", 0, "isEquivalent", "%p", other);
   bool result;
   
   if (numPos != other->numPos)
      result = false;
   else {
      result = true;
      
      for (int i = 0; result && i < numPos; i++) {
         bool otherFound = false;
         for (int j = 0; !otherFound && j < numPos; j++) {
            if (references[i] == other->references[j]) {
               otherFound = true;
               if (!(x[i] == other->x[j] && y[i] == other->y[j]))
                  result = false;
            }
         }
         
         if (!otherFound)
            result = false;
      }
   }
   
   DBG_OBJ_LEAVE_VAL ("%s", boolToStr (result));
   return result;
}
  
} // namespace core
} // namespace dw
