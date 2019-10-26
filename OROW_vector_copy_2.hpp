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
        _reading_range(0)
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
      size_t range_index = part._range_index.load();
      _reading_range.store(range_index);
      const Range& range = _ranges[range_index];

      for (size_t i = range._start_index; i < range._start_index + part._elements_number; ++i)
      {
        if (auto value_pointer = std::get_if<Value_type>(&_elements[i]))
        {
          function(*value_pointer);
        }
      }

      // TODO: ? _reading_range.store(-1);
    }
  }

  void call_function_for_all_elements(
      const std::function<
          std::optional<Value_type> (const std::optional<std::reference_wrapper<const Value_type> >&)
      >& function)
  {
    for (const Part& part : _parts)
    {
      u_char free_range_index = _free_ranges[0] != _reading_range.load() ? 0 : 1;
      size_t destination_range_index = _free_ranges[free_range_index];

      const Range& destination_range = _ranges[destination_range_index];
      const size_t source_range_index = part._range_index.load();
      const Range& source_range = _ranges[source_range_index];

      for (size_t i = 0; i < part._elements_number; ++i)
      {
        auto value_pointer = std::get_if<Value_type>(&_elements[source_range._start_index + i]);
        std::optional<Value_type> result = value_pointer ? function(*value_pointer) : function(std::nullopt);

        if (!result)
        {
          continue;
        }

        _elements[destination_range._start_index + i] = result;
      }

      _free_ranges[free_range_index] = source_range_index;
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
  mutable std::atomic<size_t> _reading_range;
};