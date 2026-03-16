#include "AIDirector.h"
#include "AudioManager.h"
#include "CameraManager.h"

#include "ObjectManager.h"
#include "Components/AnimatedModelComponent.h"
#include "Components/ModelComponent.h"

#include "Controllers/Controller.h"
#include "Objects/Object.h"
#include "Utilities/Locator.h"
#include "Utilities/RandomNumberGenerator.h"
#include "Utilities/Navmesh/navmesh.h"
#include "Camera/MainCamera.h"
#include "Controllers/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Components/HealthComponent.h"
#include "Controllers/PathfindState.h"
#include "Controllers/DeathState.h"
#include "Controllers/PipeBombState.h"
#include "Controllers/SteeringBehaviours.h"

#include <forge/Render/Renderer.h>

#include <tge/Engine.h>
#include <tge/drawers/DebugDrawer.h>
#include <tge/drawers/LineDrawer.h>
#include <tge/graphics/GraphicsEngine.h>
#include <tge/primitives/LinePrimitive.h>
#include <tge/script/BaseProperties.h>

#include <algorithm>
#include <set>
#include <unordered_set>
#include <IconFontHeaders/IconsLucide.h>
#include <imgui/imgui.h>
#include <tge/imgui/ImGuiInterface.h>
#include <tge/scene/ScenePropertyExtractor.h>
#include <tge/shaders/ModelShader.h>

#ifndef _RETAIL
bool Forge::AIDirector::ourHasDebugMenu = false;
#endif

Forge::AIDirector::~AIDirector()
{
	EventManager::Get()->RemoveListener(myEventHandler);
}

void Forge::AIDirector::Init()
{
#ifndef _RETAIL
	AddDebugMenu();
#endif
	myInitialized = false;
	// myCellCenters.clear();
	myGrid.clear();
	myBlackboard = &ourAIBlackboard;
	// myPlayer = nullptr;
	myData.player = nullptr;
	myLevelEnd = nullptr;
	myData.playerController = nullptr;


	Object* levelTransition = Locator::GetObjectManager()->FindObjectWithType(ObjectType::LevelTransition);
	Object* player = Locator::GetObjectManager()->FindObjectWithType(ObjectType::Player);
	myNavmesh = Locator::GetNavmesh();
	if (levelTransition == nullptr || player == nullptr || myNavmesh == nullptr)
	{
		std::cout << "AIDirector FAILED!! in init()\n";
		return;
	}

	// myPlayer = player;
	myData.player = player;
	myData.playerCamera = Locator::GetCameraManager()->GetMainCamera();
	myLevelEnd = levelTransition;
	myData.playerController = myData.player->GetComponent<ActorComponent>()->GetController<PlayerController>();
	myBlackboard->SetValue("PlayerPos"_tgaid, myData.player->GetTransform().GetPosition());
	if (!CreateGridAndMapWorld())
	{
		std::cout << "AIDirector FAILED!! in init()\n";
		return;
	}

	// CreateEnemies();
	myInitialized = true;

	PopulateWorld();

	myCommands.emplace_back(std::make_shared<PlayerLeftSpawnCommand>());

}

