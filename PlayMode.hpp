#include "PPU466.hpp"
#include "Mode.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <string>
#include <functional>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//some weird background animation:
	float background_fade = 0.0f;

	//----- drawing handled by PPU466 -----

	PPU466 ppu;

	// ----- player -----

	//player position:
	glm::vec2 player_at = glm::vec2(0.0f);

	//player sprites
    int player_sprites[4];

	// ----- enemy -----
	struct Enemy {
		// movement
		glm::vec2 pos;
		float velocity;
		std::function<void(Enemy&, float)> enemy_move;
		float base_x = 0.0f; 
		float time = 0.0f;

		// sprites
		std::vector<int> tiles;
		int sprite_start = 0;

		//attack
		float shoot_timer = 0.0f;
		float shoot_interval = 1.5f;
	};

	std::vector<int> enemy1_tiles_index;
	std::vector<int> enemy2_tiles_index;
	std::vector<Enemy> enemies1;
    std::vector<Enemy> enemies2;
	int maxEnemies = 3;
	float enemy_spawn_timer = 0.0f;
	float enemy_spawn_interval = 1.0f;
	void spawn_enemy();

	// ----- projectile -----
	struct Projectile {
		glm::vec2 pos;
		float velocity;
	};

	std::vector<Projectile> player_projectiles;
	std::vector<Projectile> enemy_projectiles;

	float projectile_timer = 0.0f;
	float projectile_interval = 0.7f;
	const int MaxProjectiles = 12;
	const int ProjectileSpriteStart = 33;
	void spawn_projectile(glm::vec2 pos, glm::vec2 vel);

	// ----- Helper Functions To Load PNGs -----

	// Build a color palette from the given png
	int build_palette(const std::vector<glm::u8vec4> &pixels, int start_index);

	// Slice the image to 8x8 tiles from the given png
	void build_tiles(int tile_idx, int palette_idx, const glm::u8vec4 *pixels, int x0, int y0, int image_width, int image_height);

	// Return a vector of tile indices loaded from the png
	std::vector<int> load_tiles(const std::string &filename, int tile_start, int palette_start);
};