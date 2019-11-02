//
// Created by centimo on 11.06.19.
//

#pragma once

#include <atomic>
#include <vector>
#include <optional>

#include "utils.h"


template <typename Value_type>
class Atomic_queue
{
  struct Cell
  {
    std::atomic<bool> _is_writed;
    Value_type _value;

    Cell(const Cell& copy) : _is_writed(copy._is_writed.load()), _value(copy._value) {};
    Cell(bool is_writed) : _is_writed(is_writed) {};
  };

  size_t wrapper_sub(size_t first, size_t second)
  {
    int result = first - second;
    if (result < 0)
    {
      result = 
    }
  }


private:
  static size_t make_capacity(size_t reserve_elements_number, size_t desired_capacity_exponent)
  {
    size_t result = utils::two_to_the_power_of(desired_capacity_exponent);
    for (size_t exponent = desired_capacity_exponent + 1;
         result <= reserve_elements_number && exponent <= sizeof(size_t)*8; 
         ++exponent)
    {
      result *= 2;
    }

    return result;
  }

public:
  Atomic_queue(size_t reserve_elements_number = 1, size_t exponent_of_capacity = 8) :
    _reserve_elements_number(reserve_elements_number),
    _capacity(make_capacity(reserve_elements_number, exponent_of_capacity)),
    _queue(_capacity, Cell(false)),
    _current_free_value_keeper(0),
    _current_first_value(0),
    _size(0) 
  { }

  bool push(const Value_type& value)
  {
    {
      int previous_size = _size.fetch_add(1);
      if (previous_size > _capacity - _reserve_elements_number)
      {
        _size.fetch_sub(1);
        return false;
      }
    }

    size_t current_id = _current_free_value_keeper.fetch_add(1) % _capacity;
    _queue[current_id]._value = value;
    _queue[current_id]._is_writed.store(true);
    return true;
  }

  std::optional<Value_type> pop()
  {
    if (_current_first_value.load() == _current_free_value_keeper.load())
    {
      return std::nullopt;
    }

    {
      int previous_size = _size.fetch_sub(1);
      if (previous_size < 1)
      {
        _size.fetch_add(1);
        return std::nullopt;
      }
    }

    size_t current_id = _current_first_value.fetch_add(1) % _capacity;

    while (!_queue[current_id]._is_writed.load())
    {
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    Value_type result = _queue[current_id]._value;
    _queue[current_id]._is_writed.store(false);
    return result;
  }

  size_t size()
  {
    int elements_number = _size.load();
    if (elements_number > 0)
    {
      return elements_number;
    }
    else
    {
      return 0;
    }
  }

public:
  const size_t _reserve_elements_number;
  const size_t _capacity;

private:
  std::vector<Cell> _queue;
  std::atomic_ulong _current_free_value_keeper;
  std::atomic_ulong _current_first_value;

  std::atomic<int> _size;
};