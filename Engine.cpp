//
// Created by centimo on 23.10.2019.
//

#include "Engine.h"

void Engine::send_explosion(const size_t x_coordinate, const size_t y_coordinate)
{
  size_t thread_index = _threads_cyclic_index.fetch_add(1) % _threads.size();
  _threads[thread_index]->_explosions.push({ static_cast<float>(x_coordinate), static_cast<float > (y_coordinate) });
}

void Engine::send_user_explosion(const size_t x_coordinate, const size_t y_coordinate)
{
  size_t thread_index = _threads_cyclic_index.fetch_add(1) % _threads.size();
  _threads[thread_index]->_explosions_from_user.push({ static_cast<float>(x_coordinate), static_cast<float> (y_coordinate) });
}

void Engine::call_function_for_all_particles(const std::function<void(size_t, size_t, size_t)>& function) const
{
  for (const auto& thread : _threads)
  {
    thread->_particles.
      call_function_for_all_nonempty_elements(
        [&function] (const World_t::Particle& particle)
        {
          function(particle._position[0], particle._position[1], particle._lifetime);
        });
  }
}

void Engine::thread_worker(Thread_data& thread_data)
{
  size_t current_global_time_ms = 0;
  std::array< std::optional< Particles_pack>, _max_explosions_pops_per_cycle> particles_packs;

  while (!_is_stop.load())
  {
    long long delta_t_ms = _current_global_time_ms.load() - current_global_time_ms;
    current_global_time_ms = _current_global_time_ms.load();

    if (delta_t_ms < 0)
    {
      delta_t_ms = std::numeric_limits<size_t>::max() - std::abs(delta_t_ms);
    }
    
    if (delta_t_ms < 10)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }  

    size_t explosions_buffer_iterator = 0;
    for (; explosions_buffer_iterator < thread_data._explosions_buffer.size(); ++explosions_buffer_iterator)
    {
      thread_data._explosions_buffer[explosions_buffer_iterator] = 
        thread_data.make_pack_from_explosion(thread_data._explosions_from_user.pop());

      if (!thread_data._explosions_buffer[explosions_buffer_iterator])
      {
        break;
      }
    }

    for (; explosions_buffer_iterator < thread_data._explosions_buffer.size(); ++explosions_buffer_iterator)
    {
      thread_data._explosions_buffer[explosions_buffer_iterator] =
        thread_data.make_pack_from_explosion(thread_data._explosions.pop());

      if (!thread_data._explosions_buffer[explosions_buffer_iterator])
      {
        break;
      }
    }

    thread_data.process_particles(delta_t_ms);
  }
}

