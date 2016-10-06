/*
	Student Number: N9162259
	Name: Jesse Stanger
	Extra functionality:
		Can play with either the set formation of 3-4-3 aliens. Or can set aliens to randomly generate.
			See Line: 35

		Can set as many alien's as you like (as long as ALIEN_STRUCTURE is 1), just change N_ALIENS.
			See Line: 40

		Can set as many bomb's as you like, just change N_BOMBS. 
			See Line: 44
			This will define the maximum number that can spawn at once. Default is 4.
*/


#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <windows.h>

#include "cab202_graphics.h"
#include "cab202_sprites.h"
#include "cab202_timers.h"

#define CREDITS				"Jesse Stanger (n9162259)"

/*
Alien structure: Choice of 0 and 1
	0 generates 3-4-3 structure of aliens. 
	1 is random generation of locations								*/
#define ALIEN_STRUCTURE		0

/*
If ALIEN_STRUCTURE is 0, N_ALIENS must be exactly 10
If ALIEN_STRUCTURE is 1, N_ALIENS can be any reasonable number		*/
#define N_ALIENS 			10

/*
N_BOMBS can be adjusted to any reasonable value						*/
#define N_BOMBS 			4

#define BULLET_TIMER		10
#define CURVE_BULLET_TIMER	50
#define ALIEN_TIMER			650
#define BOMB_TIMER			350
#define BOMB_DROP_TIMER		3000

#define KILL_ALIEN_POINTS	30
#define CLEAR_LEVEL_POINTS	500

#define FORMATION_X_DIST	10
#define FORMATION_Y_DIST	2

typedef struct Alien {
	sprite_id A_sprite[N_ALIENS];
	int A_sprite_id[N_ALIENS];
	int sprite_init_x[N_ALIENS];
	int sprite_init_y[N_ALIENS];
	bool alive[N_ALIENS];
	int still_alive;
	bool alien_launched;
	int alien_launched_id;

	sprite_id bombs[N_BOMBS];
	int prev_dropper;
	int num_active_bombs;

	timer_id alien_timer;
	timer_id bomb_timer;
	timer_id bomb_drop_timer;
	
} Alien;

typedef struct Game {
	bool over;
	int score;
	int event_loop_delay;
	int level;
	bool level_changed;
	int lives;
	char* levelName[5];

	sprite_id ship;
	sprite_id bullet;

	timer_id bullet_timer;
	timer_id curve_bullet_timer;
	sprite_id death;
} Game;

// ---------------------------------------------------------------- //

void setup_game(Game * game) {
	game->over = false;
	game->score = 0;
	game->level = 1;
	game->level_changed = false;
	game->lives = 3;
	game->event_loop_delay = 10;

	game->levelName[0] = "Basic";
	game->levelName[1] = "Harmonic Motion";
	game->levelName[2] = "Falling Motion";
	game->levelName[3] = "Drunken Motion";
	game->levelName[4] = "Aggressive Motion";
}

void draw_hud(Game * game) {
	int hud_score_lives_length = snprintf( NULL, 0, "Score: %d Lives: %d", game->score, game->lives );
	int hud_level_name_length = snprintf( NULL, 0, "Level: %d - %s", game->level, game->levelName[game->level - 1] );
	int scores_margin = screen_width() - hud_score_lives_length;
	int left_margin = ( screen_width() / 2 ) - ( hud_level_name_length / 2 );

	draw_formatted( left_margin, screen_height() - 1, "Level: %d - %s", game->level, game->levelName[game->level - 1]);
	draw_formatted( 0, screen_height() - 2, CREDITS);
	draw_formatted( scores_margin, screen_height() - 2, "Score: %d Lives: %d", game->score, game->lives );
	draw_line( 0, screen_height() - 3, screen_width(), screen_height() - 3, '_');
}

int rand_between(int first, int last) {
	return first + rand() % (last - first);
}

// ---------------------------------------------------------------- //

void setup_ship(Game * game) {
	static char bitmap[] = ".^.";
	game->ship = create_sprite(screen_width() / 2, screen_height() - 5, 3, 1, bitmap);
}

