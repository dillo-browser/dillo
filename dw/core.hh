#ifndef __DW_CORE_HH__
#define __DW_CORE_HH__

#define __INCLUDED_FROM_DW_CORE_HH__

/**
 * \brief Dw is in this namespace, or sub namespaces of this one.
 *
 * The core can be found in dw::core, widgets are defined directly here.
 *
 * \sa \ref dw-overview
 */
namespace dw {

/**
 * \brief The core of Dw is defined in this namespace.
 *
 * \sa \ref dw-overview
 */
namespace core {

typedef unsigned char byte;

class Layout;
class View;
class Widget;
class Iterator;

// Nothing yet to free.
inline void freeall () { }

namespace ui {

class ResourceFactory;

} // namespace ui
} // namespace core
} // namespace dw

#include "../lout/object.hh"
#include "../lout/container.hh"
#include "../lout/signal.hh"

#include "types.hh"
#include "events.hh"
#include "imgbuf.hh"
#include "imgrenderer.hh"
#include "style.hh"
#include "view.hh"
#include "platform.hh"
#include "iterator.hh"
#include "findtext.hh"
#include "selection.hh"
#include "layout.hh"
#include "widget.hh"
#include "ui.hh"

#undef __INCLUDED_FROM_DW_CORE_HH__

#endif // __DW_CORE_HH__
