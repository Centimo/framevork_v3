//
// Created by centimo on 23.10.2019.
//

#pragma once

#include <array>

template <size_t dimensions_number>
struct World
{
  template <typename Value_type>
  struct Vector : public std::array<Value_type, dimensions_number> // TODO fix type (size_t to double?)
  { 
    friend Vector
      operator * (
        const Vector& value,
        const Value_type& multiplier)
    {
      Vector<Value_type> result;
      for (size_t i = 0; i < dimensions_number; ++i)
      {
        result[i] = value[i] * multiplier;
      }

      return result;
    };

    template <typename Second_value_type>
    friend Vector
      operator + (
        const Vector<Value_type>& first_term,
        const Vector<Second_value_type>& second_term)
    {
      Vector<Value_type> result;
      for (size_t i = 0; i < dimensions_number; ++i)
      {
        result[i] = first_term[i] + second_term[i];
      }

      return result;
    }
  };

  using Radius_vector = Vector<double>;
  using Position_vector = Vector<size_t>;

  constexpr static size_t _particles_pack_size = 64;
  constexpr static Radius_vector _acceleration = { 0.0, -1.0 };

  struct Particle
  {
    Position_vector _position;
    Radius_vector _velocity;
    size_t _lifetime;

    Particle move(size_t delta_t_ms) const
    {
      return Particle{
        _position + _velocity * static_cast<double>(delta_t_ms / 1000),
        _velocity + _acceleration * static_cast<double>(delta_t_ms / 1000),
        _lifetime + delta_t_ms
      };
    }
  };


  using Particles_pack = std::array<Particle, _particles_pack_size>;
  using Explosion = Position_vector;
};