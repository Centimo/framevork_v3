namespace test
{
	const float SCREEN_WIDTH = 1024;
	const float SCREEN_HEIGHT = 768;
  const size_t time_period = 10;

  // extern std::unique_ptr<Engine> engine;

	void render(void); // Only platform::drawPoint should be used
	void update(int dt); // dt in milliseconds
	void on_click(int x, int y); // x, y - in pixels

	void init(void);
	void term(void);
};

namespace platform
{
	void drawPoint(float x, float y, float r, float g, float b, float a);
};

//std::unique_ptr<Engine> test::engine = nullptr;