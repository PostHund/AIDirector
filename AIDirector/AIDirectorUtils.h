#pragma once
#include "AIDirectorCommon.h"
#include "tge/math/Vector.h"
#include <vector>



class MainCamera;
class PlayerController;


class Object;

namespace Forge
{
	class AIDirector;


	struct AIDCell
	{
		uint32_t id       = DirectorStaticData::INVALID_ID;
		int8_t zCount     = 0;
		CellStatus status = CellStatus::Invalid;
	};

	class AreaSet
	{
	public:
		AreaSet() = default;

		AreaSet(const std::vector<int>& container) : myCellIds(container) {}
		AreaSet(const AreaSet& aSet) = default;

		AreaSet& operator=(const std::vector<int>& container);
		AreaSet& operator=(const AreaSet areaSet);
		AreaSet operator-(const AreaSet& setB) const;
		void operator+=(const AreaSet& setB);
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
		uint32_t cellId = DirectorStaticData::INVALID_ID;
		SpecialEnemy special = SpecialEnemy::None;
	};

	struct CrescendoPoint
	{
		uint32_t id = DirectorStaticData::INVALID_ID;
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


	struct DirectorDynamicData
	{
		// for statistics 
		CircularBuffer latestPlayerStress;
		uint32_t totalActiveEnemies = 0;
		float snapshotTimer = 0.f;

		//player stress
		float playerStress = 0.f;
		float stressTimer = 0.f; // tick down stress if player haven't been stressed for a while
		float takingDamageStressValue = 0.f;

		// SpawnZones
		float spawnZoneTimer = 0.f;

		// Phases
		Phases currentPhase = Phases::BuildUp;

		// Relax
		float relaxTimer = DirectorStaticData::RELAX_TIMER_RESET;

		// Build up
		float buildUpPhaseLength = DirectorStaticData::BUILD_UP_TIMER_RESET;
		float buildUpTimer = DirectorStaticData::BUILD_UP_TIMER_RESET;

		// peak
		float peakTimer = DirectorStaticData::PEAK_TIMER_RESET;

		// Horde 
		int hordeMinSize = DirectorStaticData::HORDE_MIN_SIZE;
		int hordeMaxSize = DirectorStaticData::HORDE_MAX_SIZE;

		float hordeMinInterval = DirectorStaticData::HORDE_TIMER_MIN_RESET;
		float hordeMaxInterval = DirectorStaticData::HORDE_TIMER_MAX_RESET;

		int nextHordeMax = DirectorStaticData::HORDE_MAX_SIZE;
		float hordeTimer = DirectorStaticData::HORDE_TIMER_MAX_RESET;

		// peak fade
		float peakFadeTimer = DirectorStaticData::PEAK_FADE_TIMER_RESET;

		// player
		Object* player = nullptr;
		PlayerController* playerController = nullptr;
		MainCamera* playerCamera = nullptr;
		bool playerLeftSpawn = false;

	};

	class DirectorCommand
	{
	public:
		DirectorCommand() = default;
		virtual ~DirectorCommand() = default;
		virtual void Update(float, AIDirector*) {}
		virtual bool CheckIsDone() const { return myIsDone; }
		virtual void SetIsDone(bool aIsDone) { myIsDone = aIsDone; }
	protected:
		bool myIsDone = false;
	};

	class DecompressCommand :public DirectorCommand
	{
	public:
		DecompressCommand() = default;
		DecompressCommand(AIDirector* aAiDirector);
		void Update(float fixedTime, AIDirector*) override;
	};

	class PlayerLeftSpawnCommand :public DirectorCommand
	{
		void Update(float fixedTime, AIDirector*) override;
	};
}