void safe_respawn(Game * game, Alien * aliens) {
	int center = screen_width() / 2;
	bool safe = true;

	do {
		for (int i = 0; i < N_ALIENS; i++) {
			if (aliens->A_sprite[i]->is_visible == true && aliens->A_sprite[i]->x == center) {
				safe = false;
			}
		}

		for (int j = 0; j < N_BOMBS; j++) {
			if (aliens->bombs[j]->is_visible == true && aliens->bombs[j]->x == center) {
				safe = false;
			}
		}

		if (safe == true) {
			game->ship->x = center;
		} else {
			center -= 1;
		}
		
	} while (!safe);
}

bool update_ship(Game* game, Alien * aliens, int key_code) {
	int old_x;
	int old_y;
	int new_x;
	int new_y;

	old_x = (int) round(game->ship->x);
	old_y = (int) round(game->ship->y);

	if (key_code == 'a' || key_code == 'A') {
		if (game->ship->x > 0) {
				game->ship->x -= 1;
		}
	}
	else if (key_code == 'd' || key_code == 'D') {
		if ((game->ship->x + game->ship->width) < screen_width()) {
				game->ship->x += 1;
		}
	} else {
		return false;
	}

	new_x = (int) round(game->ship->x);
	new_y = (int) round(game->ship->y);

	//player->alien collision
	for (int i = 0; i < N_ALIENS; i++) {
		if (new_x == aliens->A_sprite[i]->x && new_y == aliens->A_sprite[i]->y) {
			safe_respawn(game, aliens);
			game->lives -= 1;
			aliens->still_alive -= 1;
			aliens->A_sprite[i]->is_visible = false;
		}
	}

	return (new_x != old_x) || (new_y != old_y);
}

void draw_ship(Game * game) {
	draw_sprite(game->ship);
}

void cleanup_ship(Game * game) {
	destroy_sprite(game->ship);
	game->ship = NULL;
}

// ---------------------------------------------------------------- //

void setup_bullet(Game * game) {
	static char bitmap[] = {'|'};

	game->bullet = create_sprite(0, 0, 1, 1, bitmap);
	game->bullet->is_visible = false;
	game->bullet_timer = create_timer(BULLET_TIMER);
	game->curve_bullet_timer = create_timer(CURVE_BULLET_TIMER);
}

void launch_bullet(Game * game, int key_code) {
	if (game->bullet->is_visible) {
		return;
	}

	int x0 = 0;
	int x1 = 0;
	int x2 = 0;
	int y0 = 0;
	int y1 = 0;
	int y2 = 0;

	if (key_code == 's' || key_code == 'S') {
		game->bullet->x = game->ship->x + (game->ship->width / 2);
		game->bullet->y = game->ship->y - 1;
		game->bullet->dx = 0;
		game->bullet->dy = 1;
		game->bullet->is_visible = true;
	}
	else if (key_code == 'z' || key_code == 'Z') {
		// x0 = game->ship->x;
		// x1 = //choose an aliens x
		// x2 = rand_between(0, game->ship->x);
		// y0 = game->ship->y;
		// y1 = //choose an aliens y
		// y2 = rand_between(0, game->ship->y);
	}
	else if (key_code == 'c' || key_code == 'C') {
		// x0 = game->ship->x;
		// x1 = //choose an aliens x
		// x2 = rand_between(game->ship->x, screen_width() - 1);
		// y0 = game->ship->y;
		// y1 = //choose an aliens y
		// y2 = rand_between(0, game->ship->y);
	}
}

