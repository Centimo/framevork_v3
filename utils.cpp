#include "utils.h"

size_t utils::two_to_the_power_of(size_t exponenta)
{
  size_t result = 1;
  for (size_t i = 0; i < exponenta; ++i)
  {
    result *= 2;
  }

  return result;
};