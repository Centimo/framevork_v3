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

  };

  using Radius_vector = Vector<double>;
  using Position_vector = Vector<size_t>;

  constexpr static size_t _particles_pack_size = 64;
  constexpr static Radius_vector _acceleration = { 0, 1 };

  struct Particle
  {
    Position_vector _position;
    Radius_vector _velocity;
    size_t _lifetime;

    Particle move(size_t delta_t_ms) const;
  };


  using Particles_pack = std::array<Particle, _particles_pack_size>;
  using Explosion = Position_vector;
};

