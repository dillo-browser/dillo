#ifndef __DW_EVENTS_HH__
#define __DW_EVENTS_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief Platform independent representation.
 */
enum ButtonState
{
   /* We won't use more than these ones. */
   SHIFT_MASK    = 1 << 0,
   CONTROL_MASK  = 1 << 1,
   META_MASK     = 1 << 2,
   BUTTON1_MASK  = 1 << 3,
   BUTTON2_MASK  = 1 << 4,
   BUTTON3_MASK  = 1 << 5
};

/**
 * \brief Base class for all events.
 *
 * The dw::core::Event hierarchy describes events in a platform independent
 * way.
 */
class Event: public lout::object::Object
{
public:
};

/**
 * \brief Base class for all mouse events.
 */
class MouseEvent: public Event
{
public:
   ButtonState state;
};

/**
 * \brief Base class for all mouse events related to a specific position.
 */
class MousePositionEvent: public MouseEvent
{
public:
   int xCanvas, yCanvas, xWidget, yWidget;
};

/**
 * \brief Represents a button press or release event.
 */
class EventButton: public MousePositionEvent
{
public:
   int numPressed; /* 1 for simple click, 2 for double click, etc. */
   int button;
};

/**
 * \brief Represents a mouse motion event.
 */
class EventMotion: public MousePositionEvent
{
};

/**
 * \brief Represents a enter or leave notify event.
 */
class EventCrossing: public MouseEvent
{
public:
   Widget *lastWidget, *currentWidget;
};

} // namespace core
} // namespace dw

#endif // __DW_EVENTS_HH__
