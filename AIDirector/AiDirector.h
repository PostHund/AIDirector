#pragma once
#include "Animation/AnimatedModel.h"
#include "Utilities/Blackboard.h"
#include "Utilities/AIDirector/AIDirectorUtils.h"
#include "Controllers/CommonController.h"
#include "Events/GameEvent.h"
#include "Events/IEventListener.h"

#include <vector>
#include <algorithm>
#include <tge/math/color.h>
#include <tge/util/Frustum.h>

class Navmesh;
class AnimatedModelComponent;

namespace Tga
{
	struct SceneModel;
}

class PlayerController;
class MainCamera;
enum class ObjectType : uint8_t;

class Object;

namespace Forge
{

	class AIDirector : public IEventListener<GameEvent>
	{
		friend class PlayerLeftSpawnCommand;
		friend class DecompressCommand;

	public:
		AIDirector() = default;
		~AIDirector() override;

		void Init();
#ifndef _RETAIL
		void SetDirectorData();
		void PlayerDebug(std::string& debugText);
		void EnemyDebug(std::string& debugText);
		void MiniMapDebug(std::string& debugText);
		void AddDebugMenu();
		void DebugDraw();
		void DrawFrustum();
		bool CanDrawDebug();
		bool CanDrawEnemyDebug();
#endif
		void Update(float aFixedTime);
		bool IsInitialized() const { return myInitialized; }
		int GetCellIndexFromPosition(const Tga::Vector3f& aPosition);
		Tga::Vector3f GetXZPositionFromIndex(const int aIndex) const;
		std::vector<int> GetClosestNeighborCells(int aCellIndex) const;
		AreaSet CalculateActiveAreaCells(int aCellIndex) const;
		Object* GetMyPlayer() const;
		void OnEvent(const GameEvent& e) override;

		void CreateEnemies();
		void SpawnMob(Tga::Vector3f& aFrom, Tga::Vector3f& aTo, int amount);

		const std::array<Object*, ENEMY_MAX_TOTAL_AMOUNT>& GetEnemies() const { return myEnemies; }

		void ProvideCrescendoPoint(int id, Tga::Vector3f& aPos);
		void ProvideDecompressZone(Object* aZone);
		const CrescendoPoint* GetCrescendoPoint(int id) const;
		Tga::Vector3f GetClosestSpawnPoint() const;
		void Clear();

	private:
		bool CreateGridAndMapWorld();
		void PopulateWorld();
		Object* InstantiateEnemy(Tga::StringId anObjectDefinitionName, ObjectType anObjectType);
		void CreateModelAnimations(const Tga::SceneModel& aSceneModel, AnimatedModelComponent& aModelComponent);

		static void DespawnEnemy(Object* aEnemy);
		void SpawnEnemy(Object* aEnemy, const Tga::Vector3f& zone);
		void MakeEnemyPathFind(Object* aEnemy, const std::vector<Tga::Vector3f>& aPath);

		void UpdateActiveArea(float fixedTime);
		void UpdateSpawnZones(float fixedTime);
		void UpdateHorde(float aFixedTime);
		void HandleCommands(float aFixedTime);
		void UpdateCurrentPhase(float aFixedTime);

		bool CheckIf3DPosInFrustum(const Frustum& aFrustum, const Tga::Vector3f& aCenter, float aRadius);
		bool CheckIfXZPlanePosInFrustum(const Frustum& aFrustum, const Tga::Vector3f& aCenter);
		const Tga::Matrix4x4f CalculatePlayerPhysMatrix();

	private:
		std::vector<AIDCell> myGrid;
		
		// std::vector<Tga::Vector3f> myCellCenters;
		std::vector<CrescendoPoint> myCrescendoPoints;
		std::vector<std::pair<float, Tga::Vector3f>> myDecompressZones;
		std::vector<ThreatZone> myThreatZones;

		std::vector<Tga::Vector3f> myRoughPath;
		std::vector<Tga::Vector3f> myMainPath;
		std::vector<Tga::Vector3f> myMobToPlayerPath;
		AreaSet myActiveArea;
		AreaSet myExitedArea;
		AreaSet myEnteredArea;
		AreaSet myPossibleHordeSpawnZones;
		AreaSet myCellIdsInFrustum;

		std::vector<std::shared_ptr<DirectorCommand>> myCommands;

		std::array<Object*, ENEMY_MAX_TOTAL_AMOUNT> myEnemies{ nullptr };
		CommonData myCommonZombieData;
		bool myHasCommonData       = false;
		std::vector<std::pair<Tga::StringId, Tga::StringId>> myCommonAnimationsStrings;
		std::unordered_map<AnimationId, Tga::AnimationPlayer> myAnimationStates;
		bool myHasCommonAnimationsStrings = false;
		bool myHasCommonAnimationPlayers = false;

		Tga::Vector3f mySmallestMapPosition;
		Tga::Vector3f myLargestMapPosition;
		float myAverageMapHeight = 0.f;

		Object* myLevelEnd;
		DirectorDynamicData myData;
		Frustum myPlayerFrustum;

		float myGridCellSideLength = 600.f; //1700.f
		const float myGridCellHalfLength = myGridCellSideLength / 2.f;
		float myTotalGridSideLength = 1000.f;

		/*float myCommonZombieShoulderWith = 200.f;
		float myCommonZombieMoveForce = 200.f;*/
		float myActiveAreaRadius = 6000.f;

		int myAmountOfRows = 0;
		int myAmountOfColumns = 0;
		int myNumberOfCells = 0;
		uint16_t myNumLines;
		bool myInitialized = false;

		Navmesh* myNavmesh = nullptr;
		Blackboard<(std::numeric_limits<short>::max)()>* myBlackboard;

#ifndef _RETAIL
		uint16_t debugId = INT16_MAX;
		static bool ourHasDebugMenu;
		bool myRenderMobPath = false;
		bool myRenderDebug = false;
		bool myEnemiesDebug = false;
		bool myActiveAreaDebug = false;
		bool myDrawFrustumDebug = false;
		bool myPlayerDebug = false;
		bool myGridDebug = false;
#endif


	};

	
}
