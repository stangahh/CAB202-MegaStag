#ifndef _GAME_HEADER_
#define _GAME_HEADER_

#include <stdio.h>
#include <string.h>

#include <cab202_graphics.h>
#include "cab202_timers.h"
#include "player.h"

typedef struct Game
{
	int score;
	int lives;
	int level;
	//char levelName[]; 
} Game;

void setup_level1( Game * game );

#endif