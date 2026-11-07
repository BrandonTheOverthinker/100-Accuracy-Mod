/*
	THIS FILE IS A PART OF RDR 2 SCRIPT HOOK SDK
				http://dev-c.com
			(C) Alexander Blade 2019
*/

#include "script.h"
#include "scriptmenu.h"
#include "keyboard.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include "scriptinfo.h"

using namespace std;

	// Settings: //

static const ULONGLONG defaultKillWindow = 2000;
static const ULONGLONG bowKillWindow = 3500;
static const ULONGLONG dynamicKillWindow = 9000;
static const ULONGLONG msgDuration = 3000;  // how long the F3 text stays on screen
static const ULONGLONG executionStateLogInterval = 250;
static const bool logging = true;  // writes 100AccuracyMod.log (in the game folder)

static const unordered_set<Hash> exemptWeapons = {
	0x7A8A724A, // lasso
	0x7067E7A7, // fire bottle
};

static const unordered_set<Hash> explodingWeapons = {
	0xA64DAA5E, // dynamite
};

static const unordered_set<Hash> bowWeapons = {
	0x88A8505C, // bow
};

	// States: //

static bool modEnabled = true;
static Ped lastHorse = 0;
static bool wasShooting = false; // detecting the moment a shot starts
static Hash prevWeapon = 0;

static bool shotPending = false; // are we waiting to see if a shot lands or not?
static bool hitSeen = false; // did ped get damaged?
static ULONGLONG shotStart = 0;
static ULONGLONG shotDeadline = 0;
static ULONGLONG lastExecutionStateLog = 0;

static string statusText;
static bool msgVisible = false;
static ULONGLONG msgEnd = 0;

	// Helpers: //

// One place for the clock to experiment with the dead-eye bug, could try swapping this for GAMEPLAY::GET_GAME_TIMER().
static ULONGLONG getTime()
{
	return GetTickCount64();
}

static string hexString(Hash h)
{
	char buf[32];
	sprintf_s(buf, "0x%08X", (unsigned int)h);
	return string(buf);
}

static void logMessage(const string& msg)
{
	if (!logging) return;
	ofstream f("100AccuracyMod.log", ios::app);
	f << getTime() << "  " << msg << "\n";
}

// Bottom screen text. Uses the same UI::DRAW_TEXT approach as rampage trainer (has to be drawn every frame).
static void showStatusText(const char* text)
{
	statusText = text;
	msgVisible = true;
	msgEnd = getTime() + msgDuration;
}

static void drawStatusText()
{
	if (!msgVisible) return;

	UI::SET_TEXT_SCALE(0.5f, 0.5f);
	UI::SET_TEXT_COLOR_RGBA(255, 255, 255, 255);
	UI::SET_TEXT_CENTRE(1);
	UI::SET_TEXT_DROPSHADOW(2, 0, 0, 0, 255);
	UI::DRAW_TEXT(GAMEPLAY::CREATE_STRING(10, "LITERAL_STRING", const_cast<char*>(statusText.c_str())), 0.5f, 0.90f);
}

static void cancelPendingShot()
{
	shotPending = false;
	hitSeen = false;
}

	// Main loop: //

