//
// Created by centimo on 23.10.2019.
//

#pragma once

#include <vector>
#include <limits>
#include <random>
#include <cmath>

#include "Atomic_queue.hpp"
#include "World.hpp"
#include "OROW_vector_copy_2.hpp"


class Engine
{
  constexpr static size_t _dimensions_number = 2;
  constexpr static size_t _total_particles_number = 2048 * 64;
  constexpr static size_t _parts_number_per_thread = 32;
  constexpr static size_t _max_explosions_pops_per_cycle = 3;
  constexpr static double _probability_of_disappearing = 0.2;
  constexpr static double _probability_of_exploding = 0.1;
  constexpr static double _scale_for_weibull = 2.0;

  using World_t = World<_dimensions_number>;
  using Particles_packs_array = std::array< std::optional<World_t::Particles_pack>, _max_explosions_pops_per_cycle>;

  class Particles_by_lifetime_counter
  {
    std::array<std::array<size_t, 2>, 10> _counter;

  public:
    Particles_by_lifetime_counter();

    void add_particle(size_t lifetime_ms);
    size_t get_minimal_lifetime_for_oldest_particles_number(size_t particles_number);
  };

  class Random_generator
  {
    std::mt19937 _generator;

  public:
    bool get_random_bool_from_probability(double probability);
    double get_random_double_from_weibull(double scale);
  };

  struct Thread_data
  {
    OROW_vector<World_t::Particle> _particles;
    Atomic_queue<World_t::Explosion> _explosions;
    Engine& _engine;
    Particles_by_lifetime_counter _particles_counter;
    Random_generator _random_generator;

    std::thread _thread;

    Thread_data(Engine& engine, size_t particles_number, size_t parts_number);
    Thread_data& operator = (const Thread_data&) = default;

    void process_particles(size_t delta_t_ms);
    void add_particles(const Particles_packs_array& particles_packs);
    std::optional<World_t::Particles_pack> make_pack_from_explosion(const std::optional<World_t::Explosion>& explosion);
  };

public:

  Engine(size_t threads_number = 0);

  void send_explosion(size_t x_coordinate, size_t y_coordinate);
  void call_function_for_all_particles(const std::function<void (size_t, size_t)>& function) const;
  void update_global_time(size_t delta_t_ms);

private:
  void thread_worker(Thread_data& thread_data);

private:
  std::vector<std::unique_ptr<Thread_data> > _threads;
  std::atomic<size_t> _threads_cyclic_index;

  std::atomic<bool> _is_stop;
  std::atomic<size_t> _current_global_time_ms;
};


