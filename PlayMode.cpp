// Copilot is used to fix some syntax errors and auto formatting
// Enemy designs reference: Helldiver2
// Player designs reference: Touhou Project

#include "PlayMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

// for load_png():
#include "load_save_png.hpp"

#include <random>

// for debugging
#include <iostream>

// ----- Helper Functions To Load PNGs -----

// find the closest color in the palette for a given pixel
static int closest_palette_index(const glm::u8vec4 &pixel_ref, const PPU466::Palette &palette_ref) {
    for (int i = 1; i < 4; i++)
		if (palette_ref[i].a != 0 && pixel_ref == palette_ref[i]) return i;
    return 0;
}

// Build a color palette from the given png
int PlayMode::build_palette(const std::vector<glm::u8vec4> &pixels, int index) {
    glm::u8vec4 unique_colors[4] = {};
    int count = 1;

    for (auto &px : pixels) {
		// skip if pixel is transparent
        if (px.a == 0) continue;

		// add to palette only if the color doesn't exist in the palette
        if (std::find(unique_colors, unique_colors + count, px) == unique_colors + count)
            unique_colors[count++] = px;
    }

	// initialize palette
    ppu.palette_table[index] = {
        unique_colors[0],
        unique_colors[1],
        unique_colors[2],
        unique_colors[3]
    };

    return index;
}

// Slice the image to 8x8 tiles from the given png
void PlayMode::build_tiles(int tile_idx, int palette_idx, const glm::u8vec4 *pixels, int start_x, int start_y, int image_width, int image_height) {
	auto &tile = ppu.tile_table[tile_idx];
    for (int tile_row = 0; tile_row < 8; tile_row++) {
        uint8_t b0 = 0, b1 = 0;
        for (int tile_col = 0; tile_col < 8; tile_col++) {
			// pixel position in the image data
			// not sure why its upside down?
            int pixel_x = start_x + (7-tile_col);
            int pixel_y = start_y + (7-tile_row);

            glm::u8vec4 pixel_color = pixels[image_width * pixel_y + pixel_x];
            int palette_index = closest_palette_index(pixel_color, ppu.palette_table[palette_idx]);
            b0 |= (palette_index & 1) << (7-tile_col);
            b1 |= ((palette_index >> 1) & 1) << (7-tile_col);
        }
        tile.bit0[tile_row] = b0;
        tile.bit1[tile_row] = b1;
    }
}

// Return a vector of tile indices loaded from the png
std::vector<int> PlayMode::load_tiles(const std::string &filename, int tile_start, int palette_start) {
    glm::uvec2 size;
    std::vector<glm::u8vec4> data;
    load_png(filename, &size, &data, UpperLeftOrigin);

    int palette_idx = build_palette(data, palette_start);
    std::vector<int> tile_indices;
    int tile_idx = tile_start;

	for (int i = 0; i < (int) size.y / 8; i++)
	{
		for (int j = 0; j < (int) size.x / 8; j++)
		{
			int x0 = j * 8;
			int y0 = i * 8;

			build_tiles(tile_idx, palette_idx, data.data(), x0, y0, size.x, size.y);
			tile_indices.push_back(tile_idx);
			tile_idx++;
		}
	}

    return tile_indices;
}

// ----- Helper Functions To Manage Game Objects -----

// Spawn a random type of enemy
void PlayMode::spawn_enemy() {
    if ((int)enemies1.size() + (int)enemies2.size() >= maxEnemies) return;

    if (rand() % 2 == 0) {
		// enemy 1 (overseer)
		// move in a sine wave pattern
		// launch projectile to attack
		Enemy e;

		// movement
		e.pos = glm::vec2(rand() % (PPU466::ScreenWidth - 16), 200.0f);
		e.velocity = -60.0f;
		auto sine_wave = [](Enemy &e, float elapsed) {
			e.time += elapsed;
			float amplitude = 30.0f;
			float speed = 2.0f;
			e.pos.x = e.base_x + 128.0f + amplitude * std::sin(speed * e.time);
			e.pos.y += e.velocity * elapsed;
		};
		e.enemy_move = sine_wave;
		e.base_x = e.pos.x;

		// sprites
		e.tiles = enemy1_tiles_index;
		e.sprite_start = 4 + (int) enemies1.size() * 4;

		// attack
		e.shoot_interval = 0.5f;

		enemies1.push_back(e);
	} else {
		// enemy 2 (voteless)
		// move straight downwards
		// melee attack
		// can't be killed
		Enemy e;

		// movement
		e.pos = glm::vec2(rand() % (PPU466::ScreenWidth - 16), 200.0f);
		e.velocity = -200.0f;
		auto move_down = [](Enemy &e, float elapsed) {
			e.pos.y += e.velocity * elapsed;
		};
		e.enemy_move = move_down;

		// sprites
		e.tiles = enemy2_tiles_index;
		e.sprite_start = 4 + (int) maxEnemies * 4 + (int) enemies2.size() * 4;

		// attack
		e.shoot_interval = 10000.0f;
		enemies2.push_back(e);
	}
}

