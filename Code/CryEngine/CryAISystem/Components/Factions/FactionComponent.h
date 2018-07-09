// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/Components/IEntityFactionComponent.h>
#include "Factions/FactionSystem.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvRegistrar;
// Forward declare structures.
struct SUpdateContext;
}

class CEntityAIFactionComponent final : public IEntityFactionComponent
{
public:
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

	// IEntityComponent
	virtual uint8 GetFactionId() const override;
	virtual void SetFactionId(const uint8 factionId) override;
	virtual IFactionMap::ReactionType GetReaction(const EntityId otherEntityId) const override;
	virtual void SetReactionChangedCallback(std::function<void(const uint8, const IFactionMap::ReactionType)> callbackFunction) override;
	// IEntityComponent

private:
	SFactionID GetFactionIdSchematyc() const;
	void SetFactionIdSchematyc(const SFactionID& factionId);
	bool IsReactionEqual(Schematyc::ExplicitEntityId otherEntity, IFactionMap::ReactionType reaction) const;
	SFactionFlagsMask GetFactionMaskByReaction(IFactionMap::ReactionType reactionType) const;

	void OnReactionChanged(uint8 factionId, IFactionMap::ReactionType reaction);

	void RegisterFactionId(const SFactionID& factionId);

	SFactionID m_factionId;
	std::function<void(const uint8, const IFactionMap::ReactionType)> m_reactionChangedCallback;
};
