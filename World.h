//
// Created by centimo on 23.10.2019.
//

#pragma once

#include <array>

template <size_t dimensions_number>
struct World
{
  struct Radius_vector : public std::array<size_t, dimensions_number> // TODO fix type (size_t to double?)
  {

  };

  constexpr static Radius_vector _acceleration = { 0, 1 };

  struct Particle
  {
    Radius_vector _position;
    Radius_vector _velocity;
    size_t _lifetime;

    Particle move(size_t delta_t_ms) const;
  };


  using Particles_pack = std::array<Particle, 64>;
  using Explosion = Radius_vector;
};

