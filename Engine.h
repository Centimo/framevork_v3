//
// Created by centimo on 23.10.2019.
//

#pragma once

#include <vector>
#include <limits>
#include <random>
#include <cmath>
#include <string>

#include "Containers/Atomic_queue.hpp"
#include "Containers/OROW_vector.hpp"

#include "World.hpp"


class Engine
{
  constexpr static size_t _dimensions_number = 2;

  using World_t = World<_dimensions_number>;

  constexpr static size_t _particles_pack_size = 64;
  using Particles_pack = std::array<World_t::Particle, _particles_pack_size>;

  constexpr static size_t _total_particles_number = 4096 * _particles_pack_size;
  constexpr static size_t _parts_number_per_thread = 16;
  constexpr static size_t _max_cycles_for_explosions_receiving = 5;
  constexpr static size_t _explosions_buffer_size = 40;
  constexpr static double _probability_of_disappearing = 0.002;
  constexpr static double _probability_of_exploding = 0.00004;
  constexpr static double _scale_for_weibull = 40.0; // random speed scale


  class Particles_by_lifetime_counter
  {
    std::array<size_t, 10> _lifetime_borders;
    std::array<size_t, 10> _counter;

  public:
    Particles_by_lifetime_counter();

    void add_particle(size_t lifetime_ms);
    size_t get_minimal_lifetime_for_oldest_particles_number(size_t particles_number);
    void clear();
  };

  class Random_generator
  {
    std::random_device _random_device;
    std::mt19937 _generator;

  public:
    Random_generator();

    bool get_random_bool_from_probability(double probability);
    float get_random_float_from_weibull(double scale);
  };

  struct Thread_data
  {
    OROW_vector<World_t::Particle> _particles;
    Atomic_queue<World_t::Explosion> _explosions;
    Atomic_queue<World_t::Explosion> _explosions_from_user;
    Engine& _engine;
    Particles_by_lifetime_counter _particles_counter;
    Random_generator _random_generator;

    std::vector< std::optional< Particles_pack> > _explosions_buffer;

    std::thread _thread;

    Thread_data(Engine& engine, size_t threads_number, size_t particles_number, size_t parts_number);
    Thread_data& operator = (const Thread_data&) = default;

    void process_particles(size_t delta_t_ms);
    std::optional<Particles_pack> make_pack_from_explosion(const std::optional<World_t::Explosion>& explosion);
  };

public:

  Engine(const World_t::Position_vector& borders, size_t threads_number = 0);
  ~Engine();

  void send_user_explosion(size_t x_coordinate, size_t y_coordinate);
  void call_function_for_all_particles(const std::function<void (size_t, size_t, size_t)>& function) const;
  void update_global_time(size_t delta_t_ms);
  void change_borders(const World_t::Position_vector& new_borders);
  size_t get_particles_number();
  std::string get_debug_data();

private:
  void send_explosion(size_t x_coordinate, size_t y_coordinate);
  void thread_worker(Thread_data& thread_data);

private:
  std::vector<std::unique_ptr<Thread_data> > _threads;
  std::atomic<size_t> _threads_cyclic_index;

  std::atomic<bool> _is_stop;
  std::atomic<size_t> _current_global_time_ms;
  std::array<std::atomic<float>, _dimensions_number> _borders;
};


