// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FactionSystemSchematyc.h"
#include "Components/Factions/FactionComponent.h"

namespace FactionSystemSchematyc
{
	bool CompareFactions(const SFactionID& factionOne, const SFactionID& factionTwo)
	{
		return factionOne == factionTwo;
	}

	void ChangeReaction(const SFactionID& factionOne, const SFactionID& factionTwo, IFactionMap::ReactionType reaction)
	{
		IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();
		factionMap.SetReaction(factionOne.id, factionTwo.id, reaction);
	}

	SFactionID GetEntityFaction(Schematyc::ExplicitEntityId entityId)
	{
		return GetAISystem()->GetFactionSystem()->GetEntityFaction(static_cast<EntityId>(entityId));
	}
	
	void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope)
	{
		//Register Components
		CEntityAIFactionComponent::Register(registrar);
		
		const CryGUID FactionSystemGUID = "6c04b224-56e4-4cbf-93e1-5bd470b93704"_cry_guid;

		parentScope.Register(SCHEMATYC_MAKE_ENV_MODULE(FactionSystemGUID, "Factions"));
		Schematyc::CEnvRegistrationScope factionsScope = registrar.Scope(FactionSystemGUID);

		//GetEntityFaction
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityFaction, "99d2c416-7110-46e8-899b-bc9d887d6acf"_cry_guid, "GetEntityFaction");
			pFunction->SetDescription("Get faction for specified entity");
			pFunction->BindInput(1, 'ent', "EntityId");
			pFunction->BindOutput(0, 'ef', "Faction");
			factionsScope.Register(pFunction);
		}
		//ChangeReaction
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&ChangeReaction, "de2f6f1a-dafc-439d-886f-d21e26c6babc"_cry_guid, "ChangeReaction");
			pFunction->SetDescription("Change factions reaction");
			pFunction->BindInput(1, 'f1', "Faction A");
			pFunction->BindInput(2, 'f2', "Faction B");
			pFunction->BindInput(3, 'rt', "Reaction");
			factionsScope.Register(pFunction);
		}

		// Register types
		factionsScope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(SFactionID));
		{
			Schematyc::CEnvRegistrationScope scope = registrar.Scope(Schematyc::GetTypeDesc<SFactionID>().GetGUID());

			//CompareFactions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CompareFactions, "04565f27-d18e-4dd6-ae67-c0f85677d398"_cry_guid, "Compare");
				pFunction->SetDescription("Returns true if both factions are the same.");
				pFunction->BindInput(1, 'f1', "Faction A");
				pFunction->BindInput(2, 'f2', "Faction B");
				pFunction->BindOutput(0, 'ret', "Equal");
				scope.Register(pFunction);
			}
		}
	}
}

struct BitFlagsRowWrapper
{
	void Serialize(Serialization::IArchive& archive)
	{
		archive(*pValue, "flag", "^^");
		archive(label, "label", "!^^ ");
	}
	bool* pValue;
	string label;
};

BitFlagsRowWrapper BitFlagsRow(bool& bValue, const char* label)
{
	BitFlagsRowWrapper wrapper;
	wrapper.pValue = &bValue;
	wrapper.label = label;
	return wrapper;
}

struct BitFlagsStringListWrapper {
	uint32* pVariable;
	const Serialization::StringList* pFlagNamesList;

	void Serialize(Serialization::IArchive& archive)
	{
		const Serialization::StringList& flagNamesList = *pFlagNamesList;
		const int count = flagNamesList.size();
		if (archive.isInput())
		{
			uint32 previousValue = *pVariable;
			for (int i = 0; i < count; ++i)
			{
				const uint32 flagValue = 1 << i;
				const bool bPreviousFlag = (previousValue & flagValue) == flagValue;
				bool bFlag = false;
				
				if (archive.isEdit())
				{
					archive(BitFlagsRow(bFlag, flagNamesList[i]), "flag", " ");
				}
				else
				{
					archive(bFlag, flagNamesList[i], flagNamesList[i]);
				}

				if (bFlag != bPreviousFlag)
				{
					if (bFlag)
						*pVariable |= flagValue;
					else
						*pVariable &= ~flagValue;
				}
			}
		}
		else
		{
			for (int i = 0; i < count; ++i)
			{
				const uint32 flagValue = 1 << i;
				bool bFlag = (*pVariable & flagValue) == flagValue;

				if (archive.isEdit())
				{
					archive(BitFlagsRow(bFlag, flagNamesList[i]), "flag", " ");
				}
				else if (bFlag)
				{
					archive(bFlag, flagNamesList[i], flagNamesList[i]);
				}
			}
		}
	}
};

BitFlagsStringListWrapper StringBitFlags(uint32& value, const Serialization::StringList& stringList)
{
	BitFlagsStringListWrapper wrapper;
	wrapper.pVariable = &value;
	wrapper.pFlagNamesList = &stringList;
	return wrapper;
}

bool Serialize(Serialization::IArchive& archive, SFactionFlagsMask& value, const char* szName, const char* szLabel)
{
	IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();
	const uint32 factionsCount = factionMap.GetFactionCount();

	Serialization::StringList factionNamesList;
	factionNamesList.reserve(factionsCount);

	for (uint32 i = 0; i < factionsCount; ++i)
	{
		factionNamesList.push_back(factionMap.GetFactionName(i));
	}
	return archive(StringBitFlags(value.mask, factionNamesList), szName, szLabel);
}

bool Serialize(Serialization::IArchive& archive, SFactionID& value, const char* szName, const char* szLabel)
{
	IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();

	if (archive.isEdit())
	{
		const uint32 factionsCount = factionMap.GetFactionCount();

		Serialization::StringList factionNamesList;
		factionNamesList.reserve(factionsCount + 1);

		string factionName = factionMap.GetFactionName(value.id);
		if (factionName.IsEmpty() && !value.name.IsEmpty())
		{
			factionNamesList.push_back(value.name);
		}
		else
		{
			factionNamesList.push_back("");
		}

		for (uint32 i = 0; i < factionsCount; ++i)
		{
			factionNamesList.push_back(factionMap.GetFactionName(i));
		}

		bool bResult = false;
		if (archive.isInput())
		{
			Serialization::StringListValue temp(factionNamesList, 0);
			bResult = archive(temp, szName, szLabel);

			value.id = factionMap.GetFactionID(temp.c_str());
			if (!value.IsValid())
			{
				value.name = temp.c_str();
			}
			else
			{
				value.name.clear();
			}
		}
		else if (archive.isOutput())
		{
			if (factionName.IsEmpty() && !value.name.IsEmpty())
			{
				archive.warning(value.name, "Faction '%s' is not declared.", value.name.c_str());
				factionName = value.name;
			}
			const int pos = factionNamesList.find(factionName);
			bResult = archive(Serialization::StringListValue(factionNamesList, pos), szName, szLabel);
		}
		return bResult;
	}
	else
	{
		string factionName;
		if (archive.isInput())
		{
			archive(factionName, szName, szLabel);
			value.id = factionMap.GetFactionID(factionName);
			if (!value.IsValid())
			{
				value.name = factionName;
			}
			else
			{
				value.name.clear();
			}
		}
		else
		{
			factionName = factionMap.GetFactionName(value.id);
			if (factionName.IsEmpty())
			{
				archive(value.name, szName, szLabel);
			}
			else
			{
				archive(factionName, szName, szLabel);
			}
		}
	}
	return true;
}