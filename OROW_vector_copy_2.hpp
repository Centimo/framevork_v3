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

  struct Range
  {
    size_t _start_index;
  };

  struct Part
  {
    mutable std::atomic<size_t> _range_index;
    size_t _first_free_element;
    size_t _last_empty_element;
    size_t _elements_number;
    size_t _free_elements_number;
  };

private:

public:
  OROW_vector(size_t size, size_t parts_number)
      : _ranges_size(size / parts_number + 1),
        _ranges(parts_number + _additional_ranges_number),
        _parts(parts_number),
        _free_ranges({parts_number, parts_number + 1}),
        _reading_range(0),
        _free_elements_number(size)
  {
    _elements.reserve(_ranges_size * _parts.size());

    for (size_t i = 0; i < _parts.size(); ++i)
    {
      _parts[i] = { i, 0, _ranges_size - 1, _ranges_size, _ranges_size };
    }

    for (size_t i = 0; i < _ranges.size(); ++i)
    {
      _ranges[i] = { i * _ranges_size };
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
          std::optional<Value_type> (const std::optional<std::reference_wrapper<const Value_type> >&, const size_t)
      >& function)
  {
    for (size_t part_index = 0; part_index < _parts.size(); ++part_index)
    {
      Part& part = _parts[part_index];
      size_t next_free_element = part._first_free_element;
      size_t previous_free_element = part._last_empty_element;
      const u_char free_range_index = _free_ranges[0] != _reading_range.load() ? 0 : 1;
      const size_t destination_range_index = _free_ranges[free_range_index];

      const Range& destination_range = _ranges[destination_range_index];
      const size_t source_range_index = part._range_index.load();
      const Range& source_range = _ranges[source_range_index];

      for (size_t element_index = 0; element_index < part._elements_number; ++element_index)
      {
        std::optional<Value_type> result;

        auto value_pointer = std::get_if<Value_type>(&_elements[source_range._start_index + element_index]);
        if (value_pointer)
        {
          result = function(*value_pointer, part_index);

          if (!result)
          {
            _elements[source_range._start_index + element_index] =
                Empty_element
                {
                    part._free_elements_number ? next_free_element : element_index,
                    part._free_elements_number ? previous_free_element : element_index
                };

            previous_free_element = element_index;

            if (part._free_elements_number)
            {
              std::get<Empty_element>(
                  _elements[source_range._start_index + previous_free_element]
              )._next_empty_element = element_index;

              std::get<Empty_element>(
                  _elements[source_range._start_index + next_free_element]
              )._previous_free_element = element_index;
            }

            ++part._free_elements_number;
            ++_free_elements_number;
          }
        }
        else
        {
          result = function(std::nullopt, part_index);

          std::tie(next_free_element, previous_free_element) =
              std::get<Empty_element>(
                  _elements[source_range._start_index + element_index]
              );

          if (result)
          {
            --part._free_elements_number;
            --_free_elements_number;

            if (part._free_elements_number)
            {
              std::get<Empty_element>(
                  _elements[source_range._start_index + previous_free_element]
              )._next_empty_element = next_free_element;

              std::get<Empty_element>(
                  _elements[source_range._start_index + next_free_element]
              )._previous_free_element = previous_free_element;
            }
          }
          else
          {
            previous_free_element = element_index;
          }
        }


        if (result)
        {
          _elements[destination_range._start_index + element_index] = result.value();
        }
      }

      part._range_index.store(destination_range_index);
      _free_ranges[free_range_index] = source_range_index;
    }
  }

  /*
  void call_function_for_all_empty_elements(
      const std::function<
          std::optional<Value_type> (const size_t)
      >& function)
  {
    for (size_t part_index = 0; part_index < _parts.size(); ++part_index)
    {

    }
  }
   */

private:
  const size_t _ranges_size;
  const std::vector<Range> _ranges;

  std::vector<std::variant<Empty_element, Value_type> > _elements;
  std::vector<Part> _parts;
  std::array<size_t, _additional_ranges_number> _free_ranges;
  mutable std::atomic<size_t> _reading_range;
  std::atomic<size_t> _free_elements_number;
};