//
// Created by centimo on 23.10.2019.
//

#include "Engine.h"

void Engine::send_explosion(const size_t x_coordinate, const size_t y_coordinate)
{
  size_t thread_index = _threads_cyclic_index.fetch_add(1) % _threads.size();
  _threads[thread_index]->_explosions.push({x_coordinate, y_coordinate});
}

void Engine::call_function_for_all_particles(const std::function<void(size_t, size_t)>& function) const
{
  for (const auto& thread : _threads)
  {
    thread->_particles.
      call_function_for_all_nonempty_elements(
        [&function] (const World_t::Particle& particle)
        {
          function(particle._position[0], particle._position[1]);
        });
  }
}

void Engine::thread_worker(Thread_data& thread_data)
{
  size_t current_global_time_ms = 0;
  std::array< std::optional< World_t::Particles_pack >, _max_explosions_pops_per_cycle> particles_packs;

  while (!_is_stop.load())
  {
    // TODO: test
    long long delta_t_ms = _current_global_time_ms.load() - current_global_time_ms;
    if (delta_t_ms < 0)
    {
      delta_t_ms = std::numeric_limits<size_t>::max() - std::abs(delta_t_ms);
    }

    thread_data.process_particles(delta_t_ms);

    for (size_t i = 0; i < _max_explosions_pops_per_cycle; ++i)
    {
      particles_packs[i] = thread_data.make_pack_from_explosion(thread_data._explosions.pop());

      if (!particles_packs[i])
      {
        break;
      }
    }

    thread_data.add_particles(particles_packs);

    current_global_time_ms = _current_global_time_ms.load();
  }
}

std::optional<Engine::World_t::Particles_pack>
    Engine::Thread_data::make_pack_from_explosion(const std::optional<World_t::Explosion>& explosion)
{
  if (!explosion)
  {
    return std::nullopt;
  }

  World_t::Particles_pack result;

  for (World_t::Particle& particle : result)
  {
    particle = { explosion.value(),
                 {
                   _random_generator.get_random_double_from_weibull(_scale_for_weibull),
                   _random_generator.get_random_double_from_weibull(_scale_for_weibull)
                 },
                 0
               };
  }

  return result;
}

void Engine::update_global_time(size_t delta_t_ms)
{
  _current_global_time_ms.fetch_add(delta_t_ms);
}

Engine::Engine(size_t threads_number) 
{
  if (threads_number == 0)
  {
    threads_number = std::thread::hardware_concurrency();
  }

  if (threads_number == 0)
  {
    threads_number = 1;
  }

  _threads.reserve(threads_number);
  const size_t particles_number_per_thread = _total_particles_number / threads_number;
  const size_t threads_with_additional_particle = _total_particles_number % threads_number;

  for (size_t i = 0; i < threads_with_additional_particle; i++)
  {
    _threads.emplace_back(new Engine::Thread_data(*this, particles_number_per_thread + 1, _parts_number_per_thread));
  }

  for (size_t i = threads_with_additional_particle; i < threads_number; i++)
  {
    _threads.emplace_back(new Engine::Thread_data(*this, particles_number_per_thread, _parts_number_per_thread));
  }
}

void Engine::Thread_data::process_particles(const size_t delta_t_ms)
{
  _particles.call_function_for_all_elements(
      [this, &delta_t_ms]
        (
          const std::optional<std::reference_wrapper<const World_t::Particle> >& particle_optional,
          bool& is_stop
        )
        -> std::optional<World_t::Particle>
      {
        if (!particle_optional)
        {
          return std::nullopt;
        }

        const World_t::Particle& particle = particle_optional.value();

        if (_random_generator.get_random_bool_from_probability(_probability_of_disappearing))
        {
          return std::nullopt;
        }
        else if (_random_generator.get_random_bool_from_probability(_probability_of_exploding))
        {
          _engine.send_explosion(particle._position[0], particle._position[1]);
          return std::nullopt;
        }
        else
        {
          auto result = particle.move(delta_t_ms);
          _particles_counter.add_particle(result._lifetime);
          return result;
        }
      });
}

void Engine::Thread_data::add_particles(const Particles_packs_array& particles_packs)
{
  size_t particles_number = 0;
  for (const auto& pack : particles_packs)
  {
    if (pack)
    {
      particles_number += World_t::_particles_pack_size;
    }
    else
    {
      break;
    }
  }

  size_t mininmal_lifetime_border = 0;
  {
    int number_of_particles_to_replace = particles_number - _particles.ger_empty_elements_number();
    if (number_of_particles_to_replace > 0)
    {
      mininmal_lifetime_border =
          _particles_counter.
              get_minimal_lifetime_for_oldest_particles_number(number_of_particles_to_replace);
    }
  }

  _particles.call_function_for_all_elements(
      [this, &particles_packs, &particles_number, &mininmal_lifetime_border]
          (
            const std::optional<std::reference_wrapper<const World_t::Particle> >& particle_optional,
            bool& is_stop
          )
          -> std::optional<World_t::Particle>
      {
        if (!particles_number)
        {
          is_stop = true;
          return particle_optional;
        }

        const auto& result =
            particles_packs
              [particles_number / _max_explosions_pops_per_cycle].value()
                [particles_number % _max_explosions_pops_per_cycle];

        if (!particle_optional
            || particle_optional.value().get()._lifetime >=  mininmal_lifetime_border)
        {
          --particles_number;
          return result;
        }
        else
        {
          return particle_optional;
        }
      });
}

Engine::Thread_data::Thread_data(Engine& engine, size_t particles_number, size_t parts_number)
  : _engine(engine),
    _particles(particles_number, parts_number),
    _thread(&Engine::thread_worker, &engine, std::ref(*this))
{ }

Engine::Particles_by_lifetime_counter::Particles_by_lifetime_counter()
{
  for (size_t i = 0; i < _counter.size(); ++i)
  {
    _counter[i] = { i * (500 + (size_t)std::pow(1.5, 2*i)), 0 };
  }
}

void Engine::Particles_by_lifetime_counter::add_particle(const size_t lifetime_ms)
{
  for (size_t i = _counter.size() - 1; i >= 0; --i)
  {
    if (lifetime_ms >= _counter[i][0])
    {
      ++_counter[i][1];
    }
  }
}

size_t Engine::Particles_by_lifetime_counter::
         get_minimal_lifetime_for_oldest_particles_number(const size_t particles_number)
{
  int particles_remaining = particles_number;
  for (size_t i = _counter.size() - 1; i >= 0; --i)
  {
    particles_remaining -= _counter[i][1];

    if (particles_remaining <= 0)
    {
      return _counter[i][0];
    }
  }

  return 0;
}

bool Engine::Random_generator::get_random_bool_from_probability(double probability)
{
  std::bernoulli_distribution distribution(probability);
  return distribution(_generator);
}

double Engine::Random_generator::get_random_double_from_weibull(double scale)
{
  std::bernoulli_distribution bernoulli_distribution(0.5);
  std::weibull_distribution<double> weibull_distribution(2.0, scale);
  return (bernoulli_distribution(_generator) ? 1 : -1) * weibull_distribution(_generator);
}
