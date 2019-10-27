//
// Created by centimo on 24.10.2019.
//

#pragma once

#include <atomic>
#include <array>


namespace utils
{
  template <size_t value, size_t power>
  struct Power
  {
    enum
    {
      result = value * Power<value, power - 1>::result
    };
  };

  template <size_t value>
  struct Power<value, 0>
  {
    enum
    {
      result = 1
    };
  };

  template <size_t value>
  struct Power<value, 1>
  {
    enum
    {
      result = value
    };
  };

  /*
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
   */
}