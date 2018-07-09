// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AnimationContent.h"
#include "EditorCompressionPresetTable.h"
#include "EditorDBATable.h"
#include "Shared/AnimationFilter.h"
#include "CharacterDocument.h"
#include <CryAnimation/ICryAnimation.h>
#include "Serialization.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "Explorer/EntryList.h"

namespace CharacterTool {

AnimationContent::AnimationContent()
	: type(ANIMATION)
	, size(-1)
	, importState(NOT_SET)
	, loadedInEngine(false)
	, loadedAsAdditive(false)
	, animationId(-1)
	, delayApplyUntilStart(false)
{
}

bool AnimationContent::HasAudioEvents() const
{
	for (size_t i = 0; i < events.size(); ++i)
		if (IsAudioEventType(events[i].type.c_str()))
			return true;
	return false;
}

void AnimationContent::ApplyToCharacter(bool* triggerPreview, ICharacterInstance* characterInstance, const char* animationPath, bool startingAnimation)
{
	if (startingAnimation)
	{
		if (!delayApplyUntilStart)
			return;
	}

	IAnimEvents* animEvents = gEnv->pCharacterManager->GetIAnimEvents();

	if (IAnimEventList* animEventList = animEvents->GetAnimEventList(animationPath))
	{
		animEventList->Clear();
		for (size_t i = 0; i < events.size(); ++i)
		{
			CAnimEventData animEvent;
			events[i].ToData(&animEvent);
			animEventList->Append(animEvent);
		}
		delayApplyUntilStart = false;
	}
	else
	{
		delayApplyUntilStart = !startingAnimation;
	}

	animEvents->InitializeSegmentationDataFromAnimEvents(animationPath);

	if (type == BLEND_SPACE)
	{
		XmlNodeRef xml = blendSpace.SaveToXml();
		_smart_ptr<IXmlStringData> content = xml->getXMLData();
		gEnv->pCharacterManager->InjectBSPACE(animationPath, content->GetString(), content->GetStringLength());
		gEnv->pCharacterManager->ReloadLMG(animationPath);
		gEnv->pCharacterManager->ClearBSPACECache();
		if (triggerPreview)
			*triggerPreview = true;
	}
	if (type == COMBINED_BLEND_SPACE)
	{
		XmlNodeRef xml = combinedBlendSpace.SaveToXml();
		_smart_ptr<IXmlStringData> content = xml->getXMLData();
		gEnv->pCharacterManager->InjectBSPACE(animationPath, content->GetString(), content->GetStringLength());
		gEnv->pCharacterManager->ReloadLMG(animationPath);
		gEnv->pCharacterManager->ClearBSPACECache();
		if (triggerPreview)
			*triggerPreview = true;
	}

}

void AnimationContent::UpdateBlendSpaceMotionParameters(IAnimationSet* animationSet, IDefaultSkeleton* skeleton)
{
	for (size_t i = 0; i < blendSpace.m_examples.size(); ++i)
	{
		BlendSpaceExample& example = blendSpace.m_examples[i];
		Vec4 v;
		if (animationSet->GetMotionParameters(animationId, i, skeleton, v))
		{
			for (size_t k = 0; k < blendSpace.m_dimensions.size(); ++k)
			{
				const auto paramId = blendSpace.m_dimensions[k].parameterId;
				if (!example.parameters[paramId].userDefined)
				{
					example.parameters[paramId].value = v[k];
				}
			}
		}
	}
}

void AnimationContent::Serialize(Serialization::IArchive& ar)
{
	if (type == ANIMATION && importState == COMPILED_BUT_NO_ANIMSETTINGS)
	{
		ar.warning(*this, "AnimSettings file used to compile the animation is missing. You may need to obtain it from version control.\n\nAlternatively you can create a new AnimSettings file.");
		bool createNewAnimSettings = false;
		ar(Serialization::ToggleButton(createNewAnimSettings), "createButton", "<Create New AnimSettings");
		if (createNewAnimSettings)
			importState = IMPORTED;
	}
	if (type == ANIMATION && (importState == NEW_ANIMATION || importState == IMPORTED))
	{
		ar(settings.build.additive, "additive", "Additive");
	}
	if (type == ANIMATION && importState == NEW_ANIMATION)
	{
		ar(SkeletonAlias(newAnimationSkeleton), "skeletonAlias", "Skeleton Alias");
		if (newAnimationSkeleton.empty())
			ar.warning(newAnimationSkeleton, "Skeleton alias should be specified in order to import animation.");
	}

	if ((type == ANIMATION && (importState == IMPORTED || importState == WAITING_FOR_CHRPARAMS_RELOAD)) || type == AIMPOSE || type == LOOKPOSE)
	{
		ar(SkeletonAlias(settings.build.skeletonAlias), "skeletonAlias", "Skeleton Alias");
		if (settings.build.skeletonAlias.empty())
			ar.error(settings.build.skeletonAlias, "Skeleton alias is not specified for the animation.");
		ar(TagList(settings.build.tags), "tags", "Tags");
	}

	if (type == ANIMATION && (importState == IMPORTED || importState == WAITING_FOR_CHRPARAMS_RELOAD))
	{
		bool presetApplied = false;
		if (ar.isEdit() && ar.isOutput())
		{
			if (auto entryBase = ar.context<Explorer::EntryBase>())
			{
				SAnimationFilterItem item;
				item.path = entryBase->path;
				item.skeletonAlias = settings.build.skeletonAlias;
				item.tags = settings.build.tags;

				if (EditorCompressionPresetTable* presetTable = ar.context<EditorCompressionPresetTable>())
				{
					const EditorCompressionPreset* preset = presetTable->FindPresetForAnimation(item);
					if (preset)
					{
						presetApplied = true;
						if (ar.openBlock("automaticCompressionSettings", "+!Compression Preset"))
						{
							ar(preset->entry.name, "preset", "!^");
							const_cast<EditorCompressionPreset*>(preset)->entry.settings.Serialize(ar);
							ar.closeBlock();
						}
					}
				}

				if (EditorDBATable* dbaTable = ar.context<EditorDBATable>())
				{
					int dbaIndex = dbaTable->FindDBAForAnimation(item);
					if (dbaIndex >= 0)
					{
						string dbaName = dbaTable->GetEntryByIndex(dbaIndex)->entry.path;
						ar(dbaName, "dbaName", "<!DBA");
					}
				}
			}
		}
		if (!ar.isEdit() || !presetApplied)
		{
			int oldFilter = ar.getFilter();
			ar.setFilter(ar.getFilter() | SERIALIZE_COMPRESSION_SETTINGS_AS_TREE);
			ar(settings.build.compression, "compression", "Compression");
			ar.setFilter(oldFilter);
		}
	}
	if (type == BLEND_SPACE)
	{
		if (ar.isEdit() && ar.isOutput())
		{
			if (ICharacterInstance* characterInstance = ar.context<ICharacterInstance>())
			{
				UpdateBlendSpaceMotionParameters(characterInstance->GetIAnimationSet(), &characterInstance->GetIDefaultSkeleton());
			}
		}
		blendSpace.Serialize(ar);
	}
	if (type == COMBINED_BLEND_SPACE)
	{
		combinedBlendSpace.Serialize(ar);
	}
	if (type == ANM)
	{
		string msg = "Contains no properties.";
		ar(msg, "msg", "<!");
	}
	if (type != ANM && type != AIMPOSE && type != LOOKPOSE && (importState == IMPORTED || importState == WAITING_FOR_CHRPARAMS_RELOAD || importState == COMPILED_BUT_NO_ANIMSETTINGS))
	{
		ar(events, "events", "Animation Events");
	}
	ar(size, "size");
}

void AnimationContent::Reset()
{
	*this = AnimationContent();
}

}

