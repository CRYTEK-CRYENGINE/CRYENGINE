// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AnimEvent.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimEventPlayer.h>
#include "Serialization.h"
#include <CrySerialization/Decorators/ResourcesAudio.h>

namespace CharacterTool
{

bool IsAudioEventType(const char* type)
{
	static const char* audioEvents[] = { "sound", "foley", "footstep" };
	for (int i = 0; i < sizeof audioEvents / sizeof audioEvents[0]; ++i)
		if (stricmp(type, audioEvents[i]))
			return true;
	return false;
}

void AnimEvent::Serialize(IArchive& ar)
{
	if (!ar.isEdit())
	{
		ar(startTime, "startTime");
		ar(endTime, "endTime");
		ar(type, "type");
		ar(parameter, "parameter");
		ar(boneName, "boneName");
		ar(offset, "offset");
		ar(direction, "direction");
		ar(model, "model");
	}
	else
	{
		if (startTime >= 0.0f)
		{
			ar(startTime, "startTime", ">50>^");
			ar(endTime, "endTime", 0);
			if (ar.isInput())
			{
				if (startTime < 0.0f)
					startTime = 0.0f;
				if (endTime < 0.0f)
					endTime = 0.0f;
			}
		}

		IAnimEventPlayer* player = ar.context<IAnimEventPlayer>();
		int customEventCount = 0;

		Serialization::StringList eventList;
		eventList.push_back("Custom");
		eventList.push_back("segment1");
		eventList.push_back("segment2");
		eventList.push_back("segment3");
		if (player)
		{
			customEventCount = player->GetCustomTypeCount();
			for (int i = 0; i < customEventCount; ++i)
			{
				if (const SCustomAnimEventType* type = player->GetCustomType(i))
					eventList.push_back(type->name);
			}
		}
		std::sort(eventList.begin() + 1, eventList.end());
		eventList.erase(std::unique(eventList.begin() + 1, eventList.end()), eventList.end());

		string typeLower = type;
		typeLower.MakeLower();
		int typeIndex = eventList.find(typeLower.c_str());
		Serialization::StringListValue typeChoice(eventList, typeIndex == -1 ? 0 : typeIndex);
		int oldIndex = typeChoice.index();
		ar(typeChoice, "typeChoice", "^");
		if (ar.isInput() && oldIndex != typeChoice.index())
		{
			if (typeChoice.index() > 0)
				type = typeChoice.c_str();
			else if (eventList.find(type.c_str()) != 0)
				type.clear();
		}
		if (typeChoice.index() <= 0) // Custom
			ar(type, "type", "^");

		int customIndex = -1;
		int usedParameters = typeChoice.index() == 0 ? ANIM_EVENT_USES_ALL_PARAMETERS : 0;
		const char* tooltip = "";
		for (int i = 0; i < customEventCount; ++i)
			if (const SCustomAnimEventType* customType = player->GetCustomType(i))
				if (customType->name && type == customType->name)
				{
					usedParameters = customType->parameters;
					tooltip = customType->description;
					customIndex = i;
					break;
				}
		ar.doc(tooltip);

		if (customIndex != -1 && player)
			parameter = player->SerializeCustomParameter(parameter.c_str(), ar, customIndex);
		else
		{
			ar(parameter, "parameter", "Parameter");
			ar.doc("Custom parameter is interpeted depending on the anim event type.");
		}

		if (usedParameters & ANIM_EVENT_USES_BONE_INLINE)
		{
			ar(JointName(boneName), "boneName", "^");
			ar.doc("When specified, position the AudioProxy on this Joint");
		}
		else if (usedParameters & ANIM_EVENT_USES_BONE)
		{
			ar(JointName(boneName), "boneName", "Joint Name");
			ar.doc("When specified, position the AudioProxy on this Joint");
		}
		else
			ar(JointName(boneName), "boneName", 0);

		if (usedParameters & ANIM_EVENT_USES_OFFSET_AND_DIRECTION)
		{
			ar(LocalToJoint(offset, boneName), "offset", "Offset");
			ar(LocalToJoint(direction, boneName), "direction", "Direction");
		}
		else
		{
			ar(LocalToJoint(offset, boneName), "offset", 0);
			ar(LocalToJoint(direction, boneName), "direction", 0);
		}

		ar(model, "model");
	}
}

void AnimEvent::FromData(const CAnimEventData& eventData)
{
	startTime = eventData.GetNormalizedTime();
	endTime = eventData.GetNormalizedEndTime();
	type = eventData.GetName();
	parameter = eventData.GetCustomParameter();
	boneName = eventData.GetBoneName();
	offset = eventData.GetOffset();
	direction = eventData.GetDirection();
	model = eventData.GetModelName();
}

void AnimEvent::ToData(CAnimEventData* data) const
{
	data->SetNormalizedTime(startTime);
	data->SetNormalizedEndTime(endTime);
	data->SetName(type.c_str());
	data->SetCustomParameter(parameter.c_str());
	data->SetBoneName(boneName.c_str());
	data->SetOffset(offset);
	data->SetDirection(direction);
	data->SetModelName(model.c_str());
}

void AnimEvent::ToInstance(AnimEventInstance* instance) const
{
	instance->m_time = startTime;
	instance->m_endTime = endTime;
	instance->m_EventName = type.c_str();
	instance->m_EventNameLowercaseCRC32 = CCrc32::ComputeLowercase(type.c_str());
	instance->m_CustomParameter = parameter.c_str();
	instance->m_BonePathName = boneName.c_str();
	instance->m_vOffset = offset;
	instance->m_vDir = direction;
}

bool AnimEvent::LoadFromXMLNode(const XmlNodeRef& dataIn)
{
	if (!dataIn)
		return false;
	if (stack_string("event") != dataIn->getTag())
		return false;

	type = dataIn->getAttr("name");
	dataIn->getAttr("time", startTime = 0.0f);
	dataIn->getAttr("endTime", endTime = startTime);
	parameter = dataIn->getAttr("parameter");
	boneName = dataIn->getAttr("bone");
	dataIn->getAttr("offset", offset = Vec3(0, 0, 0));
	dataIn->getAttr("dir", direction = Vec3(0, 0, 0));
	model = dataIn->getAttr("model");
	return true;
}

void AnimEventPreset::Serialize(IArchive& ar)
{
	ar(name, "name", "Name");
	ar(Serialization::Decorators::Range<float>(colorHue, 0.0f, 1.0f), "colorHue", "Color Hue");
	ar(event, "event", "<Event");
	if (ar.isInput())
	{
		// a little hack to hide time from presets
		event.startTime = -1.0f;
		event.endTime = -1.0f;
	}
}

}