bool move_bullet(Game * game, Alien * aliens) {
	if (!game->bullet->is_visible || !timer_expired(game->bullet_timer)) {
		return false;
	}

	int old_x;
	int old_y;
	int new_x;
	int new_y;

	old_x = (int) round(game->bullet->x);
	old_y = (int) round(game->bullet->y);
	game->bullet->x += game->bullet->dx;
	game->bullet->y -= game->bullet->dy;
	new_x = (int) round(game->bullet->x);
	new_y = (int) round(game->bullet->y);

	if (new_y < 0) {
		game->bullet->is_visible = false;
		return true;
	}

	//bullet->alien collision
	for (int i = 0; i < N_ALIENS; i++) {
		if (aliens->A_sprite[i]->is_visible) {
			int alien_x = (int) round(aliens->A_sprite[i]->x);
			int alien_y = (int) round(aliens->A_sprite[i]->y);

			if ((new_x == alien_x) && (new_y == alien_y)) {
				game->bullet->is_visible = false;
				aliens->A_sprite[i]->is_visible = false;
				aliens->alive[i] = false;
				aliens->still_alive -= 1;
				game->score = game->score + KILL_ALIEN_POINTS;
			}
		}
	}
	return (old_x != new_x) || (old_y != new_y);
}

bool update_bullet(Game * game, Alien * aliens, int key_code) {
	if (game->bullet->is_visible) {
		return move_bullet(game, aliens);
	}
	else if (key_code == 's' || key_code == 'S' 
		||	key_code == 'z' || key_code == 'Z' 
		||	key_code == 'c' || key_code == 'C') {
		launch_bullet(game, key_code);
		return true;
	}
	else {
		return false;
	}
}

void draw_bullet(Game * game) {
	draw_sprite(game->bullet);
}

void cleanup_bullet(Game * game) {
	destroy_sprite(game->bullet);
	game->bullet = NULL;
}

// ---------------------------------------------------------------- //

void setup_aliens(Game * game, Alien * aliens) {
	static char bitmap[] = "@";
	aliens->still_alive = N_ALIENS;
	aliens->alien_timer = create_timer(ALIEN_TIMER);
	int horz_dist = screen_width() / 2;;
	int vert_dist = FORMATION_Y_DIST;

	if (ALIEN_STRUCTURE == 0 && N_ALIENS == 10) {

		int alien_x[N_ALIENS] = {
			horz_dist - FORMATION_X_DIST, horz_dist, horz_dist + FORMATION_X_DIST,
			horz_dist - (FORMATION_X_DIST * 1.5), horz_dist - (FORMATION_X_DIST * 0.5),
			horz_dist + (FORMATION_X_DIST * 0.5), horz_dist + (FORMATION_X_DIST * 1.5),
			horz_dist - FORMATION_X_DIST, horz_dist, horz_dist + FORMATION_X_DIST
		};

		int alien_y[N_ALIENS] = {
			vert_dist * 2, vert_dist * 2, vert_dist * 2,
			vert_dist * 3, vert_dist * 3, vert_dist * 3, vert_dist * 3,
			vert_dist * 4, vert_dist * 4, vert_dist * 4
		};

		for (int i = 0; i < N_ALIENS; i++) {
			aliens->A_sprite_id[i] = i;
			aliens->A_sprite[i] = create_sprite(alien_x[i], alien_y[i], 1, 1, bitmap);
			aliens->sprite_init_x[i] = aliens->A_sprite[i]->x;
			aliens->sprite_init_y[i] = aliens->A_sprite[i]->y;

			switch (game->level) {
				case 1:
					aliens->A_sprite[i]->dx = 1;
				case 2:
					aliens->A_sprite[i]->dx = 1;
				case 3:
					aliens->A_sprite[i]->dx = 1;
					aliens->A_sprite[i]->dy = 0.33;
				case 4:
					aliens->A_sprite[i]->dy = 0.2;
				case 5:
					aliens->A_sprite[i]->dx = 1;
					aliens->A_sprite[i]->dy = 0.33;
				default:
					break;
			}
		}
	}

	else if (ALIEN_STRUCTURE == 1) {
		int alien_x[N_ALIENS];
		int alien_y[N_ALIENS];

		//generates random positions for aliens
		for (int i = 0; i < N_ALIENS; i++) {
			if (game->level == 2) {
				alien_y[i] = rand_between( (screen_height() / 8) + 1, screen_height() / 8 * 5);
			} else {
				alien_y[i] = rand_between(0, screen_height() / 2);
			}
			alien_x[i] = rand_between(0, screen_width());
		}

		for (int i = 0; i < N_ALIENS; i++) {
			aliens->A_sprite_id[i] = i;
			aliens->A_sprite[i] = create_sprite(alien_x[i], alien_y[i], 1, 1, bitmap);
			aliens->A_sprite[i]->dx = 1;
			aliens->A_sprite[i]->dy = 0.1;
		}
	}
}

