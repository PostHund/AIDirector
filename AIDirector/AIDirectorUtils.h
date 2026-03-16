#pragma once
#include "tge/math/Vector.h"
#include <vector>
#include <cstdint>
#include <tge/math/color.h>

class MainCamera;
class PlayerController;
constexpr uint32_t INVALID_ID = UINT_MAX;
constexpr uint32_t ENEMY_MAX_TOTAL_AMOUNT = 60;
constexpr uint32_t ENEMY_MAX_SPAWN = 40;
constexpr int OTHER_PATH_MAX_SPAWN = 1;
constexpr int MAIN_PATH_MAX_SPAWN = 2;
constexpr int THREAT_ZONE_AMOUNT = 3;
constexpr int STATE_DICE = 6;
constexpr int WANDER_CHANCE = 5;
constexpr int NULL_CHANCE_SPAWN = -3;


constexpr float HIDDEN = -5000000.f;

class Object;

namespace Forge
{
	class AIDirector;
	enum class Phases : uint8_t
	{
		Relax,
		BuildUp,
		Peak
	};

	enum class CellStatus : uint8_t
	{
		Invalid,
		MainPath,
		OtherPath,
		ThreatZone,
		PlayerSpawnZone,
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

	struct AidCell
	{
		uint32_t id = INVALID_ID;
		int8_t zCount = 0;
		CellStatus status = CellStatus::Invalid;
	};

	class AreaSet
	{
	public:
		AreaSet() = default;

		AreaSet(const std::vector<int>& container) : myCellIds(container) {}
		AreaSet(const AreaSet& aSet) = default;

		AreaSet& operator=(const AreaSet areaSet);
		AreaSet operator-(AreaSet& setB) const;
		AreaSet operator-(const AreaSet& setB) const;
		void operator+=(AreaSet& setB);
		void operator+=(const AreaSet& setB);
		void operator-=(AreaSet& setB);
		void operator-=(const AreaSet& setB);

		void Insert(int cellId);
		bool Contains(const int cellId);

		void Clear();
		size_t Size() const;

		std::vector<int>& AccessCells() { return myCellIds; }
		const std::vector<int>& GetCells()const { return myCellIds; }

	private:

		std::vector<int> myCellIds;
	};

	struct ThreatZone
	{
		Tga::Vector3f spawnPoint = Tga::Vector3f::Zero;
		uint32_t cellId = INVALID_ID;
		SpecialEnemy special = SpecialEnemy::None;
	};

	struct CrescendoPoint
	{
		uint32_t id = INVALID_ID;
		Tga::Vector3f position = Tga::Vector3f::Zero;
	};


	class CircularBuffer
	{

	public:
		static constexpr size_t MAX_SIZE = 600;
		void Add(float aValue)
		{
			myValues[currentIndex] = aValue;
			--currentIndex;
			if (currentIndex == -1)
			{
				currentIndex = MAX_SIZE - 1;
			}
		}
		size_t GetCurrentIndex() const { return currentIndex; }
		std::array<float, MAX_SIZE>& GetValues() { return myValues; }

	private:
		std::array<float, MAX_SIZE> myValues = { 0.f };
		size_t currentIndex = MAX_SIZE - 1;
	};

	struct DirectorData
	{
		// for statistics 
		CircularBuffer latestPlayerStress;
		uint32_t totalActiveEnemies = 0;
		float snapshotTimer = 0.f;
		static constexpr float SNAP_SHOT_TIME = 0.016666f;

		//player stress
		float playerStress = 0.f;
		float stressTimer = 0.f; // tick down stress if player haven't been stressed for a while
		static constexpr float STRESS_MAX = 1.f;
		static constexpr float STRESS_PEAK_THRESHOLD = 0.9f;
		static constexpr float STRESS_DURATION_RESET = 10.f; // how long the player stays stressed, before stress starting to tick down
		static constexpr float STRESS_DOWN_TICK = 0.05f; // how much stress ticks down per second
		static constexpr float KILL_RANGE_SQR = 800.f * 800.f;
		static constexpr float ENEMY_KILLED_STRESS_VALUE = 0.01f; // if enemy killed within 8 meters stress goes up
		static constexpr float TAKING_DAMAGE_STRESS_VALUE = 0.05f; //  if player gets hit tress goes up
		static constexpr float ATTACKED_BY_SPECIAL_STRESS_VALUE = STRESS_MAX;

		// Phases
		Phases currentPhase = Phases::BuildUp;

		// Mob (build up)
		static constexpr float MOB_TIMER_MIN_RESET = 40.f;
		static constexpr float MOB_TIMER_MAX_RESET = 80.f;
		static constexpr float MOB_TIMER_TICK      = 1.f;
		static constexpr int MOB_MIN_SIZE   = 2;
		static constexpr int MOB_MAX_SIZE   = 7;
		int nextMobMax = MOB_MAX_SIZE;
		float mobTimer                             = MOB_TIMER_MAX_RESET;

		// Relax
		static constexpr float RELAX_TIMER_RESET = 30.f;
		static constexpr float RELAX_TIMER_TICK = 1.f;
		float relaxTimer = RELAX_TIMER_RESET;

		// peak

		static constexpr float PEAK_TIMER_RESET = 10.f;
		static constexpr float PEAK_TIMER_TICK = 1.f;
		float peakTimer = PEAK_TIMER_RESET;

		// player
		Object* player = nullptr;
		PlayerController* playerController = nullptr;
		MainCamera* playerCamera = nullptr;
		bool playerLeftSpawn = false;
	};

	class DirectorCommand
	{
	public:
		virtual ~DirectorCommand() = default;
		virtual void Update(float, AIDirector*) {}
		virtual bool CheckIsDone() const { return myIsDone; }
		virtual void SetIsDone(bool aIsDone) { myIsDone = aIsDone; }
	protected:
		bool myIsDone = false;
	};

	class PlayerLeftSpawnCommand :public DirectorCommand
	{
		void Update(float fixedTime, AIDirector*) override;
	};

	class DecompressCommand :public DirectorCommand
	{
		void Update(float fixedTime, AIDirector*) override;
	};
}

static const Tga::Color GRID_COLORS[static_cast<int>(Forge::CellStatus::Count)]
{
	{1.f,1.f,1.f,1.f},		// Invalid, white
	{0.f,.7f,0.f,1.f},		// MainPath, green
	{.7f,.7f,0.f,1.f},		// OtherPath, yellow
	{1.f,0.f,0.f,1.f},		// ThreatZone, red
	{1.f,1.f,1.f,1.f}		// player spawn zone, white
};
