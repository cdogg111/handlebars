#pragma once

#include <list>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "function.hpp"

namespace handlebars {

template<typename SignalT, typename... SlotArgTs>
struct dispatcher
{
  // signal differentiaties the type of event that is happening
  using signal_type = SignalT;
  // a slot is an event handler which can take arguments and has NO RETURN VALUE
  using slot_type = function<void(SlotArgTs...)>;
  // this tuple packs arguments that will be pack with a signal and passed to an event handler
  using args_storage_type = std::tuple<SlotArgTs...>;
  // a slot list is a sequence of event handlers that will be called consecutively to handle an event
  // list is used here to prevent invalidating iterators when calling a handler destructor, which removes slots from a
  // slot list
  using slot_list_type = std::list<slot_type>;
  // slot id  is a signal and an iterator to a slot packed together to make slot removal easier when calling
  // "disconnect" globally or from handler base class
  using slot_id_type = std::tuple<SignalT, typename slot_list_type::iterator>;
  // slot map, simply maps signals to their corresponding slot lists
  using slot_map_type = std::unordered_map<SignalT, slot_list_type>;
  // events hold all relevant data used to call an event handler
  using event_type = std::tuple<signal_type, args_storage_type>;
  // event queue is a modify-able fifo queue that stores events
  using event_queue_type = std::deque<event_type>;

  // associates a SignalT signal with a callable entity slot
  template<typename SlotT>
  static slot_id_type connect(const SignalT& signal, SlotT&& slot);

  // associates a SignalT signal with a callable entity slot, after binding arguments to it
  template<typename SlotT, typename... BoundArgTs>
  static slot_id_type connect_bind(const SignalT& signal, SlotT&& slot, BoundArgTs&&... args);

  // associates a SignalT signal with a member function pointer slot of a class instance
  template<typename ClassT, typename SlotT>
  static slot_id_type connect_member(const SignalT& signal, ClassT&& target, SlotT slot);

  // associates a SignalT signal with a member function pointer slot of a class instance, after binding arguments to it
  template<typename ClassT, typename SlotT, typename... BoundArgTs>
  static slot_id_type connect_bind_member(const SignalT& signal, ClassT&& target, SlotT slot, BoundArgTs&&... args);

  // pushes a new event onto the queue with a signal value and arguments, if any
  static void push_event(const SignalT& signal, SlotArgTs&&... args);

  // executes events and pops them off of the event queue the amount can be specified by limit, if limit is 0
  // then all events are executed
  static bool respond(size_t limit = 0);

  //  this removes an event handler from a slot list
  static void disconnect(const slot_id_type& slot_id);

  // remove all pending events of a specific type
  static void purge_events(const SignalT& signal);

private:
  // singleton signtature
  dispatcher() {}

  static slot_map_type m_slot_map;
  static event_queue_type m_event_queue;
};

template<typename SignalT, typename... SlotArgTs>
typename dispatcher<SignalT, SlotArgTs...>::slot_map_type dispatcher<SignalT, SlotArgTs...>::m_slot_map = {};

template<typename SignalT, typename... SlotArgTs>
typename dispatcher<SignalT, SlotArgTs...>::event_queue_type dispatcher<SignalT, SlotArgTs...>::m_event_queue = {};
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////Implementation//////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace handlebars {

template<typename SignalT, typename... SlotArgTs>
template<typename SlotT>
typename dispatcher<SignalT, SlotArgTs...>::slot_id_type
dispatcher<SignalT, SlotArgTs...>::connect(const SignalT& signal, SlotT&& slot)
{
  m_slot_map[signal].emplace_back(std::forward<SlotT>(slot));
  return std::make_tuple(signal, --m_slot_map[signal].end());
}

template<typename SignalT, typename... SlotArgTs>
template<typename SlotT, typename... BoundArgTs>
typename dispatcher<SignalT, SlotArgTs...>::slot_id_type
dispatcher<SignalT, SlotArgTs...>::connect_bind(const SignalT& signal, SlotT&& slot, BoundArgTs&&... args)
{
  m_slot_map[signal].emplace_back(std::move(
    [&](SlotArgTs&&... args) { slot(std::forward<BoundArgTs>(bound_args)..., std::forward<SlotArgTs>(args)...) }));
  return std::make_tuple(signal, --m_slot_map[signal].end());
}

template<typename SignalT, typename... SlotArgTs>
template<typename ClassT, typename SlotT>
typename dispatcher<SignalT, SlotArgTs...>::slot_id_type
dispatcher<SignalT, SlotArgTs...>::connect_member(const SignalT& signal, ClassT&& target, SlotT slot)
{
  m_slot_map[signal].emplace_back(std::forward<ClassT>(target), slot);
  return std::make_tuple(signal, --m_slot_map[signal].end());
}

template<typename SignalT, typename... SlotArgTs>
template<typename ClassT, typename SlotT, typename... BoundArgTs>
typename dispatcher<SignalT, SlotArgTs...>::slot_id_type
dispatcher<SignalT, SlotArgTs...>::connect_bind_member(const SignalT& signal,
                                                       ClassT&& target,
                                                       SlotT slot,
                                                       BoundArgTs&&... bound_args)
{
  if constexpr (std::is_pointer_v<ClassT>) {
    m_slot_map[signal].emplace_back(std::move([&](SlotArgTs&&... args) {
      (target->*slot)(std::forward<BoundArgTs>(bound_args)..., std::forward<SlotArgTs>(args)...);
    }));
  } else {
    m_slot_map[signal].emplace_back(std::move([&](SlotArgTs&&... args) {
      (target.*slot)(std::forward<BoundArgTs>(bound_args)..., std::forward<SlotArgTs>(args)...);
    }));
  }
  return std::make_tuple(signal, --m_slot_map[signal].end());
}

template<typename SignalT, typename... SlotArgTs>
void
dispatcher<SignalT, SlotArgTs...>::push_event(const SignalT& signal, SlotArgTs&&... args)
{
  m_event_queue.push_back(std::make_tuple(signal, std::forward_as_tuple(std::forward<SlotArgTs>(args)...)));
}

template<typename SignalT, typename... SlotArgTs>
bool
dispatcher<SignalT, SlotArgTs...>::respond(size_t limit)
{
  if (m_event_queue.size() == 0)
    return false;
  size_t progress = 0;
  for (auto& event : m_event_queue) {
    for (auto& slot : m_slot_map[std::get<0>(event)]) // fire slot chain
    {
      std::apply(slot, std::get<1>(event));
    }
    ++progress;
    m_event_queue.pop_front();
    if (progress == limit)
      break;
  }
  return m_event_queue.size() > 0;
}

template<typename SignalT, typename... SlotArgTs>
void
dispatcher<SignalT, SlotArgTs...>::disconnect(const slot_id_type& slot_id)
{

  m_slot_map[std::get<0>(slot_id)].erase(std::get<1>(slot_id));
}

template<typename SignalT, typename... SlotArgTs>
void
dispatcher<SignalT, SlotArgTs...>::purge_events(const SignalT& signal)
{
  while (m_event_queue.size()) {
    if (std::get<0>(m_event_queue.front()) == signal)
      m_event_queue.pop();
  }
}
}
