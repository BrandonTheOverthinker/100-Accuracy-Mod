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
using namespace std;

#include "scriptinfo.h"

static Ped lastHorse = 0;
#define WEAPON_LASSO 0x7A8A724A
bool modEnabled = true;

void main()
{
	Vector3 playerpos;
	while (true)
	{
		
		if (IsKeyJustUp(VK_F3))  // using the keyboard.h function
		{
			modEnabled = !modEnabled;
			// Optionally, you can display a message on screen here to tell the player the mod is now on/off.
			if (modEnabled)
			{
				// Example: UI::DisplayHelpTextThisFrame("Mod Enabled", 0);
			}
			else
			{
				// Example: UI::DisplayHelpTextThisFrame("Mod Disabled", 0);
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

			if (currentHorse != 0) {
				// While riding, make sure the current horse is invincible
				ENTITY::SET_ENTITY_INVINCIBLE(currentHorse, true);
				lastHorse = currentHorse; // update our stored reference
			}
			else if (lastHorse != 0) {
				// If we are not riding now, reset the last horse to be vulnerable.
				ENTITY::SET_ENTITY_INVINCIBLE(lastHorse, false);
				lastHorse = 0;
			}

			Hash currentWeapon = WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &currentWeapon, true, -1, false);
			if (currentWeapon == WEAPON_LASSO)
			{
				// Skip explosion because the lasso should not count.
			}
			else if (PED::IS_PED_SHOOTING(PLAYER::GET_PLAYER_PED(PLAYER::PLAYER_ID()))) {
				WAIT(3500);
				if (!PLAYER::HAS_PLAYER_DAMAGED_AT_LEAST_ONE_PED(PLAYER::PLAYER_ID())) {
					playerpos = ENTITY::GET_ENTITY_COORDS(PLAYER::GET_PLAYER_PED(PLAYER::PLAYER_ID()), true, false);
					
					FIRE::ADD_EXPLOSION(playerpos.x, playerpos.y, playerpos.z, 26, 10.0f, true, false, 10.0f);
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