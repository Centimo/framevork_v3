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

    Vector& operator * (const Value_type& multiplier)
    {
      for (size_t i = 0; i < dimensions_number; ++i)
      {
        (*this)[i] *= multiplier;
      }

      return *this;
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

    template <typename Second_value_type>
    Vector operator + (const Vector<Second_value_type>& second_term)
    {
      for (size_t i = 0; i < dimensions_number; ++i)
      {
        (*this)[i] += second_term[i];
      }

      return *this;
    }
  };

  using Radius_vector = Vector<float>;
  using Position_vector = Vector<float>;

  constexpr static size_t _particles_pack_size = 64;
  constexpr static float _acceleration_scale = 0.9;
  constexpr static Radius_vector _acceleration = { 0.0, 10.0 }; // for y - is up, + is down

  struct Particle
  {
    Position_vector _position;
    Radius_vector _velocity;
    size_t _lifetime;

    Particle move(size_t delta_t_ms) const
    {
      float delta_t_s = static_cast<float>(delta_t_ms) / 1000;

      return Particle{
        _position + _velocity * delta_t_s,
        (_velocity + _acceleration * delta_t_s) * std::pow(_acceleration_scale, delta_t_s),
        _lifetime + delta_t_ms
      };
    }
  };


  using Particles_pack = std::array<Particle, _particles_pack_size>;
  using Explosion = Position_vector;
};