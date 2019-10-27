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
private:
  struct Empty_element
  {
    size_t _next_empty_element;
    size_t _previous_empty_element;
  };

  enum Part_state
  {
    FREE,
    READING,
    WRITING,
    READING_AND_WRITING
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
    size_t _end_index; // [_start_index; _end_index)

    mutable std::atomic<Part_state> _state;
    mutable std::atomic<Copy_used> _copy_used;
  };

private:
  void update_from_copy(const Range& range)
  {
    auto destination = _elements.begin();
    std::advance(destination, range._start_index);

    std::copy_n(
        _copies[range._copy_used.load()].begin(),
        range._end_index - range._start_index,
        destination);
  }

public:
  OROW_vector(size_t size, size_t parts_number)
    : _elements(size, Empty_element{}),
      _parts(parts_number),
      _first_free_elements_of_parts(parts_number, 0)
  {
    size_t part_size = size / parts_number;
    size_t number_of_parts_with_additional_element = size % parts_number;

    for (size_t i = 0, previous_end = 0; i < _parts.size(); ++i)
    {
      size_t next_start_index = previous_end + part_size + (i < number_of_parts_with_additional_element ? 1 : 0);
      _parts[i] = { previous_end, next_start_index };
      previous_end = next_start_index;
    }

    _copies[0].reserve(part_size + 1);
    _copies[1].reserve(part_size + 1);
  }

  void call_function_for_all_nonempty_elements(const std::function<void (const Value_type&)>& function) const
  {
    for (const Range& part : _parts)
    {
      using namespace Atomic_meta;

      Part_state previous_state =
          compare(part._state).
            template _and_swap_with_one_of<
                         Change<Part_state::FREE>::template To<Part_state::WRITING>,
                         Change<Part_state::WRITING>::template To<Part_state::READING_AND_WRITING>
                     >();

      for (size_t i = part._start_index; i < part._end_index; ++i)
      {
        if (auto value_pointer = std::get_if<Value_type>(&_elements[i]))
        {
          function(*value_pointer);
        }
      }

      previous_state =
          compare(part._state).
              template _and_swap_with_one_of<
                           Change<Part_state::READING_AND_WRITING>::template To<Part_state::WRITING>,
                           Change<Part_state::READING>::template To<Part_state::FREE>
                       >();

      if (previous_state == Part_state::READING && part._copy_used.load() != Copy_used::NONE)
      {
        update_from_copy(part);

        part._copy_used.store(Copy_used::NONE);
      }
    }
  }

  void call_function_for_all_elements(
         const std::function<
                     std::optional<Value_type> (const std::optional<std::reference_wrapper<const Value_type> >&)
               >& function)
  {
    using namespace Atomic_meta;

    for (const Range& part : _parts)
    {
      Part_state previous_state =
          compare(part._state).
              template _and_swap_with_one_of<
                           Change<Part_state::FREE>::template To<Part_state::WRITING>,
                           Change<Part_state::WRITING>::template To<Part_state::READING_AND_WRITING>
                       >();

      for (size_t i = part._start_index; i < part._end_index; ++i)
      {
        auto value_pointer = std::get_if<Value_type>(&_elements[i]);
        std::optional<Value_type> result = value_pointer ? function(*value_pointer) : function(std::nullopt);

        if (!result)
        {
          continue;
        }
      }
    }
  }

private:
  std::vector<std::variant<Empty_element, Value_type> > _elements;

  std::vector<Range> _parts;
  std::vector<size_t> _first_free_elements_of_parts;
  mutable std::array<std::vector<std::variant<Empty_element, Value_type>>, 2> _copies;
};