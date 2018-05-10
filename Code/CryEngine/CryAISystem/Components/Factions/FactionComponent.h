// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Factions/FactionSystem.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvRegistrar;
// Forward declare structures.
struct SUpdateContext;
}

class CEntityAIFactionComponent final : public IEntityComponent
{
public:
	static const CryGUID& IID()
	{
		static CryGUID id = "214527d9-156b-4a50-90a5-3dc4a362d800"_cry_guid;
		return id;
	}

	struct SReactionChangedSignal
	{
		static void ReflectType(Schematyc::CTypeDesc<SReactionChangedSignal>& typeInfo);
		SFactionID m_otherFaction;
		IFactionMap::ReactionType m_reactionType;
	};

	static void ReflectType(Schematyc::CTypeDesc<CEntityAIFactionComponent>& desc);
	static void Register(Schematyc::IEnvRegistrar& registrar);

	CEntityAIFactionComponent();
	virtual ~CEntityAIFactionComponent();

	// IEntityComponent
	virtual void Initialize() override;
	virtual void OnShutDown() override;
	// ~IEntityComponent

	SFactionID GetFactionId() const;
	void SetFactionId(const SFactionID& factionId);
	bool IsReactionEqual(Schematyc::ExplicitEntityId otherEntity, IFactionMap::ReactionType reaction) const;
	SFactionFlagsMask GetFactionMaskByReaction(IFactionMap::ReactionType reactionType) const;

	void OnReactionChanged(uint8 factionId, IFactionMap::ReactionType reaction);

private:
	void RegisterFactionId(const SFactionID& factionId);

	SFactionID m_factionId;
};
