#include <memory>
#include <math.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

#include "nvToolsExt.h"

#include "Engine.h"

#include "test.h"


// Just some hints on implementation
// You could remove all of them

static std::atomic<float> globalTime;
static volatile bool workerMustExit = false;

static std::unique_ptr<Engine> engine = nullptr;
static std::atomic<size_t> render_counter;


void WorkerThread(void)
{
  nvtxRangePush(__FUNCTION__);
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
      //std::cout << engine->get_debug_data() << std::endl;
      std::cout << "PPS: " << engine->get_process_counter() << std::endl;
      std::cout << "FPS: " << render_counter.load() << std::endl;

      render_counter.store(0);
      counter = 0;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ++counter;

		nvtxRangePop();
	}
}

void test::init(void)
{
  engine.reset(new Engine({ test::SCREEN_WIDTH, test::SCREEN_HEIGHT }));

	std::thread workerThread(WorkerThread);
	workerThread.detach(); // Glut + MSVC = join hangs in atexit()
}

void test::term(void)
{
	workerMustExit = true;
  engine.release();
}

void test::render(void)
{
  render_counter.fetch_add(1);

  engine->call_function_for_all_particles(
    [](size_t x_coordinate, size_t y_ccordinate, size_t lifetime_ms)
    {
      platform::drawPoint(
        static_cast<float>(x_coordinate), 
        static_cast<float>(SCREEN_HEIGHT - y_ccordinate), 
        1.0f - 0.6f * std::log10f(lifetime_ms / 100) / (1 + std::log10f(lifetime_ms / 100)),
        0.2f + 0.2f * std::log10f(lifetime_ms / 100) / (1 + std::log10f(lifetime_ms / 100)),
        0.0f + 0.4f * std::log10f(lifetime_ms / 100) / (1 + std::log10f(lifetime_ms / 100)),
        1.0f - 0.4f * std::log10f(lifetime_ms / 100) / (1 + std::log10f(lifetime_ms / 100)));
    }
  );
}

void test::update(int dt)
{
	float time = globalTime.load();
	globalTime.store(time + dt);
}

void test::on_click(int x, int y)
{
  engine->send_user_explosion(x, y);
}