void main()
{
	while (true)
	{

		if (IsKeyJustUp(VK_F3))  // using the keyboard.h function
		{
			modEnabled = !modEnabled;

			if (modEnabled)
			{
				// Display text to indicate the mod is enabled, for 3 seconds
				showStatusText("100% Accuracy Mod: ENABLED");
			}
			else
			{
				// Display text to indicate the mod is disabled, for 3 seconds
				showStatusText("100% Accuracy Mod: DISABLED");
				cancelPendingShot();

				// Disable horse invincibility if the mod is turned off while riding:
				if (lastHorse != 0) {
					ENTITY::SET_ENTITY_INVINCIBLE(lastHorse, false);
					lastHorse = 0;
			}
		}
			logMessage(modEnabled ? "Mod ENABLED" : "Mod DISABLED");
		}

		// Hide the text after msgDuration, otherwise draw it (runs even while the mod is disabled)
		if (msgVisible && getTime() >= msgEnd)
			msgVisible = false;
		drawStatusText();

		if (!modEnabled)
		{
			WAIT(0);
			continue;
		}

		Ped playerPed = PLAYER::GET_PLAYER_PED(PLAYER::PLAYER_ID());
		Ped currentHorse = PED::GET_MOUNT(playerPed);

		if (currentHorse != 0) {
			// If riding, set the current horse to be invincible
			ENTITY::SET_ENTITY_INVINCIBLE(currentHorse, true);
			lastHorse = currentHorse; // update stored reference
		}
		else if (lastHorse != 0) {
			// If not riding, reset the last horse to be vulnerable.
			ENTITY::SET_ENTITY_INVINCIBLE(lastHorse, false);
			lastHorse = 0;
		}

		// Which weapon do we have? (this returns a HASH, unlike the old ENTITY_INDEX call)
		Hash weaponNow = 0;
		WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &weaponNow, false, 0, false);

		// Did a new shot just start?
		bool shooting = PED::IS_PED_SHOOTING(playerPed) != 0;
		bool newShot = shooting && !wasShooting; // only true on the first frame of a shot
		wasShooting = shooting; // saves this frame’s result so the next frame can compare against it

		if (newShot)
		{
			// Use last frame's weapon: thrown items (dynamite, fire bottles) have already left player's hand by the time the shot is detected, so "current" may be unarmed.
			Hash shotWeapon = (prevWeapon != 0) ? prevWeapon : weaponNow;

			if (exemptWeapons.count(shotWeapon))
			{
				logMessage("Shot ignored, exempt weapon " + hexString(shotWeapon));
			}
			else
			{
				ULONGLONG window = explodingWeapons.count(shotWeapon) ? dynamicKillWindow : (bowWeapons.count(shotWeapon) ? bowKillWindow : defaultKillWindow);
				bool inMelee = PED::IS_PED_IN_MELEE_COMBAT(playerPed) != 0;
				Ped meleeTarget = PED::GET_MELEE_TARGET_FOR_PED(playerPed);
				bool targetBeingStealthKilled = meleeTarget != 0 && PED::IS_PED_BEING_STEALTH_KILLED(meleeTarget) != 0;
				bool playerControlOn = PLAYER::IS_PLAYER_CONTROL_ON(PLAYER::PLAYER_ID()) != 0;
				int playerGrappleState = PED::GET_PED_GRAPPLE_STATE(playerPed);
				int targetGrappleState = meleeTarget != 0 ? PED::GET_PED_GRAPPLE_STATE(meleeTarget) : -1;
				float playerMeleePhase = PED::_GET_PED_MELEE_ACTION_PHASE(playerPed);
				float targetMeleePhase = meleeTarget != 0 ? PED::_GET_PED_MELEE_ACTION_PHASE(meleeTarget) : -1.0f;

				if (!shotPending)
				{
					shotPending = true;
					hitSeen = false;
					shotStart = getTime();
					shotDeadline = getTime() + window;
				}
				else
				{
					if (getTime() + window > shotDeadline)
					{
						// if the user shoots, then throws dynamite: stretch the window, never shrink it
						shotDeadline = getTime() + window;
					}
				}
				Entity aimedAt = 0;
				PLAYER::GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(PLAYER::PLAYER_ID(), &aimedAt);
				logMessage("DIAG melee=" + to_string(inMelee)
					+ " meleeTarget=" + to_string(meleeTarget)
					+ " stealthKilled=" + to_string(targetBeingStealthKilled)
					+ " control=" + to_string(playerControlOn)
					+ " grapplePlayer=" + to_string(playerGrappleState)
					+ " grappleTarget=" + to_string(targetGrappleState)
					+ " meleePhasePlayer=" + to_string(playerMeleePhase)
					+ " meleePhaseTarget=" + to_string(targetMeleePhase)
					+ " aimedAt=" + to_string(aimedAt));
				logMessage("Shot, weapon " + hexString(shotWeapon) + ", window " + to_string(window) + "ms");
			}
		}

		ULONGLONG now = getTime();
		Ped stateTarget = PED::GET_MELEE_TARGET_FOR_PED(playerPed);
		if ((PED::IS_PED_IN_MELEE_COMBAT(playerPed) || stateTarget != 0)
			&& now - lastExecutionStateLog >= executionStateLogInterval)
		{
			int playerGrappleState = PED::GET_PED_GRAPPLE_STATE(playerPed);
			int targetGrappleState = stateTarget != 0 ? PED::GET_PED_GRAPPLE_STATE(stateTarget) : -1;
			float playerMeleePhase = PED::_GET_PED_MELEE_ACTION_PHASE(playerPed);
			float targetMeleePhase = stateTarget != 0 ? PED::_GET_PED_MELEE_ACTION_PHASE(stateTarget) : -1.0f;
			logMessage("EXEC_STATE melee=" + to_string(PED::IS_PED_IN_MELEE_COMBAT(playerPed))
				+ " target=" + to_string(stateTarget)
				+ " grapplePlayer=" + to_string(playerGrappleState)
				+ " grappleTarget=" + to_string(targetGrappleState)
				+ " meleePhasePlayer=" + to_string(playerMeleePhase)
				+ " meleePhaseTarget=" + to_string(targetMeleePhase));
			lastExecutionStateLog = now;
		}

		// Only clear the damage flag after observing it; an unconditional clear can erase a hit
		// that occurs between a false poll and the end of this frame.
		if (PLAYER::HAS_PLAYER_DAMAGED_AT_LEAST_ONE_PED(PLAYER::PLAYER_ID()))
		{
			if (shotPending && !hitSeen)
			{
				hitSeen = true;
				logMessage("Hit registered " + to_string(getTime() - shotStart) + "ms after first shot");
			}
			PLAYER::CLEAR_PLAYER_HAS_DAMAGED_AT_LEAST_ONE_PED(PLAYER::PLAYER_ID());
		}

		// Waiting on a shot: (watch for a hit, explode if the window runs out)
		if (shotPending)
		{
			if (ENTITY::GET_ENTITY_HEALTH(playerPed) <= 0)
			{
				cancelPendingShot(); // already dead, nothing to explode
			}
			else if (getTime() >= shotDeadline)
			{
				if (!hitSeen)
				{
					Vector3 playerPos = ENTITY::GET_ENTITY_COORDS(playerPed, true, false);
					ENTITY::SET_ENTITY_HEALTH(playerPed, 1, false); // guarantee death from the explosion
					FIRE::ADD_EXPLOSION(playerPos.x, playerPos.y, playerPos.z, 26, 10.0f, true, false, 10.0f);
					logMessage("EXPLODE - no hit within window (weapon now " + hexString(weaponNow) + ")");
				}
				else
				{
					logMessage("Window closed OK");
				}
				cancelPendingShot();
			}
		}

		prevWeapon = weaponNow;
		WAIT(0);
	}
}

void ScriptMain()
{
	main();
}
