#pragma once
#include <cstdint>
#include <tge/math/color.h>


namespace Forge
{
	enum class Phases : uint8_t
	{
		Relax,
		BuildUp,
		Peak,
		PeakFade
	};

	enum class CellStatus : uint8_t
	{
		Invalid,
		MainPath,
		OtherPath,
		ThreatZone,
		PlayerSpawnZone,
		DecompressZone,
		Count
	};

	enum class SpecialEnemy : uint8_t
	{
		Boomer,
		Chad,
		None,
		Streamer,
		Count
	};

	namespace DirectorStaticData
	{

		//world creation
		constexpr uint32_t INVALID_ID = UINT_MAX;
		constexpr uint32_t ENEMY_MAX_TOTAL_AMOUNT = 60;
		constexpr uint32_t ENEMY_MAX_SPAWN = 40;
		constexpr int OTHER_PATH_MAX_SPAWN = 1;
		constexpr int MAIN_PATH_MAX_SPAWN = 2;
		constexpr int THREAT_ZONE_AMOUNT = 3;
		constexpr int STATE_DICE = 6;
		constexpr int WANDER_CHANCE = 5;
		constexpr int NULL_CHANCE_SPAWN = -3;
		constexpr int SPAWN_ZONE_ROOF = 5;
		constexpr float HORDE_AREA_FIDELITY = 0.45f;

		//player stress
		constexpr float STRESS_MAX = 1.f;
		constexpr float STRESS_MODIFIER = 1.2f;
		constexpr float STRESS_PEAK_THRESHOLD = 0.99f;
		constexpr float STRESS_MIN_THRESHOLD = 0.05f;
		constexpr float STRESS_DURATION_RESET = 10.f; // how long the player stays stressed, before stress starting to tick down
		constexpr float STRESS_DOWN_TICK = 0.05f; // how much stress ticks down per second
		constexpr float KILL_RANGE_SQR = 500.f * 500.f;
		constexpr float SPLATTER_RANGE_SQR = 300.f * 300.f;
		constexpr float ENEMY_KILLED_STRESS_VALUE = 0.01f; // if enemy killed within 8 meters stress goes up
		constexpr float TAKING_DAMAGE_STRESS_VALUE = 0.05f; //  if player gets hit tress goes up
		constexpr float ATTACKED_BY_SPECIAL_STRESS_VALUE = STRESS_MAX;

		// SpawnZones
		constexpr float UPDATE_SPAWN_ZONE_TIME = 0.75f;

		// Phases
		constexpr float RELAX_TIMER_RESET = 15.f;
		constexpr float BUILD_UP_TIMER_RESET = 90.f;
		constexpr float PEAK_TIMER_RESET = 0.f;
		constexpr float PEAK_MAX_TIME = 60.f;
		constexpr float PEAK_FADE_TIMER_RESET = 15.f;

		// Horde 
		constexpr float HORDE_TIMER_MIN_RESET = 10.f;
		constexpr float HORDE_TIMER_MAX_RESET = 20.f;
		constexpr int HORDE_MIN_SIZE = 2;
		constexpr int HORDE_MAX_SIZE = 9;
		constexpr float HIDDEN = -5000000.f;

		// for statistics 
		constexpr float SNAP_SHOT_TIME = 0.016666f;
	}


	static const Tga::Color GRID_COLORS[static_cast<int>(Forge::CellStatus::Count)]
	{
		{1.f,1.f,1.f,1.f},		// Invalid, white
		{0.f,.7f,0.f,1.f},		// MainPath, green
		{.7f,.7f,0.f,1.f},		// OtherPath, yellow
		{1.f,0.f,0.f,1.f},		// ThreatZone, red
		{1.f,1.f,1.f,1.f}		// player spawn zone, white
	};

}
