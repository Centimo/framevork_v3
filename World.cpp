//
// Created by centimo on 23.10.2019.
//

#include "World.h"

template <size_t dimensions_number>
typename World<dimensions_number>::Particle World<dimensions_number>::Particle::move(size_t delta_t_ms) const
{
  return World::Particle{
    _position + _velocity * delta_t_ms,
    _velocity + _acceleration,
    _lifetime + delta_t_ms
  };
}