std::optional<Engine::Particles_pack>
    Engine::Thread_data::make_pack_from_explosion(const std::optional<World_t::Explosion>& explosion)
{
  if (!explosion)
  {
    return std::nullopt;
  }

  Particles_pack result;

  for (World_t::Particle& particle : result)
  {
    particle = { explosion.value(),
                 {
                   _random_generator.get_random_float_from_weibull(_scale_for_weibull),
                   _random_generator.get_random_float_from_weibull(_scale_for_weibull)
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

void Engine::change_borders(const World_t::Position_vector& new_borders)
{
  for (size_t i = 0; i < _dimensions_number; ++i)
  {
    _borders[i].store(new_borders[i]);
  }
}

size_t Engine::get_particles_number()
{
  size_t result = 0;
  for (const auto& thread : _threads)
  {
    result += thread->_particles.size();
  }

  return result;
}

std::string Engine::get_debug_data()
{
  std::string result = std::to_string(get_particles_number()) + "\n";

  for (size_t i = 0; i < _threads.size(); ++i)
  {
    auto& thread = _threads[i];
    result += "Thread: " + std::to_string(i);
    result += ", explosion size: " + std::to_string(thread->_explosions.size());
    result += ", particles size: " + std::to_string(thread->_particles.size());
    result += ", empty number: " + std::to_string(thread->_particles.empty_number());
    result += "\n";
  }


  return result;
}

Engine::Engine(const World_t::Position_vector& borders, size_t threads_number) : 
  _is_stop(false)
{
  change_borders(borders);

  if (threads_number == 0)
  {
    threads_number = std::thread::hardware_concurrency() - 1;
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

  _threads_cyclic_index.store(0);
}


Engine::~Engine()
{
  _is_stop.store(true);
  for (auto& thread : _threads)
  {
    if (thread->_thread.joinable())
    {
      thread->_thread.join();
    }
  }
}

void Engine::Thread_data::process_particles(const size_t delta_t_ms)
{
  size_t particles_number = 0;
  for (const auto& pack : _explosions_buffer)
  {
    if (pack)
    {
      particles_number += _particles_pack_size;
    }
    else
    {
      break;
    }
  }

  if (!particles_number && _particles.is_empty())
  {
    return;
  }

  size_t mininmal_lifetime_border = std::numeric_limits<size_t>::max();
  {
    int number_of_particles_to_replace = particles_number - _particles.empty_number();
    if (number_of_particles_to_replace > 0)
    {
      mininmal_lifetime_border =
        _particles_counter.
          get_minimal_lifetime_for_oldest_particles_number(number_of_particles_to_replace);
    }
  }

  _particles.call_function_for_all_elements(
      [this, delta_t_ms, &particles_number, mininmal_lifetime_border]
        (
          const std::optional<std::reference_wrapper<const World_t::Particle> >& particle_optional
        )
        -> std::optional<World_t::Particle>
      {
        if (!particle_optional && !particles_number)
        {
          return std::nullopt;
        }

        if (particles_number)
        {
          const auto& result =
            _explosions_buffer
              [(particles_number - 1) / _particles_pack_size].value()
              [(particles_number - 1) % _particles_pack_size];


          if (!particle_optional
              || particle_optional.value().get()._lifetime >= mininmal_lifetime_border)
          {
            --particles_number;
            return result;
          }
        }

        const World_t::Particle& particle = particle_optional.value();

        if (_random_generator.get_random_bool_from_probability(_probability_of_disappearing * delta_t_ms))
        {
          return std::nullopt;
        }
        else if (_random_generator.get_random_bool_from_probability(_probability_of_exploding * delta_t_ms))
        {
          _engine.send_explosion(particle._position[0], particle._position[1]);
          return std::nullopt;
        }
        else
        {
          auto result = particle.move(delta_t_ms);
          
          for (size_t i = 0; i < _dimensions_number; ++i)
          {
            if (result._position[i] < 0.0 || result._position[i] > _engine._borders[i].load())
            {
              return std::nullopt;
            }
          }

          _particles_counter.add_particle(result._lifetime);
          return result;
        }
      });
}

Engine::Thread_data::Thread_data(Engine& engine, size_t particles_number, size_t parts_number)
  : _engine(engine),
    _explosions_from_user(static_cast<size_t>(std::log2(particles_number / 160)) + 1),
    _explosions(static_cast<size_t>(std::log2(particles_number / 40)) + 1),
    _explosions_buffer(_explosions_buffer_size),
    _particles(particles_number, parts_number),
    _thread(&Engine::thread_worker, &engine, std::ref(*this))
{ }

Engine::Particles_by_lifetime_counter::Particles_by_lifetime_counter()
{
  for (size_t i = 0; i < _counter.size(); ++i)
  {
    _counter[i] = { i * (10 + (size_t)std::pow(1.5, 2*i)), 0 };
  }
}

void Engine::Particles_by_lifetime_counter::add_particle(const size_t lifetime_ms)
{
  for (int i = _counter.size() - 1; i >= 0; --i)
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
  for (int i = _counter.size() - 1; i >= 0; --i)
  {
    particles_remaining -= _counter[i][1];

    if (particles_remaining <= 0)
    {
      return _counter[i][0];
    }
  }

  return 0;
}

Engine::Random_generator::Random_generator() :
  _generator(_random_device())
{ }

bool Engine::Random_generator::get_random_bool_from_probability(double probability)
{
  std::bernoulli_distribution distribution(probability);
  return distribution(_generator);
}

float Engine::Random_generator::get_random_float_from_weibull(double scale)
{
  std::bernoulli_distribution bernoulli_distribution(0.5);
  std::weibull_distribution<float> weibull_distribution(2.5, scale);
  return (bernoulli_distribution(_generator) ? 1 : -1) * weibull_distribution(_generator);
}