#ifndef _RETAIL
void Forge::AIDirector::AddDebugMenu()
{
	if (debugId != UINT16_MAX && ourHasDebugMenu == false)
	{
		ourHasDebugMenu = true;
		debugId = Locator::GetDebugTool()->AddFunc("AI Director", [this]()
			{
				if (!myInitialized) { return; }
				ImGui::ShowDemoWindow();

				static std::string debugText;


				// PLAYER DEBUG
				ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
				ImGui::Checkbox(ICON_LC_PERSON_STANDING, &myPlayerDebug);
				ImGui::PopFont();
				ImGui::SetItemTooltip("Player Debug Data");

				if (myPlayerDebug)
				{

					ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
					ImGui::Text(ICON_LC_TARGET);
					ImGui::PopFont();
					ImGui::SameLine();
					ImGui::DragFloat("Active Area Radius", &myActiveAreaRadius, 100.f, myGridCellHalfLength, 100000.f, "%.1f");

					ImGui::Text("Player Stress Data");
					float average = 0.f;
					auto& stressRegisters = myData.latestPlayerStress.GetValues();
					int valueAmount = static_cast<int>(stressRegisters.size());
					int currentStressIndex = (static_cast<int>(CircularBuffer::MAX_SIZE) - static_cast<int>(myData.latestPlayerStress.GetCurrentIndex())) - 1;
					static const Tga::Vector3f green = { 0.f, 1.f, 0.f };
					static const Tga::Vector3f red = { 1.f, 0.f, 0.f };
					for (int s = 0; s < valueAmount; ++s)
					{
						average += stressRegisters[s];
					}
					average /= static_cast<float>(valueAmount);
					const Tga::Vector3f averageBlend = FMath::Lerp(green, red, average);
					const Tga::Vector3f currentBlend = FMath::Lerp(green, red, myData.playerStress);


					ImGui::PushStyleColor(ImGuiCol_FrameBg, { currentBlend.x * 0.4f, currentBlend.y * 0.4f, currentBlend.z * 0.4f, 1.f });
					ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, { currentBlend.x * 0.6f, currentBlend.y * 0.6f, currentBlend.z * 0.6f, 1.f });
					ImGui::PushStyleColor(ImGuiCol_FrameBgActive, { currentBlend.x * 0.8f, currentBlend.y * 0.8f, currentBlend.z * 0.8f, 1.f });
					ImGui::PushStyleColor(ImGuiCol_SliderGrab, { currentBlend.x, currentBlend.y, currentBlend.z, 1.f });
					float playerStress = myData.playerStress;
					ImGui::VSliderFloat("current stress", ImVec2(18, 80), &playerStress, 0.0f, 1.0f, "");
					ImGui::PopStyleColor(4);

					ImGui::SameLine();
					char overlay[64];
					sprintf_s(overlay, "avg. last 10 seconds  %.*f", 2, average);

					ImGui::PushStyleColor(ImGuiCol_PlotLines, { averageBlend.x, averageBlend.y, averageBlend.z, 1.f });
					ImGui::PlotLines("average stress", myData.latestPlayerStress.GetValues().data(), valueAmount, currentStressIndex, overlay, 0.0f, 1.0f, ImVec2(0, 80.0f));
					ImGui::SameLine();
					ImGui::TextColored({ green.x, green.y, green.z, 1.f }, "0 = min");
					ImGui::SameLine();
					ImGui::TextColored({ averageBlend.x, averageBlend.y, averageBlend.z, 1.f }, " << intensity >>");
					ImGui::SameLine();
					ImGui::TextColored({ red.x, red.y, red.z, 1.f }, "1 = max");
					ImGui::PopStyleColor(1);
				}

				ImGui::Separator();
				ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
				ImGui::Checkbox(ICON_LC_SKULL, &myEnemiesDebug);
				ImGui::PopFont();
				ImGui::SetItemTooltip("Open Enemy Debug");

				// ENEMIES DEBUG
				if (myEnemiesDebug)
				{
					static Tga::Vector3f startPos = Tga::Vector3f::Zero;
					static Tga::Vector3f endPos = Tga::Vector3f::Zero;

					ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
					ImGui::Checkbox(ICON_LC_WAYPOINTS, &myRenderMobPath);
					ImGui::PopFont();
					ImGui::SetItemTooltip("Show mob path");

					if (ImGui::Button("Path find smooth"))
					{

						if (myNavmesh)
						{
							const std::vector<Tga::Vector3f> smoothPath = myNavmesh->PathfindFunneled(startPos, endPos);
							for (Object* enemy : myEnemies)
							{
								if (enemy != nullptr)
								{
									enemy->GetTransform().SetPosition(startPos);
									MakeEnemyPathFind(enemy, smoothPath);
								}
							}

						}
					}
					ImGui::SameLine();
					if (ImGui::Button("Set end pos to player position"))
					{
						endPos = myData.player->GetTransform().GetPosition();
					}
					ImGui::SameLine();
					if (ImGui::Button("Set start pos to player position"))
					{
						startPos = myData.player->GetTransform().GetPosition();
					}

					static Object* currentInspection = nullptr;
					static int currentIndex = 0;
					static std::string currentZombieName =/* "Zombie #" + */std::to_string(currentIndex);
					static std::string zombieInfo = "";
					static std::string selectedZombie = "";
					if (ImGui::BeginCombo("Select zombie", currentZombieName.c_str()))
					{
						for (int enemyIndex = 0; enemyIndex < ENEMY_MAX_TOTAL_AMOUNT; ++enemyIndex)
						{
							const bool is_selected = (currentIndex == enemyIndex);

							if (myEnemies[enemyIndex]->CheckIfActive())
							{
								zombieInfo = std::to_string(enemyIndex) + " - active";
							}
							else
							{
								zombieInfo = std::to_string(enemyIndex) + " - not active";
							}

							if (ImGui::Selectable(zombieInfo.c_str(), is_selected))
							{
								currentIndex = enemyIndex;
								currentZombieName = std::to_string(currentIndex);
								currentInspection = myEnemies[currentIndex];
								selectedZombie = "Zombie " + currentZombieName;
							}
						}
						ImGui::EndCombo();
					}

					if (currentInspection != nullptr)
					{
						ImGui::Text(selectedZombie.c_str());
						if (ImGui::Button("Toggle active"))
						{
							const bool state = currentInspection->CheckIfActive();
							currentInspection->SetActive(!state);
						}
						if (currentInspection->CheckIfActive())
						{
							if (ImGui::Button("Kill"))
							{
								CommonController* const controller = currentInspection->GetComponent<CommonController>();
								if (controller->GetCurrentStateId() != CommonState::Death)
								{
									controller->QueueNextState(CommonState::Death);
								}
							}
						}
						const Tga::Vector3f& pos = currentInspection->GetTransform().GetPosition();
						ImGui::Text("X: %i, Y: %i, Z: %i", static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z));
						if (ImGui::Button("Inspect"))
						{
							CameraManager* cameraManager = Locator::GetCameraManager();

							if (cameraManager != nullptr)
							{
								CameraType camType = cameraManager->GetActiveCameraType();
								if (camType != CameraType::Free)
								{
									cameraManager->SwitchCamera();
								}
								Tga::Camera& camera = cameraManager->GetActiveCamera();

								camera.GetTransform().SetPosition(currentInspection->GetTransform().GetPosition());
							}
						}

						ImGui::Text("Animation Debug");
						AnimatedModelComponent* animatedModel = currentInspection->GetComponent<AnimatedModelComponent>();

						debugText = "current animation: " + std::string(animatedModel->GetCurrentAnimationId().GetString());
						ImGui::Text(debugText.c_str());
						static float animationSpeed = 1.f;
						ImGui::PushItemWidth(50.f);
						if (ImGui::DragFloat("animation speed", &animationSpeed, 0.01f, 0.1f, 3.f, "%.2f"))
						{
							animatedModel->SetTimeScale(animationSpeed);
						}


						ImGui::Text("Ability Debug");
						ImGui::SetItemTooltip("This does not save the data, only for play test");
						CommonController* controller = currentInspection->GetComponent<CommonController>();

						auto& data = controller->AccessData();

						ImGui::DragInt("Set Damage", &data.damage, 1, 1, 100);
						ImGui::DragFloat("Set Top Sprint Speed", &data.sprintSpeed, 1.f, 100.f, 10000.f);
						ImGui::DragFloat("Set Top Wander Speed", &data.wanderSpeed, 1.f, 100.f, 10000.f);
						ImGui::DragFloat("Set Acceleration Force", &data.force, 1.f, 100.f, 10000.f);

						if (ImGui::Button("Give these ability and animation settings for all"))
						{
							for (auto enemy : myEnemies)
							{
								CommonController* commonController = enemy->GetComponent<CommonController>();

								auto& otherData = commonController->AccessData();
								otherData.damage = data.damage;
								otherData.sprintSpeed = data.sprintSpeed;
								otherData.wanderSpeed = data.wanderSpeed;
								otherData.force = data.force;

								auto anim = enemy->GetComponent<AnimatedModelComponent>();
								anim->SetTimeScale(animationSpeed);
							}
						}
					}
				}
				ImGui::Separator();
				ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
				ImGui::Checkbox(ICON_LC_PENCIL_LINE, &myRenderDebug);
				ImGui::PopFont();
				ImGui::SetItemTooltip("Draw Debug Data");

				//DRAW DEBUGLINES IN WORLD
				if (myRenderDebug)
				{
					ImGui::SameLine();
					ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
					ImGui::Checkbox(ICON_LC_TRIANGLE, &myDrawFrustumDebug);
					ImGui::PopFont();
					ImGui::SetItemTooltip("Show view frustum");
					// same line all the possible debug draws, and divide the already existing ones.
				}

				// MINIMAP DEBUG
				ImGui::Separator();
				ImGui::PushFont(Tga::ImGuiInterface::GetIconFontLarge());
				ImGui::Checkbox(ICON_LC_GRID_3X3, &myGridDebug);
				ImGui::PopFont();
				ImGui::SetItemTooltip("Debug World Grid");

				if (myGridDebug && myInitialized)
				{
					auto getGridColor = [](CellStatus aStatus) -> std::array<int, 4>
						{
							auto c = GRID_COLORS[static_cast<int>(aStatus)];
							return { static_cast<int>(c.r * 255.f),
								static_cast<int>(c.g * 255.f),
								static_cast<int>(c.b * 255.f),
								static_cast<int>(c.a * 255.f) };

						};

					auto displayButtonWithColor = [](const char* text, ImU32 col)
						{
							ImGui::PushStyleColor(ImGuiCol_Button, col);
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
							ImGui::Button(text);
							ImGui::PopStyleColor(3);
						};

					auto buttonCol = getGridColor(CellStatus::OtherPath);
					displayButtonWithColor("Other Path", IM_COL32(buttonCol[0], buttonCol[1], buttonCol[2], buttonCol[3]));

					ImGui::SameLine();
					buttonCol = getGridColor(CellStatus::MainPath);
					displayButtonWithColor("Main Path", IM_COL32(buttonCol[0], buttonCol[1], buttonCol[2], buttonCol[3]));

					ImGui::SameLine();
					buttonCol = getGridColor(CellStatus::ThreatZone);
					displayButtonWithColor("Threat Zone", IM_COL32(buttonCol[0], buttonCol[1], buttonCol[2], buttonCol[3]));
					static bool showActiveArea = false;
					displayButtonWithColor("A", IM_COL32(0, 255, 255, 255));
					ImGui::SameLine();
					ImGui::Checkbox("Player active area", &showActiveArea);
					ImGui::SameLine();
					displayButtonWithColor("F", IM_COL32(127, 0, 255, 255));
					ImGui::SameLine();
					static bool showActiveFrustum = false;
					ImGui::Checkbox("In frustum", &showActiveFrustum);

					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
					displayButtonWithColor("S", IM_COL32(255, 255, 255, 255));
					ImGui::PopStyleColor(1);
					ImGui::SameLine();
					static bool showPossibleHordeSpawn = false;
					ImGui::Checkbox("Potential Horde Spawn", &showPossibleHordeSpawn);
					ImDrawList* const drawList = ImGui::GetWindowDrawList();





					// Background


					ImGui::BeginChild("World Grid", ImVec2(0, 0), true);
					ImVec2 canvasP0 = ImGui::GetCursorScreenPos();
					ImVec2 canvasSize = ImGui::GetContentRegionAvail();
					if (canvasSize.x < 50) canvasSize.x = 50;
					if (canvasSize.y < 50) canvasSize.y = 50;

					ImVec2 canvasP1 = ImVec2(canvasP0.x + canvasSize.x,
						canvasP0.y + canvasSize.y);
					drawList->PushClipRect(canvasP0, canvasP1, true);
					drawList->AddRectFilled(canvasP0, canvasP1,
						IM_COL32(50, 50, 50, 255));
					drawList->AddRect(canvasP0, canvasP1,
						IM_COL32(255, 255, 255, 255));

					// Grid
					const float gridStepX = canvasSize.x / static_cast<float>(myAmountOfColumns);
					const float gridStepY = canvasSize.y / static_cast<float>(myAmountOfRows);
					const float worldWidth = myLargestPosition.x - mySmallestNodePosition.x;
					const float worldHeight = myLargestPosition.z - mySmallestNodePosition.z;
					const float scaleX = canvasSize.x / worldWidth;
					const float scaleY = canvasSize.y / worldHeight;
					const float scale = std::min(scaleX, scaleY);
					const float offsetX = canvasP0.x + (canvasSize.x - worldWidth * scale) * 0.5f;
					const float offsetY = canvasP0.y + (canvasSize.y - worldHeight * scale) * 0.5f;

					for (float x = 0; x < canvasSize.x; x += gridStepX)
					{
						drawList->AddLine(
							ImVec2(canvasP0.x + x, canvasP0.y),
							ImVec2(canvasP0.x + x, canvasP1.y),
							IM_COL32(200, 200, 200, 40));
					}

					for (float y = 0; y < canvasSize.y; y += gridStepY)
					{
						drawList->AddLine(
							ImVec2(canvasP0.x, canvasP0.y + y),
							ImVec2(canvasP1.x, canvasP0.y + y),
							IM_COL32(200, 200, 200, 40));
					}


					auto gridPosFromIndex = [this, canvasP0, canvasP1, gridStepX, gridStepY](const int index) ->ImVec2
						{
							const int col = index % myAmountOfColumns;
							const int row = index / myAmountOfColumns;
							const int flippedRow = (myAmountOfRows - 1) - row;
							const float x = canvasP0.x + col * gridStepX + gridStepX * 0.5f;
							const float y = canvasP0.y + flippedRow * gridStepY + gridStepY * 0.5f;
							return { x, y };
						};


					auto getGridPos = [this, gridPosFromIndex, canvasP0, canvasP1, gridStepX, gridStepY, offsetX, offsetY, scale](const Tga::Vector3f& pos) ->ImVec2
						{
							const int cellIndex = GetCellIndexFromPosition(pos);
							const Tga::Vector3f cellCenter = /*myCellCenters[cellIndex]*/GetXZPositionFromIndex(cellIndex);
							const float xDelta = pos.x - cellCenter.x;
							const float zDelta = pos.z - cellCenter.z;
							const float xOffset = (xDelta / myGridCellSideLength) * gridStepX;
							const float yOffset = (zDelta / myGridCellSideLength) * gridStepY;
							ImVec2 cellPos = gridPosFromIndex(cellIndex);
							return { cellPos.x + xOffset, cellPos.y - yOffset };

						};

					std::vector<ImVec2> gridCells;
					gridCells.reserve(50);

					for (auto& cell : myGrid)
					{
						if (cell.status != CellStatus::Invalid)
						{
							gridCells.emplace_back(gridPosFromIndex(cell.id));
						}
					}
					for (auto& c : gridCells)
					{
						const auto col = getGridColor(CellStatus::OtherPath);
						const ImVec2 min = c - ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);
						const ImVec2 max = c + ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);;
						drawList->AddRectFilled(min, max, IM_COL32(col[0], col[1], col[2], col[3]));
					}
					gridCells.clear();

					for (auto& cell : myGrid)
					{
						if (cell.status == CellStatus::MainPath)
						{
							gridCells.emplace_back(gridPosFromIndex(cell.id));
						}
					}
					for (auto& c : gridCells)
					{
						const auto col = getGridColor(CellStatus::MainPath);
						const ImVec2 min = c - ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);
						const ImVec2 max = c + ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);;
						drawList->AddRectFilled(min, max, IM_COL32(col[0], col[1], col[2], col[3]));
					}

					gridCells.clear();
					for (auto& cell : myGrid)
					{
						if (cell.status == CellStatus::ThreatZone)
						{
							gridCells.emplace_back(gridPosFromIndex(cell.id));
						}
					}
					for (auto& c : gridCells)
					{
						const auto col = getGridColor(CellStatus::ThreatZone);
						const ImVec2 min = c - ImVec2(gridStepX * 1.5f, gridStepY * 1.5f);
						const ImVec2 max = c + ImVec2(gridStepX * 1.5f, gridStepY * 1.5f);;
						drawList->AddRectFilled(min, max, IM_COL32(col[0], col[1], col[2], col[3]));
					}

					if (showActiveArea)
					{
						gridCells.clear();
						for (auto& id : myActiveArea.GetCells())
						{
							const ImVec2 c = gridPosFromIndex(id);
							const ImVec2 min = c - ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);
							const ImVec2 max = c + ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);;
							drawList->AddRectFilled(min, max, IM_COL32(0, 255, 255, 255));
						}
					}

					if (showActiveFrustum)
					{
						gridCells.clear();
						for (auto& id : myCellIdsInFrustum.GetCells())
						{
							const ImVec2 c = gridPosFromIndex(id);
							const ImVec2 min = c - ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);
							const ImVec2 max = c + ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);;
							drawList->AddRectFilled(min, max, IM_COL32(127, 0, 255, 255));
						}
					}

					if (showPossibleHordeSpawn)
					{
						gridCells.clear();

						int spawnzone = 0;
						const int zoneAmount = static_cast<int>(myPossibleHordeSpawnZones.GetCells().size());
						for (auto& id : myPossibleHordeSpawnZones.GetCells())
						{
							const float blend = static_cast<float>(spawnzone) / static_cast<float>(zoneAmount);

							const int color = static_cast<int>(FMath::Lerp(255.f, 0.f, blend));

							const ImVec2 c = gridPosFromIndex(id);
							const ImVec2 min = c - ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);
							const ImVec2 max = c + ImVec2(gridStepX * 0.5f, gridStepY * 0.5f);;
							drawList->AddRectFilled(min, max, IM_COL32(color, color, color, 255));
							spawnzone++;
						}

					}

					std::vector<std::pair<ImVec2, CommonState>> enemyDots;
					enemyDots.reserve(ENEMY_MAX_TOTAL_AMOUNT);
					for (auto enemy : myEnemies)
					{
						if (enemy && enemy->CheckIfActive())
						{
							const Tga::Vector3f& pos = enemy->GetTransform().GetPosition();
							auto controller = enemy->GetComponent<CommonController>();
							if (controller)
							{
								const CommonState state = controller->GetCurrentStateId();
								enemyDots.emplace_back(getGridPos(pos), state);
							}
						}
					}

					ImU32 stateColor;
					for (auto& p : enemyDots)
					{
						if (p.second == CommonState::Death)
						{
							stateColor = (((ImU32)(255) << IM_COL32_A_SHIFT) | ((ImU32)(0) << IM_COL32_B_SHIFT) | ((ImU32)(0) << IM_COL32_G_SHIFT) | ((ImU32)(0) << IM_COL32_R_SHIFT));
						}
						else if (p.second != CommonState::Idle && p.second != CommonState::Wander)
						{
							stateColor = (((ImU32)(255) << IM_COL32_A_SHIFT) | ((ImU32)(127) << IM_COL32_B_SHIFT) | ((ImU32)(0) << IM_COL32_G_SHIFT) | ((ImU32)(255) << IM_COL32_R_SHIFT));
						}
						else
						{
							stateColor = (((ImU32)(255) << IM_COL32_A_SHIFT) | ((ImU32)(200) << IM_COL32_B_SHIFT) | ((ImU32)(200) << IM_COL32_G_SHIFT) | ((ImU32)(200) << IM_COL32_R_SHIFT));
						}
						drawList->AddCircleFilled(p.first, 5.0f, IM_COL32(0, 0, 0, 255));
						drawList->AddCircleFilled(p.first, 4.0f, stateColor);
					}

					// DRAW PLAYER
					Tga::Vector3f pos = myData.player->GetTransform().GetPosition();
					drawList->AddCircleFilled(getGridPos(pos), 6.0f, IM_COL32(255, 255, 255, 255));
					Tga::Vector3f aheadOfPlayer = pos + myData.player->GetTransform().GetForward() * 600.f;
					drawList->AddLine(getGridPos(pos), getGridPos(aheadOfPlayer), IM_COL32(255, 255, 255, 255), 3.f);

					//DRAW DEBUG TEXT
					constexpr  float margin = 15.f;
					debugText = "Tempo: " + std::string(magic_enum::enum_name(myData.currentPhase).data());
					float textSize = ImGui::CalcTextSize(debugText.c_str()).x;
					drawList->AddText(canvasP0 + ImVec2(canvasSize.x - (textSize + margin), gridStepY), IM_COL32(255, 255, 255, 255), debugText.c_str());
					debugText = "Mob spawn timer: " + std::to_string(myData.mobTimer);
					textSize = ImGui::CalcTextSize(debugText.c_str()).x;
					drawList->AddText(canvasP0 + ImVec2(canvasSize.x - (textSize + margin), gridStepY * 2.f), IM_COL32(255, 255, 255, 255), debugText.c_str());
					debugText = "Next mob size: " + std::to_string(myData.nextMobMax);
					textSize = ImGui::CalcTextSize(debugText.c_str()).x;
					drawList->AddText(canvasP0 + ImVec2(canvasSize.x - (textSize + margin), gridStepY * 3.f), IM_COL32(255, 255, 255, 255), debugText.c_str());


					ImGui::InvisibleButton("canvas", canvasSize);
					drawList->PopClipRect();
					ImGui::EndChild();


				}


			});

	}
}
#endif

