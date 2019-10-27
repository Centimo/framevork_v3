//
// Created by centimo on 11.06.19.
//

#pragma once

#include <atomic>
#include <array>
#include <optional>

#include "RCU_cell.hpp"
#include "utils.hpp"

template <typename Value_type, size_t power_of_two_for_capacity = 8>
class Atomic_queue
{
  constexpr static size_t _capacity = utils::Power<2, power_of_two_for_capacity>::result;

public:

  unsigned char push(const Value_type& value)
  {
    auto current_id = _current_free_value_keeper.fetch_add(1) % _capacity;
    _queue[current_id].write(value);
    _size.fetch_add(1);
    return current_id;
  }

  unsigned short size()
  {
    auto elements_number = _size.load();
    if (elements_number > 0)
    {
      return elements_number;
    }
    else
    {
      return 0;
    }
  }

  Value_type& get_reference(const unsigned char id)
  {
    return _queue[id];
  }

  unsigned char get_last_value_id()
  {
    return static_cast<unsigned char>(_current_free_value_keeper.load() % _capacity - 1);
  }

  std::optional<Value_type> pop()
  {
    auto prev_elements_number = _size.fetch_sub(1);
    if (prev_elements_number < 1)
    {
      _size.fetch_add(1);
      return std::nullopt;
    }

    auto current_id = _current_first_value.fetch_add(1);

    return _queue[current_id].read();
  }

private:

  std::array<RCU_cell_light<Value_type>, _capacity> _queue;
  std::atomic_ulong _current_free_value_keeper;
  std::atomic_ulong _current_first_value;

  std::atomic_short _size;
};