#include "AIDirectorUtils.h"
#include "Managers/AiDirector.h"
#include "Managers/AudioManager.h"
#include "Objects/Object.h"
#include "Utilities/Locator.h"

Forge::AreaSet& Forge::AreaSet::operator=(const AreaSet areaSet)
{
		myCellIds = areaSet.myCellIds;
		return *this;
}

Forge::AreaSet Forge::AreaSet::operator-(AreaSet& setB) const
{
	std::vector<int> difference;
	difference.reserve(27);

	for (const int id : this->myCellIds)
	{
		if (std::ranges::find(setB.myCellIds.begin(), setB.myCellIds.end(), id) == setB.myCellIds.end())
		{
			difference.emplace_back(id);
		}
	}
	return AreaSet(difference);
}

void Forge::AreaSet::operator+=(AreaSet& setB)
{
	for (const int id : setB.myCellIds)
	{
		if (std::ranges::find(this->myCellIds.begin(), this->myCellIds.end(), id) == this->myCellIds.end())
		{
			this->myCellIds.emplace_back(id);
		}
	}
}

void Forge::AreaSet::operator+=(const AreaSet& setB)
{
	for (const int id : setB.myCellIds)
	{
		if (std::ranges::find(this->myCellIds.begin(), this->myCellIds.end(), id) == this->myCellIds.end())
		{
			this->myCellIds.emplace_back(id);
		}
	}
}

Forge::AreaSet Forge::AreaSet::operator-(const AreaSet& setB) const
{
	std::vector<int> toKeep;
	toKeep.reserve(27);

	for (const int id : this->myCellIds)
	{
		if (std::ranges::find(setB.myCellIds.begin(), setB.myCellIds.end(), id) == setB.myCellIds.end()) // other set did not have it therefore this set should not remove it
		{
			toKeep.emplace_back(id);
		}
	}
	return AreaSet(toKeep);
}

void Forge::AreaSet::operator-=(AreaSet& setB)
{
	std::vector<int> toKeep;
	toKeep.reserve(27);
	for (const int id : this->myCellIds)
	{
		if (std::ranges::find(setB.myCellIds.begin(), setB.myCellIds.end(), id) == setB.myCellIds.end())
		{
			toKeep.emplace_back(id);
		}
	}
	this->myCellIds = toKeep;
}

void Forge::AreaSet::operator-=(const AreaSet& setB)
{
	std::vector<int> toKeep;
	toKeep.reserve(27);

	for (const int id : this->myCellIds)
	{
		if (std::ranges::find(setB.myCellIds.begin(), setB.myCellIds.end(), id) == setB.myCellIds.end())
		{
			toKeep.emplace_back(id);
		}
	}
	this->myCellIds = toKeep;
}

void Forge::AreaSet::Insert(int cellId)
{
	if (std::ranges::find(myCellIds.begin(), myCellIds.end(), cellId) == myCellIds.end())
	{
		myCellIds.emplace_back(cellId);
	}
}

bool Forge::AreaSet::Contains(const int cellId)
{
	if (std::ranges::find(myCellIds.begin(), myCellIds.end(), cellId) != myCellIds.end())
	{
		return true;
	}
	return false;
}

void Forge::AreaSet::Clear()
{
	myCellIds.clear();
}

size_t Forge::AreaSet::Size() const
{
	return myCellIds.size();
}

void Forge::PlayerLeftSpawnCommand::Update(float, AIDirector* aAiDirector)
{
	if (aAiDirector)
	{
		auto& data = aAiDirector->myData;
		const int playerGridCell = aAiDirector->GetCellIndexFromPosition(data.player->GetTransform().GetPosition());
		myIsDone = aAiDirector->myGrid[playerGridCell].status != CellStatus::PlayerSpawnZone;

		if (myIsDone)
		{
			data.currentPhase = Phases::BuildUp;
			Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(data.currentPhase));
			data.playerLeftSpawn = true;
#ifndef _RETAIL
			std::cout << "player left spawn\n";
#endif
		}

	}
}

void Forge::DecompressCommand::Update(float, AIDirector* aAiDirector)
{
	if (aAiDirector)
	{
		auto& data = aAiDirector->myData;
		if (!myIsDone)
		{
#ifndef _RETAIL
			std::cout << "player is decompressing\n";
#endif
			data.relaxTimer   = DirectorData::RELAX_TIMER_RESET;
			data.currentPhase = Phases::Relax;
			Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(data.currentPhase));
		}
		else
		{
#ifndef _RETAIL
			std::cout << "player entered battle again\n";
#endif
			data.relaxTimer   = DirectorData::RELAX_TIMER_RESET;
			data.currentPhase = Phases::BuildUp;
			Locator::GetAudioManager()->SetParameter(FmodId::MainMusic, static_cast<float>(data.currentPhase));
		}

	}
}