#ifndef _RETAIL
void Forge::AIDirector::DebugDraw()
{
	if (!myInitialized || !myRenderDebug)
	{
		return;
	}

	//Cell draw
	Tga::Engine& engine = *Tga::Engine::GetInstance();
	Tga::DebugDrawer& debugDrawer = engine.GetDebugDrawer();
	Tga::Vector3f boxDimension = { myGridCellSideLength, myGridCellSideLength, myGridCellSideLength };
	for (int i = 0; i < myGrid.size(); ++i)
	{
		const CellStatus status = myGrid[i].status;

		if (status != CellStatus::Invalid)
		{
			debugDrawer.DrawBox(GetXZPositionFromIndex(i), boxDimension, GRID_COLORS[static_cast<int>(status)]);

		}
		if (status == CellStatus::ThreatZone)
		{
			debugDrawer.DrawBox(GetXZPositionFromIndex(i), boxDimension / 2.f, GRID_COLORS[static_cast<int>(status)]);
			debugDrawer.DrawBox(GetXZPositionFromIndex(i), boxDimension / 4.f, GRID_COLORS[static_cast<int>(status)]);
		}

	}

	int playerIn = GetCellIndexFromPosition(myData.player->GetTransform().GetPosition());
	debugDrawer.DrawBox(GetXZPositionFromIndex(playerIn), boxDimension / 2.f, { 0.f, 1.f, 1.f, 1.f });



	if (myActiveAreaDebug)
	{

		for (const int i : myActiveArea.GetCells())
		{
			debugDrawer.DrawBox(GetXZPositionFromIndex(i), boxDimension, { 0.f, 1.f, 0.f, 1.f });
			debugDrawer.DrawBox(GetXZPositionFromIndex(i), boxDimension / 2.f, { 0.f, 1.f, 1.f, 1.f });
		}
	}

	if (myDrawFrustumDebug)
	{
		// 2D frustum draw
		DrawFrustum();

		for (int view : myCellIdsInFrustum.GetCells())
		{
			debugDrawer.DrawBox(GetXZPositionFromIndex(view), boxDimension * 0.75f, { .5f, 0.f, 1.f, 1.f });
			debugDrawer.DrawBox(GetXZPositionFromIndex(view), boxDimension * .33f, { 0.5f, 0.f, 1.f, 1.f });
		}
	}


	if (myRenderMobPath && !myMobToPlayerPath.empty())
	{
		Tga::LineDrawer& lineDrawer = Tga::Engine::GetInstance()->GetGraphicsEngine().GetLineDrawer();
		Tga::LinePrimitive line;
		line.color = { 1.f, 0.f, 0.f, 1.f };
		for (int point = 0; point < myMobToPlayerPath.size() - 1; ++point)
		{
			line.fromPosition = myMobToPlayerPath[point];
			line.toPosition = myMobToPlayerPath[point + 1];
			lineDrawer.Draw(line);
		}
	}
}
#endif
void Forge::AIDirector::Update(const float aFixedTime)
{
	if (!myInitialized)
	{
		return;
	}

	const Tga::Vector3f playerPos = myData.player->GetTransform().GetPosition();
	myBlackboard->SetValue("PlayerPos"_tgaid, playerPos);

	// stress data & active enemies data
	myData.snapshotTimer += aFixedTime;
	if (myData.snapshotTimer > DirectorData::SNAP_SHOT_TIME)
	{
		myData.latestPlayerStress.Add(myData.playerStress);

		myData.totalActiveEnemies = 0;
		for (auto enemy : myEnemies)
		{
			if (enemy && enemy->CheckIfActive())
			{
				++myData.totalActiveEnemies;
			}
		}

		myData.snapshotTimer = 0.f;
	}

	myData.stressTimer -= aFixedTime;
	if (myData.stressTimer < 0.f && myData.playerStress > 0.f)
	{
		myData.playerStress -= DirectorData::STRESS_DOWN_TICK * aFixedTime;
		myData.playerStress = std::clamp(myData.playerStress, 0.f, 1.f);
	}

	// view frustum data
	myCellIdsInFrustum.Clear();
	myPlayerFrustum = CalculateFrustum(myData.playerCamera->GetCamera());
	for (auto& cell : myGrid)
	{
		const float distanceSquared = (GetXZPositionFromIndex(cell.id) - playerPos).LengthSqr();
		if (distanceSquared < ((myActiveAreaRadius * 2.f) * (myActiveAreaRadius * 2.f)))
		{
			if (CheckIfXZPlanePosInFrustum(myPlayerFrustum, GetXZPositionFromIndex(cell.id)))
			{
				if (myGrid[cell.id].status != CellStatus::Invalid)
				{
					myCellIdsInFrustum.Insert(cell.id);
				}
			}
		}
	}

	UpdateActiveAreaAndSpawnZones();
	UpdateMobSpawns(aFixedTime);
	HandleCommands(aFixedTime);
	UpdateCurrentPhase(aFixedTime);
}

