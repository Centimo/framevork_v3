//
// Created by centimo on 24.10.2019.
//
// One Writer One Reader vector

#pragma once

#include <atomic>
#include <vector>
#include <array>
#include <variant>
#include <functional>

#include "Atomic_meta.hpp"


template <typename Value_type>
class OROW_vector
{
  constexpr static size_t _additional_ranges_number = 2;

private:
  struct Empty_element
  {
    size_t _next_empty_element;
    size_t _previous_empty_element;
  };

  enum Range_state
  {
    FREE,
    READING,
    WRITING
  };

  enum Copy_used
  {
    NONE,
    FIRST,
    SECOND
  };

  struct Range
  {
    size_t _start_index;

    mutable std::atomic<Range_state> _state;
  };

  struct Part
  {
    mutable std::atomic<size_t> _range_index;
    size_t _elements_number;
  };

private:

public:
  OROW_vector(size_t size, size_t parts_number)
      : _ranges_size(size / parts_number + 1),
        _ranges(parts_number + _additional_ranges_number),
        _parts(parts_number),
        _first_free_elements_of_parts(parts_number, 0),
        _is_reading(false)
  {
    _elements.reserve(_ranges_size * _parts.size());

    for (size_t i = 0; i < _parts.size(); ++i)
    {
      _parts[i] = { i, _ranges_size };
    }

    _parts.back()._elements_number = size - (parts_number - 1) * _ranges_size;
  }

  void call_function_for_all_nonempty_elements(const std::function<void (const Value_type&)>& function) const
  {
    for (const Part& part : _parts)
    {
      size_t range_index = 0;

      while (true)
      {
        auto previous_range_state = Range_state::FREE;
        range_index = part._range_index.load();
        if (_ranges[range_index]._state.compare_exchange_strong(previous_range_state, Range_state::READING))
        {
          break;
        }
      }

      const Range& range = _ranges[range_index];

      for (size_t i = range._start_index; i < range._start_index + part._elements_number; ++i)
      {
        if (auto value_pointer = std::get_if<Value_type>(&_elements[i]))
        {
          function(*value_pointer);
        }
      }

      range._state.store(Range_state::FREE);
    }
  }

  void call_function_for_all_elements(
      const std::function<
          std::optional<Value_type> (const std::optional<std::reference_wrapper<const Value_type> >&)
      >& function)
  {
    for (const Part& part : _parts)
    {
      size_t destination_range_index = 0;
      u_char free_range_index = 0;

      for (;; free_range_index = (free_range_index + 1) % _additional_ranges_number)
      {
        auto previous_range_state = Range_state::FREE;
        destination_range_index = _free_ranges[free_range_index];
        if (_ranges[destination_range_index]._state.compare_exchange_strong(previous_range_state, Range_state::WRITING))
        {
          break;
        }
      }

      const Range& destination_range = _ranges[destination_range_index];
      const size_t source_range_index = part._range_index.load();

      for (size_t i = 0; i < part._elements_number; ++i)
      {
        auto value_pointer = std::get_if<Value_type>(&_elements[_ranges[source_range_index]._start_index + i]);
        std::optional<Value_type> result = value_pointer ? function(*value_pointer) : function(std::nullopt);

        if (!result)
        {
          continue;
        }

        _elements[destination_range._start_index + i] = result;
      }

      _free_ranges[free_range_index] = source_range_index;
      destination_range._state.store(Range_state::FREE);
      part._range_index.store(destination_range_index);
    }
  }

private:
  const size_t _ranges_size;
  const std::vector<Range> _ranges;

  std::vector<std::variant<Empty_element, Value_type> > _elements;
  std::vector<Part> _parts;
  std::vector<size_t> _first_free_elements_of_parts;
  std::array<size_t, _additional_ranges_number> _free_ranges;
  mutable std::atomic<bool> _is_reading;
};