void draw_aliens(Alien * aliens) {
	for (int i = 0; i < N_ALIENS; i++) {
		draw_sprite(aliens->A_sprite[i]);
	}
}

bool move_alien(Game * game, Alien * aliens, sprite_id alien, int alien_id) {
	if (!alien->is_visible) {
		return false;
	}

	int old_x = (int) round(alien->x);
	int old_y = (int) round(alien->y);

	int choice = 0;
	int init_x = aliens->sprite_init_x[alien_id];

	switch (game->level) {
		case 1:
			alien->x += alien->dx;
			break;
		case 2:
			alien->x += alien->dx;

			if (!aliens->A_sprite[choice]->is_visible) {
				do {
					choice += 1;
				} while (!aliens->A_sprite[choice]->is_visible);
			}
			
			alien->y += sin(0.6 * (aliens->A_sprite[choice]->x));
			break;
		case 4:
			if (ALIEN_STRUCTURE == 1) {
				if (alien->x < 4) {
					alien->x += rand_between(5, 10);
				}
				else if (alien->x > screen_width() - 4) {
					alien->x += rand_between(-10, -5);
				}
				else {
					alien->x += rand_between(-2, 3);
				}
			}
			else if (ALIEN_STRUCTURE == 0) {
				choice = rand() % 5;

				if (choice == 1 || choice == 3) {
					alien->x = rand_between( init_x, init_x - FORMATION_X_DIST / 5);
				} 
				else if (choice == 2 || choice == 4) {
					alien->x = rand_between(init_x, init_x + FORMATION_X_DIST / 5 );
				}
				else if (choice == 0) {
					for (int i = 0; i < N_ALIENS; i++) {
						if (i == 0 || i == 3 || i == 4 ||i == 7) {
							aliens->A_sprite[i]->x -= FORMATION_X_DIST / 6;
						}
						else if (i ==2 || i == 5 || i == 6 || i == 9) {
							aliens->A_sprite[i]->x += FORMATION_X_DIST / 6;
						}
					}
				}
			}

			alien->y += alien->dy;
			break;
		case 5:
			// if (aliens->alien_launched == false) {
			// 	choice = rand_between(0, N_ALIENS);

			// 	if (choice == 4 || choice == 5) {
			// 		//do {
			// 			alien->x = -1;
			// 		//} while (alien->x != aliens->A_sprite[alien_id - 4]->x);

			// 		if (alien->x == aliens->A_sprite[3]->x || alien->y == aliens->A_sprite[0]->y) {
			// 			alien->x = screen_width() / 2; 
			// 			alien->y = screen_height() / 2; 
			// 			alien->dx = 0; 
			// 			alien->dy = 0; 
						

			// 		}
			// 		aliens->alien_launched = true;
			// 		aliens->alien_launched_id = choice;
			// 	} else {
			// 		alien->x -= 3;
			// 		aliens->alien_launched = true;
			// 		aliens->alien_launched_id = choice;
			// 	}
			// }
		
			alien->x += alien->dx;
			alien->y += alien->dy;
			break;
		default:
			alien->x += alien->dx;
			alien->y += alien->dy;
			break;
	}

	int new_x = (int) round(alien->x);
	int new_y = (int) round(alien->y);

	if (new_x < 0) {
		alien->x = screen_width() - 1;
		return true;
	}
	if (new_x >= screen_width()) {
		alien->x = 0;
		return true;
	}

	if (new_y >= screen_height() - 4) {
		alien->y = 0;
		return true;
	}

	//Alien->player collision
	if (new_x == game->ship->x && new_y == game->ship->y) {
		safe_respawn(game, aliens);
		game->lives -= 1;
		aliens->still_alive -= 1;
		alien->is_visible = false;
	}

	return (old_x != new_x) || (old_y != new_y);
}

