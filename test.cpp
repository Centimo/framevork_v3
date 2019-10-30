#include <memory>
#include <math.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

#include "Engine.h"

#include "test.h"


// Just some hints on implementation
// You could remove all of them

static std::atomic<float> globalTime;
static volatile bool workerMustExit = false;

static std::unique_ptr<Engine> engine = nullptr;
static std::vector<World<2>::Position_vector> global_particles;
static std::atomic<size_t> render_counter;
static size_t fps;

// some code

void WorkerThread(void)
{
  //nvtxRangePush(__FUNCTION__);
  auto start_time = std::chrono::high_resolution_clock::now();

  size_t counter = 0;

	while (!workerMustExit)
	{
    size_t time_delta_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time
      ).count();

    if (time_delta_ms >= test::time_period)
    {
      engine->update_global_time(time_delta_ms);
      start_time = std::chrono::high_resolution_clock::now();
    }

    if (counter > 1000)
    {
      fps = render_counter.load();
      render_counter.store(0);

      std::cout << engine->get_debug_data() << std::endl;
      std::cout << "FPS: " << fps << std::endl;

      counter = 0;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ++counter;

		// nvtxRangePop();
	}
}


void test::init(void)
{
	// some code
  engine.reset(new Engine());
  global_particles.reserve(64*2048);

	std::thread workerThread(WorkerThread);
	workerThread.detach(); // Glut + MSVC = join hangs in atexit()

	// some code
}

void test::term(void)
{
	// some code

	workerMustExit = true;
  engine.release();

	// some code
}

void test::render(void)
{
  //std::cout << "Thread id: " << std::this_thread::get_id() << std::endl;
  //auto start_time = std::chrono::high_resolution_clock::now();

  render_counter.fetch_add(1);

  engine->call_function_for_all_particles(
    [](size_t x_coordinate, size_t y_ccordinate)
    {
      platform::drawPoint(static_cast<float>(x_coordinate), static_cast<float>(SCREEN_HEIGHT - y_ccordinate), 1.0, 1.0, 1.0, 1.0);
    }
  );

  // std::cout << "all_particles_number: " << all_particles_number << std::endl;
  //global_particles.clear();
 

  // std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count() << std::endl;
}

void test::update(int dt)
{
	// some code

	float time = globalTime.load();
	globalTime.store(time + dt);
 
  // std::cout << "Delta_t: " << std::this_thread::get_id() << std::endl;

	// some code
}

void test::on_click(int x, int y)
{
  std::cout << "x: " << x << ", y: " << y << std::endl;
  // std::cout << "Thread id: " << std::this_thread::get_id() << std::endl;
  engine->send_explosion(x, y);
  std::cout << "Particles: " << engine->get_particles_number() << std::endl;
}