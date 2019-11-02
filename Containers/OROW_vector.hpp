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
#include <mutex>


template <typename Value_type>
class OROW_vector
{
  constexpr static size_t _additional_ranges_number = 2;

private:
  struct Range
  {
    size_t _start_index;

    static std::vector<Range> make_vector_of_ranges(size_t size, size_t ranges_size)
    {
      std::vector<Range> result(size);

      for (size_t i = 0; i < size; ++i)
      {
        result[i] = { i * ranges_size };
      }

      return result;
    }
  };

  struct Part
  {
    mutable std::atomic<size_t> _range_index;
    size_t _elements_number;

    Part(const Part& source)
      : _range_index(source._range_index.load()),
        _elements_number(source._elements_number)
    { }

    Part(const size_t range_index, 
         const size_t elements_number)

      : _range_index(range_index),
        _elements_number(elements_number)
    { }

    static std::vector<Part> make_vector_of_parts(size_t size, size_t ranges_size)
    {
      std::vector<Part> result(size, Part(0, ranges_size));

      for (size_t i = 0; i < size; ++i)
      {
        result[i]._range_index.store(i);
      }

      return result;
    }
  };

private:

public:
  OROW_vector(size_t size, size_t parts_number)
      : _ranges_size(size / parts_number + 1),
        _desired_size(size),
        _ranges(Range::make_vector_of_ranges(parts_number + _additional_ranges_number, _ranges_size)),
        _parts(Part::make_vector_of_parts(parts_number, _ranges_size)),
        _free_ranges({parts_number, parts_number + 1}),
        _reading_range(0)
  {
    _parts.back()._elements_number = size - (parts_number - 1) * _ranges_size;
    _elements.assign(_ranges_size * _ranges.size(), std::nullopt);
    _empty_elements_number.store(size);
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
        if (auto value = _elements[i])
        {
          function(value.value());
        }
      }

      _reading_range.store(-1);
    }
  }

  void call_function_for_all_elements(
      const std::function<
          std::optional<Value_type> (const std::optional<std::reference_wrapper<const Value_type> >&)
      >& function)
  {
    for (size_t part_index = 0; part_index < _parts.size(); ++part_index)
    {
      Part& part = _parts[part_index];
      const size_t free_range_index = (_free_ranges[0] != _reading_range.load()) ? 0 : 1;
      const size_t destination_range_index = _free_ranges[free_range_index];

      const Range& destination_range = _ranges[destination_range_index];
      const size_t source_range_index = part._range_index.load();
      const Range& source_range = _ranges[source_range_index];

      for (size_t element_index = 0; element_index < part._elements_number; ++element_index)
      {
        std::optional<Value_type> result;

        auto& current_value = _elements[source_range._start_index + element_index];
        if (current_value)
        {
          result = function(current_value);

          if (!result)
          {
            _empty_elements_number.fetch_add(1);
          }
        }
        else
        {
          result = function(std::nullopt);

          if (result)
          {
            _empty_elements_number.fetch_sub(1);
          }
        }

        _elements[destination_range._start_index + element_index] = result;
      }

      part._range_index.store(destination_range_index);
      _free_ranges[free_range_index] = source_range_index;
    }
  }

  size_t size() const
  {
    return _desired_size - _empty_elements_number.load();
  }

  bool is_empty() const
  {
    return _desired_size == _empty_elements_number.load();
  }

  size_t empty_number() const
  {
    return _empty_elements_number.load();
  }

private:
  const size_t _ranges_size;
  const std::vector<Range> _ranges;
  const size_t _desired_size;

  std::vector<std::optional<Value_type> > _elements;
  std::vector<Part> _parts;
  std::array<size_t, _additional_ranges_number> _free_ranges;
  mutable std::atomic<size_t> _reading_range;
  std::atomic<size_t> _empty_elements_number;
};