bool update_aliens(Game * game, Alien * aliens) {
	if (!timer_expired(aliens->alien_timer)) {
		return false;
	}

	bool changed = false;

	for (int i = 0; i < N_ALIENS; i++) {
		changed = move_alien(game, aliens, aliens->A_sprite[i], aliens->A_sprite_id[i]) || changed;
	}
	return changed;
}

void cleanup_aliens(Alien * aliens) {
	for (int i = 0; i < N_ALIENS; i++) {
		destroy_sprite(aliens->A_sprite[i]);
		aliens->A_sprite[i] = NULL;
		aliens->still_alive = 0;
	}
}

// -------------------------------------------------------------------------------------------------########------------------------- //
void setup_bombs(Alien * aliens) {
	static char bitmap[] = {'!'};
	aliens->bomb_timer = create_timer(BOMB_TIMER);
	aliens->bomb_drop_timer = create_timer(BOMB_DROP_TIMER);
	aliens->num_active_bombs = 0;
	aliens->prev_dropper = 0;

	for (int i = 0; i < N_BOMBS; i++) {
		aliens->bombs[i] = create_sprite(0, 0, 1, 1, bitmap);	
		aliens->bombs[i]->is_visible = false;
	}
}

void cleanup_bombs(Alien * aliens) {
	for (int i = 0; i < N_BOMBS; i++) {
		destroy_sprite(aliens->bombs[i]);
		aliens->bombs[i] = NULL;
	}
}

bool move_bomb(Game * game, Alien * aliens, int i) {
	int old_x = (int) round(aliens->bombs[i]->x);
	int old_y = (int) round(aliens->bombs[i]->y);
	aliens->bombs[i]->y += aliens->bombs[i]->dy;
	int new_x = (int) round(aliens->bombs[i]->x);
	int new_y = (int) round(aliens->bombs[i]->y);

	//invisible if out of screen
	if (new_y > screen_height() - 4) {
		aliens->bombs[i]->is_visible = false;
		aliens->num_active_bombs -= 1;
	}

	//bomb->player collision
	if (new_y == game->ship->y && new_x == game->ship->x + 1) {
		game->lives -= 1;
		cleanup_bombs(aliens);
		setup_bombs(aliens);
		safe_respawn(game, aliens);
	}
	return (old_x != new_x) || (old_y != new_y);
}

void launch_bomb(Alien * aliens, int dropper) {
	int bomb_to_drop = 0;

	for (int i = 0; i < N_BOMBS; i++) {
		if (!aliens->bombs[i]->is_visible) {
			bomb_to_drop = i;
		}
	}

	aliens->bombs[bomb_to_drop]->x = aliens->A_sprite[dropper]->x;
	aliens->bombs[bomb_to_drop]->y = aliens->A_sprite[dropper]->y + 1;
	aliens->bombs[bomb_to_drop]->dy = 1;
	aliens->bombs[bomb_to_drop]->is_visible = true;
	aliens->num_active_bombs += 1;
}

bool update_bomb(Game * game, Alien * aliens) {
	if (aliens->num_active_bombs < N_BOMBS && timer_expired(aliens->bomb_drop_timer)) {

		int dropper = rand_between(0, N_ALIENS);

		if (aliens->still_alive > 1) {
	 		do { dropper = rand_between(0, N_ALIENS); }
	 		while (dropper == aliens->prev_dropper || aliens->A_sprite[dropper]->is_visible == false);
	 	} else {
	 		do { dropper = rand_between(0, N_ALIENS); }
	 		while (aliens->A_sprite[dropper]->is_visible == false);
	 	}

		launch_bomb(aliens, dropper);
		aliens->prev_dropper = dropper;
	}

	if (timer_expired(aliens->bomb_timer)) {
		for (int i = 0; i < N_BOMBS; i++) {
			if (aliens->bombs[i]->is_visible) {
				move_bomb(game, aliens, i);
			}
		}
		return true;
	}
	return false;
}

void draw_bombs(Alien * aliens) {
	for (int i = 0; i < N_BOMBS; i++ ) {
		if ( aliens->bombs[i]->is_visible == true ) {
			draw_sprite(aliens->bombs[i]);
		}
	}
}

