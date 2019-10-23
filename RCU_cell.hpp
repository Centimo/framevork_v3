//
// Created by centimo on 12.09.2019.
//

#pragma once


#include <atomic>
#include <array>
#include <thread>
#include <optional>
#include <functional>

#include <boost/scope_exit.hpp>

namespace utils
{

  template <typename Return_type, bool is_void = std::is_void<Return_type>::value>
  class Return_wrapper
  {

  public:
    template <typename Value_type>
    void set(std::function<Return_type(Value_type&)> function, Value_type& value)
    {
      if constexpr (is_void)
      {
        function(value);
      }
      else
      {
        _value = function(value);
      }
    }

    template <typename Value_type>
    void set(std::function<Return_type(const Value_type&)> function, const Value_type& value)
    {
      if constexpr (is_void)
      {
        function(value);
      }
      else
      {
        _value = function(value);
      }
    }

    Return_type get()
    {
      if constexpr (is_void)
      {
        return void();
      }
      else
      {
        return _value;
      }
    }

  private:
    typename std::conditional<is_void, bool, Return_type>::type _value;
  };
}

template <
    typename Value_type,
    size_t writing_state_sleep_mcs = 1,
    size_t writing_cell_sleep_mcs = 0>
class Lock_free_cell
{
  struct Cell
  {
    mutable std::atomic<short> _readers_counter{0};
    std::atomic<bool> _is_writing;

    Value_type _value;
  };

  static constexpr size_t _copies_number = 2;

private:
  template <typename Return_type>
  Return_type write_to_cell(size_t index, std::function<Return_type (Value_type&)> function,
                            std::atomic<size_t>* _wait_counter = nullptr)
  {
    Cell& current_write_reference = _elements[index % _copies_number];
    current_write_reference._is_writing.store(true);

    while (current_write_reference._readers_counter.load() != 0)
    {

      std::this_thread::yield();
      // std::this_thread::sleep_for(std::chrono::microseconds(writing_cell_sleep_mcs));
      if (_wait_counter)
      {
        _wait_counter->fetch_add(1);
      }
    }

    BOOST_SCOPE_EXIT_ALL(&current_write_reference)
        {
          current_write_reference._is_writing.store(false);
        };

    return function(current_write_reference._value);
  }

public:
  Lock_free_cell() :
      _read_cell_pointer_id(0),
      _is_writing_state(ATOMIC_FLAG_INIT),
      _elements({Cell{0, false, Value_type()}, Cell{0, false, Value_type()}})
  {  }

  explicit Lock_free_cell(const Value_type& value) :
      _read_cell_pointer_id(0),
      _is_writing_state(ATOMIC_FLAG_INIT),
      _elements({Cell{0, false, value}, Cell{0, false, value}})
  {  }

  template <typename Return_type>
  Return_type read(std::function<Return_type (const Value_type&)> function) const
  {
    while (true)
    {
      const Cell& cell_reference = _elements[_read_cell_pointer_id.load() % _copies_number];
      cell_reference._readers_counter.fetch_add(1);
      if (cell_reference._is_writing.load())
      {
        cell_reference._readers_counter.fetch_sub(1);
        continue;
      }

      BOOST_SCOPE_EXIT_ALL(&cell_reference)
          {
            cell_reference._readers_counter.fetch_sub(1);
          };

      return function(cell_reference._value);
    }
  }

  template <typename Return_type>
  Return_type write(std::function<Return_type (Value_type&)> function,
                    std::atomic<size_t>* _wait_counter = nullptr)
  {
    while (_is_writing_state.test_and_set(std::memory_order_acquire))
    {
      std::this_thread::sleep_for(std::chrono::microseconds(writing_state_sleep_mcs));
      if (_wait_counter)
      {
        _wait_counter->fetch_add(1);
      }
    }

    auto read_pointer_index = _read_cell_pointer_id.load();

    write_to_cell(read_pointer_index + 1, function, _wait_counter);

    ++_read_cell_pointer_id;

    auto& flag_reference = _is_writing_state;
    BOOST_SCOPE_EXIT_ALL(&flag_reference)
        {
          flag_reference.clear(std::memory_order_release);
        };

    return write_to_cell(read_pointer_index, function, _wait_counter);
  }

private:
  std::atomic<unsigned char> _read_cell_pointer_id;

  std::atomic_flag _is_writing_state;

  std::array<Cell, _copies_number> _elements;
};