int Forge::AIDirector::GetCellIndexFromPosition(const Tga::Vector3f& aPosition)
{
	int tileX = static_cast<int>((ceil(aPosition.x - mySmallestNodePosition.x + myGridCellSideLength * 0.5f) / myGridCellSideLength));
	int tileZ = static_cast<int>((ceil(aPosition.z - mySmallestNodePosition.z + myGridCellSideLength * 0.5f) / myGridCellSideLength));
	int tileIndex = tileX + (tileZ * myAmountOfColumns);

	if (tileIndex < 0 || tileIndex > static_cast<int>(myGrid.size()) - 1)
	{
		tileIndex = static_cast<int>(myGrid.size()) - 1;
	}

	return tileIndex;
}

Tga::Vector3f Forge::AIDirector::GetXZPositionFromIndex(const int aIndex) const
{
	const int row = (aIndex / myAmountOfColumns);
	const int column = (aIndex % myAmountOfColumns);


	float const X = mySmallestNodePosition.x + (myGridCellSideLength * static_cast<float>(column));
	float const Z = mySmallestNodePosition.z + (myGridCellSideLength * static_cast<float>(row));

	return { X, 0.f, Z };
}

std::vector<int> Forge::AIDirector::GetClosestNeighborCells(const int aCellIndex) const
{
	std::vector<int> neighborIndices; neighborIndices.reserve(8);

	const int east = aCellIndex + 1 - ((aCellIndex % myAmountOfColumns) / (myAmountOfColumns - 1)) * myAmountOfColumns;
	const int west = (aCellIndex - 1) + (((aCellIndex - 1) % myAmountOfColumns) / (myAmountOfColumns - 1)) * myAmountOfColumns;

	if (east != aCellIndex)
	{
		neighborIndices.push_back(east);
	}

	if (west - aCellIndex < 2)
	{
		neighborIndices.push_back(west);
	}

	const int south = aCellIndex - myAmountOfColumns;
	if (south > -1)
	{
		neighborIndices.push_back(south);
	}
	const int north = aCellIndex + myAmountOfColumns;
	if (north < myNumberOfCells)
	{
		neighborIndices.push_back(north);
	}

	const int northEast = north + 1 - ((north % myAmountOfColumns) / (myAmountOfColumns - 1)) * myAmountOfColumns;

	if (northEast != north && northEast < myNumberOfCells)
	{
		neighborIndices.push_back(northEast);
	}

	const int northWest = (north - 1) + (((north - 1) % myAmountOfColumns) / (myAmountOfColumns - 1)) * myAmountOfColumns;

	if (northWest != north && northWest < myNumberOfCells)
	{
		neighborIndices.push_back(northWest);
	}

	const int southEast = south + 1 - ((south % myAmountOfColumns) / (myAmountOfColumns - 1)) * myAmountOfColumns;

	if (southEast != south && southEast > -1)
	{
		neighborIndices.push_back((southEast));

	}
	const int southWest = (south - 1) + (((south - 1) % myAmountOfColumns) / (myAmountOfColumns - 1)) * myAmountOfColumns;
	if (southWest != south && southWest > -1)
	{
		neighborIndices.push_back((southWest));
	}

	return neighborIndices;
}

Forge::AreaSet Forge::AIDirector::CalculateActiveAreaCells(int aCellIndex) const
{
	AreaSet activeAreaIndexes; // debug this in draw function
	const int freeColumnsToTheLeft = (aCellIndex % myAmountOfColumns);
	const int freeColumnsToTheRight = ((myAmountOfColumns - 1) - freeColumnsToTheLeft);
	const int spanLeftToRight = static_cast<int>(floor(myActiveAreaRadius / myGridCellSideLength)) + 1;

	auto searchDirection = [&](const int rowDir, const int columnDir, const int horisontalSpace, const int rowOffset, const int columnOffset)
		{
			for (int row = rowOffset; row < spanLeftToRight; row++)
			{
				for (int column = columnOffset; column < spanLeftToRight; column++)
				{
					int searchedIndex = aCellIndex + rowDir * (row * myAmountOfRows) + columnDir * column;
					if (searchedIndex < myGrid.size() && searchedIndex >= 0)
					{
						if (myGrid[searchedIndex].status != CellStatus::Invalid)
						{
							activeAreaIndexes.Insert(searchedIndex);
						}

					}
					if (column >= horisontalSpace)
					{
						break;
					}
				}
			}
		};

	searchDirection(-1, -1, freeColumnsToTheLeft, 0, 0);
	searchDirection(1, 1, freeColumnsToTheRight, 0, 1);
	searchDirection(1, -1, freeColumnsToTheLeft, 1, 0);
	searchDirection(-1, 1, freeColumnsToTheRight, 1, 1);

	return activeAreaIndexes;
}

Object* Forge::AIDirector::GetMyPlayer() const
{
	return myData.player;
}

void Forge::AIDirector::OnEvent(const GameEvent& e)
{

	if (!myInitialized) { return; }

	auto stressUpdate = [this]()
		{
			myData.playerStress = std::clamp(myData.playerStress, 0.f, 1.f);
			myData.stressTimer = DirectorData::STRESS_DURATION_RESET;
		};

	switch (e.message)
	{
	case GameEvent::Message::EnemyKilled:
	{
		const auto enemy = e.GetValue<Object*>();
		const float distanceSqr = (enemy->GetTransform().GetPosition() - myData.player->GetTransform().GetPosition()).LengthSqr();
		if (distanceSqr < DirectorData::KILL_RANGE_SQR)
		{
			myData.playerStress += DirectorData::ENEMY_KILLED_STRESS_VALUE;
			stressUpdate();
		}
		break;
	}
	case GameEvent::Message::DespawnEnemy:
	{
		if (const auto enemy = e.GetValue<Object*>())
		{
			DespawnEnemy(enemy);
		}
		break;
	}
	case GameEvent::Message::PlayerHitByCommon:
	{
		myData.playerStress += DirectorData::TAKING_DAMAGE_STRESS_VALUE;
		stressUpdate();
		break;
	}
	case GameEvent::Message::PlayerHitBySpecial:
	{
		myData.playerStress = DirectorData::ATTACKED_BY_SPECIAL_STRESS_VALUE;
		stressUpdate();
		break;
	}
	case GameEvent::Message::PipeBombThrown:
	{
		Object* bomb = e.GetValue<Object*>();
		constexpr float attractionRadiusSqr = 2000.f * 2000.f; // twenty meters squared
		std::vector<Object*> enemiesInRange;
		enemiesInRange.reserve(ENEMY_MAX_TOTAL_AMOUNT);

		const Tga::Vector3f bombPos = bomb->GetTransform().GetPosition();

		for (auto enemy : myEnemies)
		{
			if (!enemy->CheckIfActive())
			{
				continue;
			}
			const Tga::Vector3f enemyPos = enemy->GetTransform().GetPosition();

			if ((enemyPos - bombPos).LengthSqr() < attractionRadiusSqr)
			{
				enemiesInRange.emplace_back(enemy);
			}
		}

		for (auto bombSeeker : enemiesInRange)
		{
			CommonController* controller = bombSeeker->GetComponent<CommonController>();
			controller->QueueNextState(CommonState::PipeBomb);
			PipeBombState* pipeBombState = controller->AccessState<PipeBombState>(CommonState::PipeBomb);
			pipeBombState->ProvideBombAndNavMesh(bomb, myNavmesh);
		}
		break;
	}
	case GameEvent::Message::BombExplode:
	{
		// std::cout << "Bomb exploded\n";
		const auto [explosionPos, radius] = e.GetValue<std::pair<Tga::Vector3f, float>>();

		for (auto enemy : myEnemies)
		{
			if (!enemy->CheckIfActive())
			{
				continue;
			}
			CommonController* controller = enemy->GetComponent<CommonController>();
			const CommonState currentState = controller->GetCurrentStateId();

			if (((explosionPos - enemy->GetTransform().GetPosition()).LengthSqr() < (radius * radius)))
			{
				controller->QueueNextState(CommonState::Death);
			}
			else if (currentState == CommonState::PipeBomb)
			{
				controller->QueueNextState(CommonState::Pathfind);
			}
		}
		break;
	}
	case GameEvent::Message::DirectorStopUpdate:
	{
		myInitialized = false;
		break;
	}
	case GameEvent::Message::DecompressAction:
	{
		myCommands.emplace_back(std::make_shared<DecompressCommand>());
		break;
	}
	case GameEvent::Message::RecommenceAction:
	{
		for (std::shared_ptr<DirectorCommand> const& command : myCommands)
		{
			DecompressCommand* decompress = dynamic_cast<DecompressCommand*> (command.get());
			if (decompress)
			{
				decompress->SetIsDone(true);
			}
		}
		break;
	}
	default:
	{
		break;
	}
	}
}

void Forge::AIDirector::ProvideCrescendoPoint(int id, Tga::Vector3f& aPos)
{
	myCrescendoPoints.emplace_back(id, aPos);
}

void Forge::AIDirector::SpawnMob(Tga::Vector3f& aFrom, Tga::Vector3f& aTo, int amount)
{
	int enemiesActivated = 0;

	if (!myInitialized) { return; }

	if (myNavmesh)
	{
		const std::vector<Tga::Vector3f> smoothPath = myNavmesh->PathfindFunneled(aFrom, aTo/*myPlayer->GetTransform().GetPosition()*/);
#ifndef _RETAIL
		myMobToPlayerPath = smoothPath;
#endif

		Tga::Vector3f offsetPos = aFrom;
		for (Object* enemy : myEnemies)
		{
			if (enemy != nullptr && !enemy->CheckIfActive())
			{
				enemy->SetActive(true);
				offsetPos += Tga::Vector3f{ 0.f, 0.f, 5.f };
				SpawnEnemy(enemy, offsetPos);
				MakeEnemyPathFind(enemy, smoothPath);
				++enemiesActivated;

				if (enemiesActivated >= amount)
				{
					break;
				}
			}
		}
	}
}

