//
// Created by centimo on 12.09.2019.
//

#pragma once


#include <atomic>
#include <array>
#include <thread>
#include <optional>


template <
    typename Value_type,
    size_t writing_state_sleep_mcs = 10,
    size_t writing_cell_sleep_mcs = 10>
class RCU_cell_light
{
  struct Cell
  {
    mutable std::atomic<short> _readers_counter{0};
    std::atomic<bool> _is_writing;

    Value_type _value;

    Cell& operator = (const Cell& source)
    {
      _readers_counter.store(0);
      _is_writing.store(false);
      _value = source._value;
      return *this;
    }

    /*
    RCU_cell_light& operator = (const RCU_cell_light& source_cell)
    {
      _read_cell_pointer_id.store(0);
      _is_writing_state.clear();
      _elements.fill(Cell{ 0, false, source_cell.read() });
    }
    */
  };

  static constexpr size_t _copies_number = 2;

private:
  void write_to_cell(size_t index, const Value_type& value, std::atomic<size_t>* _wait_counter = nullptr)
  {
    Cell& current_write_reference = _elements[index % _copies_number];
    current_write_reference._is_writing.store(true);

    while (current_write_reference._readers_counter.load() != 0)
    {

      //std::this_thread::yield();
      std::this_thread::sleep_for(std::chrono::microseconds(writing_cell_sleep_mcs));
      if (_wait_counter)
      {
        _wait_counter->fetch_add(1);
      }
    }

    current_write_reference._value = value;

    return current_write_reference._is_writing.store(false);
  }

public:
  RCU_cell_light() :
      _read_cell_pointer_id(0),
      _is_writing_state{ ATOMIC_FLAG_INIT }
  {
    _elements.fill(Cell{0, false, Value_type()});
  }

  explicit RCU_cell_light(const Value_type& value) :
      _read_cell_pointer_id(0),
      _is_writing_state{ ATOMIC_FLAG_INIT }
  {
    _elements.fill(Cell{ 0, false, value });
  }


  Value_type read() const
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

      Value_type result = cell_reference._value;

      cell_reference._readers_counter.fetch_sub(1);

      return result;
    }
  }

  void write(const Value_type& value, std::atomic<size_t>* _wait_counter = nullptr)
  {
    while (_is_writing_state.test_and_set(std::memory_order::memory_order_relaxed))
    {
      std::this_thread::sleep_for(std::chrono::microseconds(writing_state_sleep_mcs));
    }

    auto read_pointer_index = _read_cell_pointer_id.load();

    write_to_cell(read_pointer_index + 1, value, _wait_counter);

    ++_read_cell_pointer_id;

    return _is_writing_state.clear();
  }

private:
  std::atomic<unsigned char> _read_cell_pointer_id;

  std::atomic_flag _is_writing_state;

  std::array<Cell, _copies_number> _elements;
};