//
// Created by centimo on 25.10.2019.
//

#pragma once

#include <atomic>
#include <array>

namespace Atomic_meta
{

  template <size_t compare_with_value>
  struct Swap
  {
    template <size_t change_to_value>
    struct to
    {
      constexpr static std::array<size_t, 2> pair_of_values{compare_with_value, change_to_value};
    };
  };

  class Compare
  {
  public:
    Compare(std::atomic<size_t>& value) : _value(value) {};

    template <class... Arguments>
    size_t _with_one_of_and_swap()
    {
      constexpr std::array<std::array<size_t, 2>, sizeof...(Arguments)> pairs = {Arguments::pair_of_values...};

      size_t result = 0;
      bool stop = false;

      while (!stop)
      {
        for (const auto& pair : pairs)
        {
          result = pair[0];
          if (_value.compare_exchange_strong(result, pair[1]))
          {
            stop = true;
            break;
          }
        }
      }

      return result;
    }

  private:
    std::atomic<size_t>& _value;
  };

  Compare compare(std::atomic<size_t>& value)
  {
    return Compare(value);
  }
}