const Forge::CrescendoPoint* Forge::AIDirector::GetCrescendoPoint(int id) const
{
	for (const CrescendoPoint& point : myCrescendoPoints)
	{
		if (static_cast<int>(point.id) == id)
		{
			return &point;
		}
	}
	return nullptr;
}

Tga::Vector3f Forge::AIDirector::GetClosestSpawnPoint() const
{
	if (myPossibleHordeSpawnZones.GetCells().empty()) { return Tga::Vector3f::Zero; }
	return (GetXZPositionFromIndex(myPossibleHordeSpawnZones.GetCells().front()));
}

void Forge::AIDirector::Clear()
{
	for (Object* enemy : myEnemies)
	{
		enemy = nullptr;
	}
	myCrescendoPoints.clear();
	myThreatZones.clear();

	myData.mobTimer = DirectorData::MOB_TIMER_MAX_RESET;
	myData.stressTimer = DirectorData::STRESS_DURATION_RESET;
	myData.playerStress = 0.f;
	myInitialized = false;

	myData.currentPhase = Phases::Relax;
	Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(myData.currentPhase));
}

bool Forge::AIDirector::CreateGridAndMapWorld()
{

	RandomNumberGenerator* randomNumberGenerator = Locator::GetRandomNumberGenerator();
	// myCellCenters.clear();
	myGrid.clear();

	Tga::Vector3f smallestXYZ{ 0.f, 0.f, 0.f };
	Tga::Vector3f largestXYZ{ 0.f, 0.f, 0.f };

	auto& nodes = Locator::GetNavmesh()->GetNodes();

	for (auto& object : nodes)
	{
		Tga::Vector3f pos = object.myCenter;
		smallestXYZ.x = std::min(pos.x, smallestXYZ.x);
		smallestXYZ.z = std::min(pos.z, smallestXYZ.z);
		largestXYZ.x = std::max(pos.x, largestXYZ.x);
		largestXYZ.z = std::max(pos.z, largestXYZ.z);
	}

	Tga::Vector3f playerPos = myData.player->GetTransform().GetPosition();
	Tga::Vector3f endPos = myLevelEnd->GetTransform().GetPosition();

	Tga::Vector3f middleGround = (playerPos + endPos) / 2.f;
	middleGround.y += 500.f;
	float gridXDimension = abs(smallestXYZ.x - largestXYZ.x);
	float gridZDimension = abs(smallestXYZ.z - largestXYZ.z);

	myLineExtreme = std::max(gridXDimension, gridZDimension);
	myNumLines = 3 + (uint16_t)(myLineExtreme / myGridCellSideLength);

	myAmountOfColumns = static_cast<int>(myNumLines);
	myAmountOfRows = static_cast<int>(myNumLines);
	myNumberOfCells = myAmountOfColumns * myAmountOfRows;
	// myCellCenters.resize(myNumberOfCells);
	myGrid.resize(myNumberOfCells);

	mySmallestNodePosition = { smallestXYZ.x - myGridCellHalfLength, 0, smallestXYZ.z - myGridCellHalfLength };
	myLargestPosition = { largestXYZ.x - myGridCellHalfLength, 0, largestXYZ.z - myGridCellHalfLength };

	for (unsigned int row = 0; row < static_cast<unsigned int>(myAmountOfRows); ++row)
	{
		row;
		for (unsigned int column = 0; column < static_cast<unsigned int>(myAmountOfColumns); ++column)
		{
			int const nodeIndex = (row * myAmountOfRows) + column;
			/*float const X = mySmallestNodePosition.x + (myGridCellSideLength * static_cast<float>(column));
			float const Z = mySmallestNodePosition.z + (myGridCellSideLength * static_cast<float>(row));*/

			// myCellCenters[nodeIndex] = { X, middleGround.y, Z };
			myGrid[nodeIndex].id = nodeIndex;
		}
	}

	// std::cout << "grid center size " << myCellCenters.size() << std::endl;

	if (!myNavmesh)
	{
		return false;
	}
	auto& navMeshNodes = myNavmesh->GetNodes();
	if (navMeshNodes.empty())
	{
		return false;
	}
	for (auto& node : navMeshNodes)
	{
		const Tga::Vector3f& center = node.myCenter;
		int index = GetCellIndexFromPosition(center);
		myGrid[index].status = CellStatus::OtherPath;

		for (auto neighbor : node.myConnections) // somehow check if connections are valid/ some might not be. 
		{
			if (neighbor != -1)
			{
				const Tga::Vector3f neibPos = navMeshNodes[neighbor].myCenter;
				Tga::Vector3f toNeighbor = (neibPos - center);
				const int cellSteps = static_cast<int>(toNeighbor.Length() / myGridCellHalfLength);
				toNeighbor.Normalize();
				for (int step = 0; step < cellSteps; ++step)
				{
					const float floatStep = static_cast<float>(step);
					const Tga::Vector3f betweenPos = center + toNeighbor * myGridCellSideLength * floatStep;
					index = GetCellIndexFromPosition(betweenPos);

					myGrid[index].status = CellStatus::OtherPath;
				}
			}
		}
	}

	for (auto& cell : myGrid)
	{
		if (cell.status == CellStatus::OtherPath)
		{
			cell.zCount = static_cast<int8_t>(randomNumberGenerator->GenerateRandomInt(NULL_CHANCE_SPAWN, OTHER_PATH_MAX_SPAWN));
		}
	}

	myRoughPath = myNavmesh->AStarPathfind(myData.player->GetTransform().GetPosition(), myLevelEnd->GetTransform().GetPosition());

	if (myRoughPath.empty())
	{
		return false;
	}

	for (int main = 0; main < myRoughPath.size(); ++main)
	{
		const int index = GetCellIndexFromPosition(myRoughPath[main]);
		myGrid[index].status = CellStatus::MainPath;

		if (const int neighborIndex = main + 1; neighborIndex < myRoughPath.size())
		{

			Tga::Vector3f toNeighbor = (myRoughPath[neighborIndex] - myRoughPath[main]);
			const int cellSteps = static_cast<int>(toNeighbor.Length() / myGridCellHalfLength);
			toNeighbor.Normalize();
			for (int step = 0; step < cellSteps; ++step)
			{
				const float floatStep = static_cast<float>(step);
				const Tga::Vector3f betweenPos = myRoughPath[main] + toNeighbor * myGridCellSideLength * floatStep;
				const int cell = GetCellIndexFromPosition(betweenPos);

				myGrid[cell].status = CellStatus::MainPath;
			}
		}
	}
	for (auto& cell : myGrid)
	{
		if (cell.status == CellStatus::MainPath)
		{
			cell.zCount = static_cast<int8_t>(randomNumberGenerator->GenerateRandomInt(0, MAIN_PATH_MAX_SPAWN));
		}
	}
	const int triangleCount = static_cast<int>(myRoughPath.size() - 1);
	constexpr int subdivisions = 16;
	int spacing = triangleCount / subdivisions;
	std::vector<int> multipliers;

	//starting subdivision higher than zero or one, as not to place threat zone directly att player spawn.
	for (int subDiv = 4; subDiv < subdivisions + 1; subDiv += 2)
	{
		multipliers.emplace_back(subDiv);
	}

	std::ranges::shuffle(multipliers, Locator::GetRandomNumberGenerator()->myRandomState.GetMyRandomState());
	std::vector<SpecialEnemy> specialEnemies;

	for (int sE = 0; sE < static_cast<int>(SpecialEnemy::Count); ++sE)
	{
		specialEnemies.emplace_back(static_cast<SpecialEnemy>(sE));
	}
	std::ranges::shuffle(specialEnemies, Locator::GetRandomNumberGenerator()->myRandomState.GetMyRandomState());

	for (int mult = 0; mult < THREAT_ZONE_AMOUNT; ++mult)
	{
		int threatZoneIndex = multipliers[mult] * spacing;
		int gridIndex = GetCellIndexFromPosition(myRoughPath[threatZoneIndex]);
		myGrid[gridIndex].status = CellStatus::ThreatZone;
		myGrid[gridIndex].zCount = 0;

		SpecialEnemy special = specialEnemies.back();
		specialEnemies.pop_back();
		myThreatZones.emplace_back(myRoughPath[threatZoneIndex], myGrid[gridIndex].id, special);
	}

	myGrid[GetCellIndexFromPosition(myData.player->GetTransform().GetPosition())].status = CellStatus::PlayerSpawnZone;
	std::cout << "AI_DIRECTOR was initialized successfully!\n";

	return true;
}

void Forge::AIDirector::CreateEnemies()
{
	// createIndex;
	unsigned zombieIndex = 0;
	std::string zombieName;

	const std::string maleZombie = "MaleZombie_";
	const std::string femaleZombie = "FemaleZombie_";
	Tga::StringId objectDefinitionName;

	while (zombieIndex < ENEMY_MAX_TOTAL_AMOUNT)
	{
		for (unsigned int i = 0; i < (ENEMY_MAX_SPAWN / 4); ++i)
		{
			zombieName = maleZombie + std::to_string(i + 1);
			objectDefinitionName = Tga::StringRegistry::RegisterOrGetString(zombieName);
			Object* newMale = InstantiateEnemy(objectDefinitionName, ObjectType::CommonZombie);
			if (newMale != nullptr)
			{
				myEnemies[zombieIndex] = newMale;

				DespawnEnemy(newMale);
				++zombieIndex;
			}
			zombieName = femaleZombie + std::to_string(i + 1);
			objectDefinitionName = Tga::StringRegistry::RegisterOrGetString(zombieName);
			Object* newFemale = InstantiateEnemy(objectDefinitionName, ObjectType::CommonZombie);
			if (newFemale != nullptr)
			{
				myEnemies[zombieIndex] = newFemale;

				DespawnEnemy(newFemale);
				++zombieIndex;
			}
		}
	}

	/*myHasCommonAnimationPlayers = false;
	while (zombieIndex < ENEMY_MAX_TOTAL_AMOUNT)
	{
		for (unsigned int i = 0; i < (ENEMY_MAX_SPAWN / 4); ++i)
		{
			/*zombieName = maleZombie + std::to_string(i + 1);
			Tga::StringId objectDefinitionName = Tga::StringRegistry::RegisterOrGetString(zombieName);
			Object* newMale = InstantiateEnemy(objectDefinitionName, ObjectType::CommonZombie);
			if (newMale != nullptr)
			{
				myEnemies[zombieIndex] = newMale;

				DespawnEnemy(newMale);
				++zombieIndex;
			}#1#
			zombieName = femaleZombie + std::to_string(i + 1);
			objectDefinitionName = Tga::StringRegistry::RegisterOrGetString(zombieName);
			Object* newFemale = InstantiateEnemy(objectDefinitionName, ObjectType::CommonZombie);
			if (newFemale != nullptr)
			{
				myEnemies[zombieIndex] = newFemale;

				DespawnEnemy(newFemale);
				++zombieIndex;
			}
		}
	}*/


	std::ranges::shuffle(myEnemies, Locator::GetRandomNumberGenerator()->myRandomState.GetMyRandomState());

	/*myHasCommonAnimationPlayers = false;
	myHasCommonAnimationsStrings = false;*/
}