// ------------------------------------------------------------------------------------------------#########-------------------------- //

void setup_all(Game * game, Alien * aliens) {
	setup_game(game);
	setup_ship(game);
	setup_bullet(game);
	setup_aliens(game, aliens);
	setup_bombs(aliens);
}

void cleanup_all(Game * game, Alien * aliens) {
	cleanup_ship(game);
	cleanup_bullet(game);
	cleanup_aliens(aliens);
	cleanup_bombs(aliens);
}

void draw_all(Game * game, Alien * aliens) {
	clear_screen();
	draw_hud(game);
	draw_ship(game);
	draw_bullet(game);
	draw_aliens(aliens);
	draw_bombs(aliens);
	show_screen();
}

void show_game_over(Game * game, Alien * aliens) {
	clear_screen();

	static char bitmap[] = 
		"               GAME OVER             "
		"             YOU ARE DEAD            "
		" PRESS 'q' TO QUIT OR 'r' TO RESTART ";

	int sprite_width = sizeof(bitmap) / 3;
	int sprite_height = sizeof(bitmap) / 37;
	int sprite_x = (screen_width() / 2) - sprite_width/2;
	int sprite_y = (screen_height() / 2) - sprite_height/2;

	game->death = create_sprite(sprite_x, sprite_y, sprite_width, sprite_height, bitmap);
	draw_sprite(game->death);

	int score_width = snprintf(NULL, 0, "You scored: %d", game->score);
	int score_x = (screen_width() / 2) - score_width / 2;
	int score_y = (screen_height() / 2) + sprite_height;

	draw_formatted(score_x, score_y, "You scored: %d", game->score);

	show_screen();

	int key_code = wait_char();

	if (key_code == 'r' || key_code == 'R') {
		setup_all(game, aliens);
	}
	else if (key_code == 'q' || key_code == 'Q') {
		game->over = true;
	}
}

void level_manager(Game * game, Alien * aliens, int key_code) {
	if (game->level_changed == true) {
		cleanup_all(game, aliens);
		setup_ship(game);
		setup_bullet(game);
		setup_aliens(game, aliens);
		setup_bombs(aliens);
		game->level_changed = false;
	}

	switch (aliens->still_alive) {
		case 0:
			game->score += CLEAR_LEVEL_POINTS;
			aliens->still_alive = N_ALIENS;
			game->level += 1;
			game->level_changed = true;
			break;
		default:
			break;
	}
}

void event_loop() {
	Game game;
	Alien aliens;

	setup_all(&game, &aliens);
	draw_all(&game, &aliens);

	while (!game.over) {
		int key_code = get_char();

		if (game.level > 5) {
			game.level = 1;
			game.level_changed = true;
		}
		else if (game.lives == 0) {
			show_game_over(&game, &aliens);		
		}
		else if (key_code == 'q' || key_code == 'Q') {
			game.over = true;
		}
		else if (key_code == 'r' || key_code == 'R') {
			setup_all(&game, &aliens);
		}
		else if (key_code == 'l' || key_code == 'L') {
			game.level += 1;
			game.level_changed = true;
		}

		bool show_ship = update_ship(&game, &aliens, key_code);
		bool show_bullet = update_bullet(&game, &aliens, key_code);
		bool show_aliens = update_aliens(&game, &aliens);
		bool show_bomb = update_bomb(&game, &aliens);

		if (show_ship || show_bullet || show_aliens || show_bomb) {
			draw_all(&game, &aliens);
		}

		level_manager(&game, &aliens, key_code);
		timer_pause(game.event_loop_delay);
	}
	cleanup_all(&game, &aliens);
}

void ShowConsoleCursor(bool showFlag) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO     cursorInfo;
    GetConsoleCursorInfo(out, &cursorInfo);
    cursorInfo.bVisible = showFlag; // set the cursor visibility
    SetConsoleCursorInfo(out, &cursorInfo);
}

int main() {
	srand(time(NULL));
	ShowConsoleCursor(false);

	setup_screen();
	event_loop();
	cleanup_screen();

	return 0;
}