PlayMode::PlayMode() {
	//TODO:
	// you *must* use an asset pipeline of some sort to generate tiles.
	// don't hardcode them like this!
	// or, at least, if you do hardcode them like this,
	//  make yourself a script that spits out the code that you paste in here
	//   and check that script into your repository.

	// define tile 0 as background tile
	PPU466::Tile background_tile;
	for (uint32_t y = 0; y < 8; ++y) {
		background_tile.bit0[y] = 0x00;
		background_tile.bit1[y] = 0x00;
	}
	ppu.tile_table[0] = background_tile;

	// define tile 1-4 as player tile (16x16)
	// define palette 1 as player palette
	auto player_tile_index = load_tiles("assets/reimu_16x16.png", 1, 1);
    for (int i = 0; i < 4; i++)
	{
        player_sprites[i] = player_tile_index[i];
		ppu.sprites[i].attributes = 1;
    }

	// set player initial position to bottom middle
	player_at.x = (PPU466::ScreenWidth - 16) / 2;
    player_at.y = PPU466::ScreenHeight / 4;

	// define tile 5-8 as enemy1 tile (16x16)
	// define palette 2 as enemy1 palette
	enemy1_tiles_index = load_tiles("assets/overseer.png", 5, 2);

	// define tile 9-12 as enemy2 tile (16x16)
	// define palette 3 as enemy2 palette
	enemy2_tiles_index = load_tiles("assets/voteless.png", 9, 3);

	// define tile 17 as player projectile tile (8x8)
	// define palette 4 as player projectile palette
	auto projectile_tiles = load_tiles("assets/reimu_proj.png", 17, 4);

	// define tile 18 as enemy projectile tile (8x8)
	// define palette 5 as enemy projectile palette
	auto enemy_tiles = load_tiles("assets/overseer_proj.png", 18, 5);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// player movement
	constexpr float PlayerSpeed = 60.0f;
	if (left.pressed) player_at.x -= PlayerSpeed * elapsed;
	if (right.pressed) player_at.x += PlayerSpeed * elapsed;
	if (down.pressed) player_at.y -= PlayerSpeed * elapsed;
	if (up.pressed) player_at.y += PlayerSpeed * elapsed;

	// player can pass through bounds
	if (player_at.x < 0.0f)
		player_at.x = float(PPU466::ScreenWidth);
	if (player_at.x > float(PPU466::ScreenWidth))
		player_at.x = 0.0f;
	if (player_at.y < 0.0f)
		player_at.y = float(PPU466::ScreenHeight);
	if (player_at.y > float(PPU466::ScreenHeight))
		player_at.y = 0.0f;

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	// spawn enemy every <enemy_spawn_interval>
	enemy_spawn_timer += elapsed;
    if (enemy_spawn_timer >= enemy_spawn_interval) {
        enemy_spawn_timer -= enemy_spawn_interval;
		spawn_enemy();
	}

	// update enemy positions, erase if out of bounds
	for (auto it = enemies1.begin(); it != enemies1.end();) {
		it->enemy_move(*it, elapsed);

		// enemy can pass through x bounds
		if (it->pos.x < 0.0f)
			it->pos.x += (float) PPU466::ScreenWidth;
		if (it->pos.x > (float) PPU466::ScreenWidth)
			it->pos.x -= (float) PPU466::ScreenWidth;

		// if enemy reaches the bottom, delete it
		if (it->pos.y <= 0.0f) {
			it = enemies1.erase(it);
		} else {
			++it;
		}
	}

	for (auto it = enemies2.begin(); it != enemies2.end();) {
		it->enemy_move(*it, elapsed);

		// enemy can pass through x bounds
		if (it->pos.x < 0.0f)
			it->pos.x += (float) PPU466::ScreenWidth;
		if (it->pos.x > (float) PPU466::ScreenWidth)
			it->pos.x -= (float) PPU466::ScreenWidth;

		// if enemy reaches the bottom, delete it
		if (it->pos.y <= 0.0f) {
			it = enemies2.erase(it);
		} else {
			++it;
		}
	}

	// move projectiles and check for collision
	// used AABB for collision detection because all my sprites are boxes:
	// https://en.wikipedia.org/wiki/Minimum_bounding_box#Axis-aligned_minimum_bounding_box
	for (auto p_it = player_projectiles.begin(); p_it != player_projectiles.end();) {
		Projectile &proj = *p_it;
		proj.pos += glm::vec2(0.0f, proj.velocity) * elapsed;

		bool hit = false;

		if (proj.pos.y < 0 || proj.pos.y > (float) PPU466::ScreenHeight) {
			p_it = player_projectiles.erase(p_it);
			continue;
		}

		for (auto e_it = enemies1.begin(); e_it != enemies1.end(); ) {
			if (proj.pos.x < e_it->pos.x + 16 && proj.pos.x + 8 > e_it->pos.x &&
				proj.pos.y < e_it->pos.y + 16 && proj.pos.y + 8 > e_it->pos.y) {

				p_it = player_projectiles.erase(p_it);
				hit = true;
				e_it = enemies1.erase(e_it);

				break;
			} else {
				++e_it;
			}
		}

		if (!hit) ++p_it;
	}

	// launch player projectile
	projectile_timer += elapsed;
	if (projectile_timer >= projectile_interval) {
		projectile_timer -= projectile_interval;

		if (player_projectiles.size() + enemy_projectiles.size() < MaxProjectiles) {
			Projectile p;
			p.pos = player_at + glm::vec2(5.0f, 5.0f);
			p.velocity = 100.0f;
			player_projectiles.push_back(p);
		}
	}

	// enemy projectile movement
	for (auto it = enemy_projectiles.begin(); it != enemy_projectiles.end();) {
		Projectile &ep = *it;
		ep.pos += glm::vec2(0.0f, ep.velocity) * elapsed;

		// if enemy projectile hit player, quit game
		if (ep.pos.x < player_at.x + 16 && ep.pos.x + 8 > player_at.x &&
			ep.pos.y < player_at.y + 16 && ep.pos.y + 8 > player_at.y) {
			std::exit(0);
		}

		if (ep.pos.y < 0 || ep.pos.y > (float) PPU466::ScreenHeight) {
			it = enemy_projectiles.erase(it);
		} else {
			++it;
		}
	}

	// launch enemy projectile
	for (auto &enemy : enemies1) {
		enemy.shoot_timer += elapsed;
		if (enemy.shoot_timer >= enemy.shoot_interval) {
			enemy.shoot_timer -= enemy.shoot_interval;

			if (player_projectiles.size() + enemy_projectiles.size() < MaxProjectiles) {
				Projectile p;
				p.pos = enemy.pos + glm::vec2(5.0f, -5.0f);
				p.velocity = -150.0f;
				enemy_projectiles.push_back(p);
			}
		}
	}

	// if enemy 2 hit player, quit
	for (auto enemy : enemies2) {
		if (enemy.pos.x < player_at.x + 16 && enemy.pos.x + 16 > player_at.x &&
			enemy.pos.y < player_at.y + 16 && enemy.pos.y + 16 > player_at.y) {
			std::exit(0);
		}
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//--- set ppu state based on game state ---
	for (uint32_t y = 0; y < PPU466::BackgroundHeight; ++y) {
		for (uint32_t x = 0; x < PPU466::BackgroundWidth; ++x) {
			ppu.background[x+PPU466::BackgroundWidth*y] = 0;
		}
	}

	// clear sprites to avoid ghost sprites
	for (int i = 0; i < 64; i++)
	{
		ppu.sprites[i].y = 240;
	}

	// set player sprites
	// used the exact same way as the base code given
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 2; i++) {
			int sprite_index = j*2 + i;
			ppu.sprites[sprite_index].x = (uint8_t)(player_at.x + i*8);
			ppu.sprites[sprite_index].y = (uint8_t)(player_at.y + (1-j)*8);
			ppu.sprites[sprite_index].index = (uint8_t)(player_sprites[sprite_index]); 
			ppu.sprites[sprite_index].attributes = 1;
		}
	}

	// set enemy sprites
    for (auto enemy : enemies1) {
		int sprite_idx = enemy.sprite_start;
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 2; ++i) {
				int tile_idx = j * 2 + i;
                ppu.sprites[sprite_idx].x = (uint8_t) (enemy.pos.x + i * 8);
                ppu.sprites[sprite_idx].y = (uint8_t) (enemy.pos.y + (1 - j) * 8);
				ppu.sprites[sprite_idx].index = (uint8_t) (enemy.tiles[tile_idx]);
                ppu.sprites[sprite_idx].attributes = 2;
                sprite_idx++;
            }
        }
    }

	for (auto enemy : enemies2) {
		int sprite_idx = enemy.sprite_start;
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 2; ++i) {
				int tile_idx = j * 2 + i;
                ppu.sprites[sprite_idx].x = (uint8_t) (enemy.pos.x + i * 8);
                ppu.sprites[sprite_idx].y = (uint8_t) (enemy.pos.y + (1 - j) * 8);
				ppu.sprites[sprite_idx].index = (uint8_t) (enemy.tiles[tile_idx]);
                ppu.sprites[sprite_idx].attributes = 3;
                sprite_idx++;
            }
        }
    }

	// set projectile sprites
	int sprite_idx = ProjectileSpriteStart;
	for (auto p : player_projectiles) {
		ppu.sprites[sprite_idx].x = (uint8_t) (p.pos.x);
		ppu.sprites[sprite_idx].y = (uint8_t) (p.pos.y);
		ppu.sprites[sprite_idx].index = 17;
		ppu.sprites[sprite_idx].attributes = 4;
		sprite_idx++;
	}

	for (auto ep : enemy_projectiles) {
        ppu.sprites[sprite_idx].x = (uint8_t) (ep.pos.x);
        ppu.sprites[sprite_idx].y = (uint8_t) (ep.pos.y);
        ppu.sprites[sprite_idx].index = 18;
        ppu.sprites[sprite_idx].attributes = 5;
        sprite_idx++;
    }

	//--- actually draw ---
	ppu.draw(drawable_size);
}