void Forge::AIDirector::PopulateWorld()
{
	if (!myInitialized) { return; }
	for (Object* enemy : myEnemies)
	{
		if (enemy) DespawnEnemy(enemy);
	}
	// Populate world
	int zombieIndex = 0;

	/*
	 * Object* enemy = nullptr;
	 *for (auto& threatZone : myThreatZones) // in alfa state common zombies get to populate the threat zones.
	{
		for (int e = 0; e < 5; e++)
		{
			enemy = myEnemies[zombieIndex];
			if (enemy != nullptr && !enemy->CheckIfActive())
			{
				Tga::Vector3f zone = threatZone.spawnPoint;
				zone += Tga::Vector3f{ static_cast<float>(rand() % 20) * static_cast<float>(e) , 0.f, static_cast<float>(rand() % 20) * static_cast<float>(e) };
				SpawnEnemy(enemy, zone);
			}
			zombieIndex++;
		}
	}*/
	const Tga::Vector3f playPos = myData.player->GetTransform().GetPosition();
	int playerCell = GetCellIndexFromPosition(playPos);
	myActiveArea = CalculateActiveAreaCells(playerCell);

	const float savedActiveAreaRadius = myActiveAreaRadius;
	constexpr float safetyScalar = 1.5f;
	// myActiveAreaRadius *= safetyScalar; // in the beginning we don't want enemies to immediately  spawn close to and attack the player
	myActiveAreaRadius = myCommonZombieData.lostViewDistance * safetyScalar; // in the beginning we don't want enemies to immediately  spawn close to and attack the player
	const AreaSet safeArea = CalculateActiveAreaCells(playerCell);
	myActiveAreaRadius = savedActiveAreaRadius;



	for (const int cell : safeArea.GetCells())
	{
		myGrid[cell].status = CellStatus::PlayerSpawnZone;
	}

	auto sortClosestToPlayer = [this, playPos](const int cellA, const int cellB)
		{
			const float lengthSqrToA = (playPos - GetXZPositionFromIndex(cellA)).LengthSqr();
			const float lengthSqrToB = (playPos - GetXZPositionFromIndex(cellB)).LengthSqr();
			return lengthSqrToA < lengthSqrToB;
		};

	myActiveArea -= safeArea;

	std::ranges::sort(myActiveArea.AccessCells(), sortClosestToPlayer);

	for (const int cellId : myActiveArea.GetCells())
	{
		const CellStatus status = myGrid[cellId].status;
		if (status == CellStatus::ThreatZone || status == CellStatus::PlayerSpawnZone || status == CellStatus::Invalid)
		{
			continue;
		}
		const int8_t zCount = myGrid[cellId].zCount;
		if (zCount > 0)
		{
			Tga::Vector3f spawnPoint = GetXZPositionFromIndex(cellId);
			for (int8_t spawnCount = 0; spawnCount < zCount; ++spawnCount)
			{
				SpawnEnemy(myEnemies[zombieIndex], spawnPoint);
				zombieIndex++;
				if (zombieIndex >= ENEMY_MAX_TOTAL_AMOUNT)
				{
					break;
				}
			}
		}
		if (zombieIndex >= ENEMY_MAX_TOTAL_AMOUNT)
		{
			break;
		}
	}
}

Object* Forge::AIDirector::InstantiateEnemy(Tga::StringId anObjectDefinitionName, ObjectType anObjectType)
{
	ObjectManager* objectManager = Locator::GetObjectManager();
	Renderer* renderer = Locator::GetRenderer();

	Tga::SceneObjectDefinition* definition = objectManager->AccessSceneDefinitionManager().Get(anObjectDefinitionName);
	if (!definition)
	{
		std::cout << "could NOT find zombie tgo\n";
		return nullptr;
	}

	Object* newObject = *objectManager->AccessObjects().emplace(new Object(anObjectType));
	newObject->GetTransform().SetPosition(ourAIBlackboard.GetValue<Tga::Vector3f>("PlayerPos"_tgaid));
	SceneLoading::ScenePropertyExtractor props(definition->EditProperties());

	const std::vector<const Tga::SceneModel*> sceneModels = props.GetAllCopyOnWriteWrapperByType<Tga::SceneModel>();

	for (std::size_t index{}; index < sceneModels.size(); ++index)
	{
		const Tga::SceneModel* sceneModel = sceneModels[index];

		Tga::RenderMode modelRenderMode = sceneModel->renderMode;
		Tga::BlendMode modelBlendMode = sceneModel->blendMode;

		std::shared_ptr<Tga::ModelShader> shader = ObjectManager::InitShader(sceneModel);
		if (!sceneModel->pixelShader.IsEmpty())
		{

			if (!shader->Init(sceneModel->isAnimated ? "Shaders/AnimatedPbrModelShaderVS" : "Shaders/PbrModelShaderVS", sceneModel->pixelShader.GetString()))
			{
				std::cout << "Could not load " << sceneModel->pixelShader.GetString() << std::endl;
			}
		}

		if (sceneModel->isAnimated)
		{

			auto& model = renderer->AddAnimatedModel(std::move(objectManager->CreateAnimatedModelInstance(*sceneModel)), modelRenderMode, modelBlendMode, shader);
			model.id = newObject->GetObjectID();
			AnimatedModelComponent* animatedModelComponent = newObject->CreateComponent<AnimatedModelComponent>(model, newObject);

			animatedModelComponent->SetLocalTransform(sceneModel->GetTransform());

			CreateModelAnimations(*sceneModels[0], *animatedModelComponent);

		}
		else
		{
			std::cout << "z was not animated\n";
		}

	}


	if (!myHasCommonData)
	{
		myCommonZombieData.damage = props.GetValue<int>("Damage", myCommonZombieData.damage);
		myCommonZombieData.health = props.GetValue<int>("Health", myCommonZombieData.health);
		myCommonZombieData.viewDistance = props.GetValue<float>("View Distance", myCommonZombieData.viewDistance);
		myCommonZombieData.sprintSpeed = props.GetValue<float>("Sprint Speed", myCommonZombieData.sprintSpeed);
		myCommonZombieData.wanderSpeed = props.GetValue<float>("Wander Speed", myCommonZombieData.wanderSpeed);
		myCommonZombieData.force = props.GetValue<float>("Force", myCommonZombieData.force);
		// myCommonZombieData.force = myCommonZombieData.sprintSpeed * 0.75f;

		myCommonZombieData.physData = *props.GetCopyOnWriteWrapperByType<Tga::PhysicsData>();
		myCommonZombieData.physData.physMat = Tga::PhysicsData::PhysicsMaterialType::Flesh;

		myHasCommonData = true;
	}

	newObject->CreateComponent<CommonController>(newObject, myCommonZombieData);
	newObject->CreateComponent<HealthComponent>(newObject, static_cast<HealthComponent::HealthType>(myCommonZombieData.health));
	return newObject;
}

