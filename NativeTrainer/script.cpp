/*
	THIS FILE IS A PART OF RDR 2 SCRIPT HOOK SDK
				http://dev-c.com
			(C) Alexander Blade 2019
*/

#include "script.h"
#include "scriptmenu.h"
#include "keyboard.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include "scriptinfo.h"

using namespace std;
static Ped lastHorse = 0;
#define WEAPON_LASSO 0x7A8A724A // Don't think this is the correct hash
bool modEnabled = true;

void main()
{
	Vector3 playerPos;
	while (true)
	{
		
		if (IsKeyJustUp(VK_F3))  // using the keyboard.h function
		{
			modEnabled = !modEnabled;
			
			if (modEnabled)
			{
				// Display text to indicate the mod is enabled, for 3 seconds
				
			}
			else
			{
				// Display text to indicate the mod is disabled, for 3 seconds
			}
		}

		if (!modEnabled)
		{
			WAIT(0);
			continue;
		}
		
		if (modEnabled) {
			Ped playerPed = PLAYER::GET_PLAYER_PED(PLAYER::PLAYER_ID());
			Ped currentHorse = PED::GET_MOUNT(playerPed);
			Entity currentWeapon = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, -1);

			if (currentHorse != 0) {
				// If riding, set the current horse to be invincible
				ENTITY::SET_ENTITY_INVINCIBLE(currentHorse, true);
				lastHorse = currentHorse; // update our stored reference
			}
			else if (lastHorse != 0) {
				// If not riding, reset the last horse to be vulnerable.
				ENTITY::SET_ENTITY_INVINCIBLE(lastHorse, false);
				lastHorse = 0;
			}

			
			if (currentWeapon == WEAPON_LASSO)
			{
				// Skip explosion because the lasso should not count. still doesn't work
			}
			else if (PED::IS_PED_SHOOTING(PLAYER::GET_PLAYER_PED(PLAYER::PLAYER_ID()))) {
				WAIT(3500);
				if (!PLAYER::HAS_PLAYER_DAMAGED_AT_LEAST_ONE_PED(PLAYER::PLAYER_ID())) {
					playerPos = ENTITY::GET_ENTITY_COORDS(PLAYER::GET_PLAYER_PED(PLAYER::PLAYER_ID()), true, false);
					
					FIRE::ADD_EXPLOSION(playerPos.x, playerPos.y, playerPos.z, 26, 10.0f, true, false, 10.0f);
				}
			}
			
			PLAYER::CLEAR_PLAYER_HAS_DAMAGED_AT_LEAST_ONE_PED(PLAYER::PLAYER_ID());
			WAIT(0);
		}

		
	}
}

void ScriptMain()
{
	srand(GetTickCount());
	main();
}
