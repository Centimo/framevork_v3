//
// Created by centimo on 11.06.19.
//

#pragma once

#include <atomic>
#include <array>
#include <optional>

#include "utils.h"
#include "RCU_cell.hpp"


template <typename Value_type>
class Atomic_queue
{
public:

  size_t push(const Value_type& value)
  {
    size_t current_id = _current_free_value_keeper.fetch_add(1) % _capacity;
    _queue[current_id].write(value);
    _size.fetch_add(1);
    return current_id;
  }

  size_t size()
  {
    size_t elements_number = _size.load();
    if (elements_number > 0)
    {
      return elements_number;
    }
    else
    {
      return 0;
    }
  }

  Atomic_queue(size_t exponenta_of_capacity = 8) :
    _capacity(utils::two_to_the_power_of(exponenta_of_capacity)),
    _queue(_capacity),
    _current_free_value_keeper(0),
    _current_first_value(0),
    _size(0) 
  { }

  std::optional<Value_type> pop()
  {
    size_t prev_elements_number = _size.fetch_sub(1);
    if (prev_elements_number < 1)
    {
      _size.fetch_add(1);
      return std::nullopt;
    }

    size_t current_id = _current_first_value.fetch_add(1) % _capacity;

    return _queue[current_id].read();
  }

private:
  size_t _capacity;
  std::vector<RCU_cell_light<Value_type>> _queue;
  std::atomic_ulong _current_free_value_keeper;
  std::atomic_ulong _current_first_value;

  std::atomic_int _size;
};