void Forge::AIDirector::CreateModelAnimations(const Tga::SceneModel& aSceneModel, AnimatedModelComponent& aModelComponent)
{
	// Load animations
	const auto& instance = aModelComponent.GetAnimatedModelInstance();
	AnimatedModel& animatedModel = aModelComponent.GetAnimatedModel();
	animatedModel;

	if (!myHasCommonAnimationsStrings)
	{
		myCommonAnimations = aSceneModel.animations;
		myHasCommonAnimationsStrings = true;
	}


	if (instance.IsValid() && !myHasCommonAnimationPlayers/*&& !aSceneModel.animations.empty()*/)
	{
		for (const auto& [tag, path] : myCommonAnimations)
		{
			if (tag == "idleA"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("idleA"_tgaid, path.GetString(), true, true);
			}
			else if (tag == "idleB"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("idleB"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "idleC"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("idleC"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "sprintA"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("sprintA"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "sprintB"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("sprintB"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "sprintC"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("sprintC"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "attackA"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("attackA"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "attackB"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("attackB"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "attackC"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("attackC"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "deathA"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("deathA"_tgaid, path.GetString(), false, false);
			}
			else if (tag == "deathB"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("deathB"_tgaid, path.GetString(), false, false);
			}
			else if (tag == "deathC"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("deathC"_tgaid, path.GetString(), false, false);
			}
			else if (tag == "wanderA"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("wanderA"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "wanderB"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("wanderB"_tgaid, path.GetString(), true, false);
			}
			else if (tag == "wanderC"_tgaid)
			{
				aModelComponent.GetAnimatedModel().AddNewAnimation("wanderC"_tgaid, path.GetString(), true, false);
			}
		}
		myAnimationStates = animatedModel.GetAnimationStates();
		myHasCommonAnimationPlayers = true;

	}
	else
	{
		std::cout << "copied animations\n";
		animatedModel.SetAnimationStates(myAnimationStates);
		animatedModel.SetCurrentState("idleA"_tgaid);
	}

}




void Forge::AIDirector::MakeEnemyPathFind(Object* aEnemy, const std::vector<Tga::Vector3f>& aPath)
{
	CommonController* controller = aEnemy->GetComponent<CommonController>();
	if (!controller) { return; }
	controller->AccessState<PathfindState>(CommonState::Pathfind)->SetPath(aPath);
	controller->QueueNextState(CommonState::Pathfind);
}

void Forge::AIDirector::UpdateActiveAreaAndSpawnZones()
{
	// calculate entered and exited area sets

	const Tga::Vector3f playerPos = myData.player->GetTransform().GetPosition();

	const int playerCell = GetCellIndexFromPosition(playerPos);
	const AreaSet currentSet = CalculateActiveAreaCells(playerCell);
	const AreaSet tempExited = myActiveArea - currentSet;
	const AreaSet tempEntered = currentSet - myActiveArea;
	myActiveArea = currentSet;

	if (tempExited.Size() > 0)
	{
		myExitedArea = tempExited;
		std::vector<Object*> toDespawn;
		toDespawn.reserve(myExitedArea.Size() * MAIN_PATH_MAX_SPAWN);

		for (auto zombie : myEnemies)
		{
			if (!zombie) continue;
			const int zombieCell = GetCellIndexFromPosition(zombie->GetTransform().GetPosition());
			auto controller = zombie->GetComponent<CommonController>();
			const CommonState state = controller->GetCurrentStateId();
			if (myExitedArea.Contains(zombieCell) && (state == CommonState::Idle || state == CommonState::Wander)) // might want to check for state so that path finding zombie are not despawned...
			{
				toDespawn.push_back(zombie);
			}
		}

		for (auto despawn : toDespawn)
		{
			DespawnEnemy(despawn);
		}
	}

	if (tempEntered.Size() > 0)
	{
		myEnteredArea = tempEntered;
		// std::cout << "entered area size : " << myEnteredArea.Size() << std::endl;
		std::vector<Object*> toSpawn;
		toSpawn.reserve(ENEMY_MAX_TOTAL_AMOUNT);


		if (myData.currentPhase != Phases::Relax)
		{
			for (auto zombie : myEnemies)
			{
				if (zombie && !zombie->CheckIfActive())
				{
					toSpawn.emplace_back(zombie);
				}
			}

			if (!toSpawn.empty())
			{
				for (const int cellID : myEnteredArea.GetCells())
				{
					const int8_t zCount = myGrid[cellID].zCount;
					if (zCount < 1 || myGrid[cellID].status == CellStatus::PlayerSpawnZone)
					{
						continue;
					}
					for (int8_t spawnCount = 0; spawnCount < zCount; spawnCount++)
					{
						const Tga::Vector3f spawnPoint = GetXZPositionFromIndex(cellID);
						Object* const ready = toSpawn.back();
						toSpawn.pop_back();
						SpawnEnemy(ready, spawnPoint);
						++myData.totalActiveEnemies;
						if (toSpawn.empty() || myData.totalActiveEnemies >= ENEMY_MAX_SPAWN)
						{
							break;
						}
					}
					if (toSpawn.empty() || myData.totalActiveEnemies >= ENEMY_MAX_SPAWN)
					{
						break;
					}
				}
			}
		}
	}


	// Discern possible horde spawn zone
	myPossibleHordeSpawnZones.Clear();
	myPossibleHordeSpawnZones = myActiveArea;
	myPossibleHordeSpawnZones += myExitedArea;

	AreaSet impossibleZones;
	impossibleZones.Insert(playerCell);

	const Tga::Vector3f playerCellPosition = GetXZPositionFromIndex(playerCell);
	for (const int cell : myPossibleHordeSpawnZones.GetCells())
	{
		// minimum distance check
		const Tga::Vector3f otherCellPosition = GetXZPositionFromIndex(cell);
		const Tga::Vector3f cellToPlayer = (playerCellPosition - otherCellPosition);
		const float distance = cellToPlayer.Length();
		constexpr float distanceFidelity = 0.75f; // lower number means more generous inclusion
		if (distance < (myActiveAreaRadius * distanceFidelity))
		{
			impossibleZones.Insert(cell);
			continue;
		}
		const Tga::Vector3f cellToPlayerNormalized = cellToPlayer.GetNormalized();
		constexpr float traceFidelity = 0.25f; // lower number means higher fidelity;
		const int steps = static_cast<int>(distance / (myGridCellHalfLength * traceFidelity));
		const float increment = distance / static_cast<float>(steps);

		bool isPossible = false;
		for (int step = 0; step < steps; ++step)
		{
			const int cellToCheck = GetCellIndexFromPosition(otherCellPosition + (cellToPlayerNormalized * increment * static_cast<float>(step + 1)));
			if (myGrid[cellToCheck].status == CellStatus::Invalid)
			{
				isPossible = true;
				break;
			}
		}

		if (!isPossible)
		{
			impossibleZones.Insert(cell);
			continue;
		}

		/*if (myNavmesh->AStarPathfind(otherCellPosition ,playerPos).empty()) // could no path find from here to player, may this is to strict?
		{
			impossibleZones.Insert(cell);
			// continue;
		}*/

	}
	myPossibleHordeSpawnZones -= impossibleZones;

	auto sortClosestToPlayer = [this, playerCellPosition](const int cellA, const int cellB)
		{
			const float lengthSqrToA = (playerCellPosition - GetXZPositionFromIndex(cellA)).LengthSqr();
			const float lengthSqrToB = (playerCellPosition - GetXZPositionFromIndex(cellB)).LengthSqr();
			return lengthSqrToA < lengthSqrToB;
		};

	std::ranges::sort(myPossibleHordeSpawnZones.AccessCells().begin(), myPossibleHordeSpawnZones.AccessCells().end(), sortClosestToPlayer);
	// may sort again based of they are behind player or not...

	//myExitedArea

	/*auto sortBehindPlayer = [this, playerCellPosition](const int cellA, const int cellB)
		{
		 // if flow distance to level start for cell is less than the player flow distance to level start, then it is counted as behind.
		// this can not be calculated to often... because expensive.
		};*/
}

void Forge::AIDirector::UpdateMobSpawns(float aFixedTime)
{
	myData.mobTimer -= (DirectorData::MOB_TIMER_TICK)*aFixedTime;

	if (myData.mobTimer < 0.f && myData.currentPhase != Phases::Relax)
	{

		if (myData.currentPhase != Phases::Peak)
		{
			// if player is stressed next horde will be smaller and vice versa	
			myData.nextMobMax = static_cast<int>(FMath::Lerp(static_cast<float>(DirectorData::MOB_MAX_SIZE), static_cast<float>(DirectorData::MOB_MIN_SIZE), myData.playerStress));
			// if player is stressed next horde will delay longer and vice versa
			myData.mobTimer = FMath::Lerp(DirectorData::MOB_TIMER_MIN_RESET, DirectorData::MOB_TIMER_MAX_RESET, myData.playerStress);
		}

		const int spawnZone = myPossibleHordeSpawnZones.GetCells().front();
		std::vector<Object*> toSpawn;
		toSpawn.reserve(myData.nextMobMax);

		for (Object* enemy : myEnemies)
		{
			if (enemy && !enemy->CheckIfActive())
			{
				toSpawn.emplace_back(enemy);
				if (toSpawn.size() == myData.nextMobMax)
				{
					break;
				}
			}
		}

		std::vector<Tga::Vector3f> path = myNavmesh->PathfindFunneled(GetXZPositionFromIndex(spawnZone), myData.player->GetTransform().GetPosition());

		if (!path.empty())
		{
			for (Object* enemy : toSpawn)
			{
				SpawnEnemy(enemy, GetXZPositionFromIndex(spawnZone));
				MakeEnemyPathFind(enemy, path);
			}




		}

	}
}

void Forge::AIDirector::DespawnEnemy(Object* aEnemy)
{
	if (!aEnemy) return;

	CommonController* commonController = aEnemy->GetComponent<CommonController>();
	if (!commonController) return;
	aEnemy->GetTransform().SetPosition({ HIDDEN, HIDDEN, HIDDEN });
	auto& character = commonController->AccessCharacter();
	auto bodyId = character->GetBodyID();
	auto bodyInterface = Locator::GetPhysicsEngine()->GetBodyInterface();
	character->SetPosition({ HIDDEN, HIDDEN, HIDDEN }, JPH::EActivation::DontActivate);
	commonController->Update(1.f / 60.f);
	AnimatedModelComponent* animatedModel = aEnemy->GetComponent<AnimatedModelComponent>();
	animatedModel->Update(1.f / 60.f);
	bodyInterface->RemoveBody(bodyId);
	aEnemy->SetActive(false);
}

void Forge::AIDirector::SpawnEnemy(Object* aEnemy, const Tga::Vector3f& zone)
{
	if (!aEnemy) return;

	auto health = aEnemy->GetComponent<HealthComponent>();
	health->HealFully();
	auto commonController = aEnemy->GetComponent<CommonController>();
	if (!commonController) { return; }
	aEnemy->SetActive(true);
	auto [nodeIndex, newZone] = myNavmesh->FindClosestTriangleAndPoint(zone);
	newZone = myNavmesh->GetNodes()[nodeIndex].myCenter;
	newZone.y += 100.f;

	aEnemy->GetTransform().SetPosition(newZone);
	const float randY = Locator::GetRandomNumberGenerator()->GenerateRandomFloat(0.f, FMath::Tau);
	aEnemy->GetTransform().SetRotation({ 0.f, randY * -FMath::RadToDeg + 90.f, 0.f });

	auto& character = commonController->AccessCharacter();
	auto bodyId = character->GetBodyID();
	auto bodyInterface = Locator::GetPhysicsEngine()->GetBodyInterface();
	if (!bodyInterface->IsAdded(bodyId))
	{
		bodyInterface->AddBody(bodyId, JPH::EActivation::DontActivate);
	}

	character->SetPosition({ newZone.x, newZone.y, newZone.z }, JPH::EActivation::DontActivate);
	JPH::Quat quat = JPH::Quat::sEulerAngles({ 0.f, aEnemy->GetTransform().GetRotationAsQuaternion().GetYawPitchRoll().y * FMath::DegToRad, 0.f });
	character->SetRotation(quat, JPH::EActivation::DontActivate);
	character->SetLinearVelocity(JPH::Vec3::sZero());
	character->SetLinearAndAngularVelocity(JPH::Vec3::sZero(), JPH::Vec3::sZero());
	bodyInterface->ActivateBody(bodyId);

	if (myData.currentPhase != Phases::Relax)
	{
		int  state = Locator::GetRandomNumberGenerator()->GenerateRandomInt(1, STATE_DICE);
		if (state >= WANDER_CHANCE)
		{
			aEnemy->GetComponent<CommonController>()->QueueNextState(CommonState::Wander);
			// std::cout << "spawned wanderer\n";
		}
		else
		{
			aEnemy->GetComponent<CommonController>()->QueueNextState(CommonState::Idle);
			// std::cout << "spawned idle\n";
		}
	}
	else
	{
		aEnemy->GetComponent<CommonController>()->QueueNextState(CommonState::Idle);
	}
}

bool Forge::AIDirector::CheckIf3DPosInFrustum(const Frustum& aFrustum, const Tga::Vector3f& aCenter, float aRadius)
{

	if (aFrustum.top.normal.Dot(aCenter - aFrustum.top.pos) <= -aRadius) return false;
	if (aFrustum.right.normal.Dot(aCenter - aFrustum.right.pos) <= -aRadius) return false;
	if (aFrustum.bottom.normal.Dot(aCenter - aFrustum.bottom.pos) <= -aRadius) return false;
	if (aFrustum.left.normal.Dot(aCenter - aFrustum.left.pos) <= -aRadius) return false;
	if (aFrustum.nearplane.normal.Dot(aCenter - aFrustum.nearplane.pos) <= -aRadius) return false;
	if (aFrustum.farplane.normal.Dot(aCenter - aFrustum.farplane.pos) <= -aRadius) return false;

	return true;

}

bool Forge::AIDirector::CheckIfXZPlanePosInFrustum(const Frustum& aFrustum, const Tga::Vector3f& aCenter)
{
	//     a     
	//     /\    
	//    /  \   *
	//   / *  \  
	//  /______\ 
	// c        b
	const Tga::Vector3f playerPos = myData.player->GetTransform().GetPosition();
	const Tga::Matrix4x4f playerPhysAxis = CalculatePlayerPhysMatrix();
	const Tga::Vector2f center = { aCenter.x, aCenter.z };

	const float frustumDistance = myData.playerCamera->GetSettings().farPlane - myData.playerCamera->GetSettings().nearPlane;
	const float frustumScalar = myActiveAreaRadius / frustumDistance;
	const float rightDistance = (aFrustum.farrect.bl - aFrustum.farrect.br).Length() * frustumScalar;

	const Tga::Vector3f aheadOfPlayer = playerPos + playerPhysAxis.GetForward() * myActiveAreaRadius /*frustumDistance * frustumScalar*/;
	const Tga::Vector3f leftPoint = aheadOfPlayer + (playerPhysAxis.GetRight() * rightDistance * -0.5f);
	const Tga::Vector3f rightPoint = aheadOfPlayer + (playerPhysAxis.GetRight() * rightDistance * 0.5f);

	const Tga::Vector2f a = { playerPos.x,  playerPos.z };
	const Tga::Vector2f b = { leftPoint.x, leftPoint.z };
	const Tga::Vector2f aToB = (b - a);

	const Tga::Vector2f c = { rightPoint.x, rightPoint.z };
	const Tga::Vector2f bToC = (c - b);
	const Tga::Vector2f cToA = a - c;

	std::vector<Tga::Vector2f> centerExtremes;
	centerExtremes.resize(5);
	constexpr int topRight = 0;
	constexpr int topLeft = 1;
	constexpr int bottomRight = 2;
	constexpr int bottomLeft = 3;
	constexpr int middle = 4;

	centerExtremes[topRight] = Tga::Vector2f(center + Tga::Vector2f{ myGridCellHalfLength, myGridCellHalfLength });   // top right
	centerExtremes[topLeft] = Tga::Vector2f(center + Tga::Vector2f{ -myGridCellHalfLength, myGridCellHalfLength });  // top left
	centerExtremes[bottomRight] = Tga::Vector2f(center + Tga::Vector2f{ myGridCellHalfLength, -myGridCellHalfLength });  // bottom right
	centerExtremes[bottomLeft] = Tga::Vector2f(center + Tga::Vector2f{ -myGridCellHalfLength, -myGridCellHalfLength }); // bottom left
	centerExtremes[middle] = Tga::Vector2f(center); // middle

	for (auto& point : centerExtremes)
	{
		const Tga::Vector2f aToPoint = (point - a);
		if (aToB.Cross(aToPoint) > 0.f) { continue; }

		const Tga::Vector2f bToPoint = (point - b);
		if (bToC.Cross(bToPoint) > 0.f) { continue; }

		const Tga::Vector2f cToPoint = point - c;
		if (cToA.Cross(cToPoint) > 0.f) { continue; }

		return true;
	}

	auto pointInAABB = [centerExtremes](const Tga::Vector2f& point)
		{
			if (point.x > centerExtremes[topRight].x) { return false; }
			if (point.y > centerExtremes[topRight].y) { return false; }
			if (point.x < centerExtremes[bottomLeft].x) { return false; }
			if (point.y < centerExtremes[bottomLeft].y) { return false; }
			return true;
		};

	if (pointInAABB(a)) { return true; }
	if (pointInAABB(b)) { return true; }
	if (pointInAABB(c)) { return true; }

	return false;
}

const Tga::Matrix4x4f Forge::AIDirector::CalculatePlayerPhysMatrix()
{
	if (!myData.playerController) return {};

	const JPH::BodyID& bodyId = myData.playerController->GetBodyId();
	const auto quaternion = Locator::GetPhysicsEngine()->GetBodyInterface()->GetRotation(bodyId)/*.GetEulerAngles()*/;
	Tga::Quaternionf quat;
	quat.W = quaternion.GetW();
	quat.X = quaternion.GetX();
	quat.Y = quaternion.GetY();
	quat.Z = quaternion.GetZ();

	return Tga::Matrix4x4f::CreateFromRotation(quat);
}

void Forge::AIDirector::HandleCommands(float aFixedTime)
{
	std::vector<std::shared_ptr<DirectorCommand>> finishedCommands;
	finishedCommands.reserve(myCommands.size());
	for (std::shared_ptr<DirectorCommand> const& command : myCommands)
	{
		command->Update(aFixedTime, this);
		if (command->CheckIsDone())
		{
			finishedCommands.emplace_back(command);
		}
	}

	for (auto& toDelete : finishedCommands)
	{
		for (int command = 0; command < myCommands.size(); ++command)
		{
			if (toDelete.get() == myCommands[command].get())
			{
				myCommands[command] = myCommands.back();
				myCommands.pop_back();
				--command;
			}
		}
	}
}

void Forge::AIDirector::UpdateCurrentPhase(float aFixedTime)
{
	switch (myData.currentPhase)
	{
	case Phases::Relax:
	{
		myData.relaxTimer -= DirectorData::RELAX_TIMER_TICK * aFixedTime;
		if (myData.relaxTimer < 0.f && myData.playerLeftSpawn)
		{
			myData.relaxTimer = DirectorData::RELAX_TIMER_RESET;
			myData.currentPhase = Phases::BuildUp;
			Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(myData.currentPhase));
		}
		break;
	}
	case Phases::BuildUp:
	{
		if (myData.playerStress >= DirectorData::STRESS_PEAK_THRESHOLD)
		{
			myData.currentPhase = Phases::Peak;
			myData.nextMobMax = DirectorData::MOB_MIN_SIZE;
			// if player is stressed next horde will delay longer and vice versa
			myData.mobTimer = DirectorData::MOB_TIMER_MAX_RESET;
			Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(myData.currentPhase));
		}
		break;
	}
	case Phases::Peak:
	{
		myData.peakTimer -= DirectorData::PEAK_TIMER_TICK * aFixedTime;
		if (myData.peakTimer < 0.f && myData.stressTimer < FLT_EPSILON)
		{
			myData.peakTimer = DirectorData::PEAK_TIMER_RESET;
			myData.currentPhase = Phases::Relax;
			Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(myData.currentPhase));
		}

		break;
	}
	default:
	{
		break;
	}
	}
}
#ifndef _RETAIL
void Forge::AIDirector::DrawFrustum()
{
	Tga::LineDrawer& lineDrawer = Tga::Engine::GetInstance()->GetGraphicsEngine().GetLineDrawer();
	Tga::LinePrimitive line;

	//first iteration 2D draw
	const Tga::Vector3f playerPos = myData.player->GetTransform().GetPosition();
	const Tga::Matrix4x4f playerPhysAxis = CalculatePlayerPhysMatrix();
	const float forwardDistance = myData.playerCamera->GetSettings().farPlane - myData.playerCamera->GetSettings().nearPlane;
	const float frustumScalar = myActiveAreaRadius / forwardDistance;
	const float rightDistance = (myPlayerFrustum.farrect.bl - myPlayerFrustum.farrect.br).Length() * frustumScalar;
	const Tga::Vector3f aheadOfPlayer = playerPos + playerPhysAxis.GetForward() * myActiveAreaRadius;
	const Tga::Vector3f leftPoint = aheadOfPlayer + (playerPhysAxis.GetRight() * rightDistance * -0.5f);
	const Tga::Vector3f rightPoint = aheadOfPlayer + (playerPhysAxis.GetRight() * rightDistance * 0.5f);

	line.color = { 0.5f, 0.73f, 1.f, 1.f };

	line.fromPosition = playerPos;
	line.toPosition = leftPoint;
	lineDrawer.Draw(line);

	line.fromPosition = leftPoint;
	line.toPosition = rightPoint;
	lineDrawer.Draw(line);

	line.fromPosition = rightPoint;
	line.toPosition = playerPos;
	lineDrawer.Draw(line);


}

bool Forge::AIDirector::CanDrawDebug()
{
	return myRenderDebug;
}

bool Forge::AIDirector::CanDrawEnemyDebug()
{
	return myEnemiesDebug;
}
#endif
