// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>

class XmlNodeRef;
class CAnimEventData;
struct AnimEventInstance;

#include <CrySerialization/Forward.h>
#include <CryAnimation/IAnimEventPlayer.h>

namespace CharacterTool
{

struct AnimEvent
{
	float  startTime;
	float  endTime;
	string type;
	string parameter;
	string boneName;
	string model;
	Vec3   offset;
	Vec3   direction;

	AnimEvent()
		: startTime(0.0f)
		, endTime(0.0f)
		, type("audio_trigger")
		, offset(ZERO)
		, direction(ZERO)
	{
	}

	bool operator<(const AnimEvent& animEvent) const { return startTime < animEvent.startTime; }

	void Serialize(Serialization::IArchive& ar);
	void FromData(const CAnimEventData& eventData);
	void ToData(CAnimEventData* eventData) const;
	void ToInstance(AnimEventInstance* instance) const;
	bool LoadFromXMLNode(const XmlNodeRef& node);
};
typedef std::vector<AnimEvent> AnimEvents;

struct AnimEventPreset
{
	string    name;
	float     colorHue;
	AnimEvent event;

	AnimEventPreset() : name("Preset"), colorHue(0.66f) {}

	void Serialize(Serialization::IArchive& ar);
};
typedef std::vector<AnimEventPreset> AnimEventPresets;

bool IsAudioEventType(const char* audioEventType);

class AnimEventPlayer_CharacterTool : public IAnimEventPlayer
{
public:
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(IAnimEventPlayer)
	CRYINTERFACE_END()
	CRYGENERATE_CLASS_GUID(AnimEventPlayer_CharacterTool, "AnimEventPlayer_CharacterTool", "a5fefb2d-fe05-dec4-a816-9d6e3ac635b0"_cry_guid)

		AnimEventPlayer_CharacterTool();
	virtual ~AnimEventPlayer_CharacterTool() {}

	const SCustomAnimEventType* GetCustomType(int customTypeIndex) const override
	{
		int listSize = m_list.size();
		int lowerLimit = 0;
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			int typeCount = m_list[i]->GetCustomTypeCount();
			if (customTypeIndex >= lowerLimit && customTypeIndex < lowerLimit + typeCount)
				return m_list[i]->GetCustomType(customTypeIndex - lowerLimit);
			lowerLimit += typeCount;
		}
		return 0;
	}

	void Initialize() override
	{
		size_t listSize = m_list.size();
		for (size_t i = 0; i < listSize; ++i)
		{
			if (!m_list[i].get())
				continue;
			m_list[i]->Initialize();
		}
	}

	int GetCustomTypeCount() const override
	{
		int typeCount = 0;

		size_t listSize = m_list.size();
		for (size_t i = 0; i < listSize; ++i)
		{
			if (!m_list[i].get())
				continue;
			typeCount += m_list[i]->GetCustomTypeCount();
		}

		return typeCount;
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		// m_list could be serialized directly to unlock full customization of the list:
		//   ar(m_list, "list", "^");

		size_t num = m_list.size();
		for (size_t i = 0; i < num; ++i)
			ar(*m_list[i], m_list[i]->GetFactory()->GetName(), m_names[i].c_str());
	}

	const char* SerializeCustomParameter(const char* parameterValue, Serialization::IArchive& ar, int customTypeIndex) override
	{
		int listSize = m_list.size();
		int lowerLimit = 0;
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			int typeCount = m_list[i]->GetCustomTypeCount();
			if (customTypeIndex >= lowerLimit && customTypeIndex < lowerLimit + typeCount)
				return m_list[i]->SerializeCustomParameter(parameterValue, ar, customTypeIndex - lowerLimit);
			lowerLimit += typeCount;
		}

		return "";
	}

	void Reset() override
	{
		int listSize = m_list.size();
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			m_list[i]->Reset();
		}
	}

	void Render(const QuatT& playerPosition, SRendParams& params, const SRenderingPassInfo& passInfo) override
	{
		int listSize = m_list.size();
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			m_list[i]->Render(playerPosition, params, passInfo);
		}
	}

	void Update(ICharacterInstance* character, const QuatT& playerLocation, const Matrix34& cameraMatrix) override
	{
		int listSize = m_list.size();
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			m_list[i]->Update(character, playerLocation, cameraMatrix);
		}
	}

	void EnableAudio(bool enableAudio) override
	{
		int listSize = m_list.size();
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			m_list[i]->EnableAudio(enableAudio);
		}
	}

	bool Play(ICharacterInstance* character, const AnimEventInstance& event) override
	{
		int listSize = m_list.size();
		for (int i = 0; i < listSize; ++i)
		{
			if (!m_list[i])
				continue;
			if (m_list[i]->Play(character, event))
				return true;
		}
		return false;
	}

private:
	std::vector<IAnimEventPlayerPtr> m_list;
	std::vector<string>              m_names;
	string                           m_defaultGamePlayerName;
};
}

