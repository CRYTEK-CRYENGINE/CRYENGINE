// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverUserComponent.h"
#include "CoverMovementBlocks.h"

#include "Environment.h"

#include "Cover/CoverSystem.h"
#include "Cover/CoverSystemSchematyc.h"

#include "Movement/MovementSystem.h"
#include "Components/Navigation/NavigationComponent.h"

//////////////////////////////////////////////////////////////////////////
// Reflect Signals

void CEntityAICoverUserComponent::SCoverCompromisedSignal::ReflectType(Schematyc::CTypeDesc<SCoverCompromisedSignal>& desc)
{
	desc.SetGUID("DB200CEA-B4F7-4F0F-9699-9DC1864B7B0B"_cry_guid);
	desc.SetLabel("CoverCompromised");
	desc.SetDescription("Sent when the cover where the entity is hiding or is moving to is exposed.");
	desc.AddMember(&SCoverCompromisedSignal::coverId, 'cid', "coverId", "Cover ID", nullptr, CoverID());
}

void CEntityAICoverUserComponent::SCoverEnteredSignal::ReflectType(Schematyc::CTypeDesc<SCoverEnteredSignal>& desc)
{
	desc.SetGUID("203EDECB-215C-4DDE-BEE1-B87468602626"_cry_guid);
	desc.SetLabel("CoverEntered");
	desc.SetDescription("Sent when the entity enters the cover.");
	desc.AddMember(&SCoverEnteredSignal::coverId, 'cid', "coverId", "Cover ID", nullptr, CoverID());
}

void CEntityAICoverUserComponent::SCoverLeftSignal::ReflectType(Schematyc::CTypeDesc<SCoverLeftSignal>& desc)
{
	desc.SetGUID("0D6713E2-09C2-42AD-8961-B4FFCA7C6F9F"_cry_guid);
	desc.SetLabel("CoverLeft");
	desc.SetDescription("Sent when the entity leaves the cover.");
	desc.AddMember(&SCoverLeftSignal::coverId, 'cid', "coverId", "Cover ID", nullptr, CoverID());
}

void CEntityAICoverUserComponent::SMoveToCoverFailedSignal::ReflectType(Schematyc::CTypeDesc<SMoveToCoverFailedSignal>& desc)
{
	desc.SetGUID("117C8F12-F137-4991-A169-566321635AEE"_cry_guid);
	desc.SetLabel("MoveToCoverFailed");
	desc.SetDescription("Sent when the movement request to the cover failed.");
	desc.AddMember(&SMoveToCoverFailedSignal::coverId, 'cid', "coverId", "Cover ID", nullptr, CoverID());
}

void CEntityAICoverUserComponent::SRefreshEyesSignal::ReflectType(Schematyc::CTypeDesc<SRefreshEyesSignal>& desc)
{
	desc.SetGUID("C9AFCB5B-EE30-4BED-9A98-1AC51F8ADC47"_cry_guid);
	desc.SetLabel("RefreshEyes");
	desc.SetDescription("Sent when cover user wants to update possible cover eyes.");
}

//////////////////////////////////////////////////////////////////////////
CEntityAICoverUserComponent::CEntityAICoverUserComponent()
	: m_pCoverUser(nullptr)
	, m_pCoverEyesTemp(nullptr)
{
}

CEntityAICoverUserComponent::~CEntityAICoverUserComponent()
{
}

void CEntityAICoverUserComponent::ReflectType(Schematyc::CTypeDesc<CEntityAICoverUserComponent>& desc)
{
	desc.AddBase<IEntityCoverUserComponent>();
	desc.SetGUID("D3C6A814-DBEE-4CB1-9322-9AC6028A3EB8"_cry_guid);

	desc.SetLabel("AI Cover User");
	desc.SetDescription("Cover user component");
	desc.SetEditorCategory("AI");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Singleton, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

	desc.AddComponentInteraction(SEntityComponentRequirements::EType::SoftDependency, IEntity::SComponentType<CEntityAINavigationComponent>::GetGUID());

	desc.AddMember(&CEntityAICoverUserComponent::m_effectiveCoverHeight, 'ech', "effectiveCoverHeight", "Min Effective Cover Height", nullptr, 0.85f);
	desc.AddMember(&CEntityAICoverUserComponent::m_inCoverRadius, 'icr', "inCoverRadius", "In Cover Radius", nullptr, 0.3f);
	desc.AddMember(&CEntityAICoverUserComponent::m_distanceToCover, 'dtc', "distToCover", "Distance To Cover", nullptr, 0.5f);
	desc.AddMember(&CEntityAICoverUserComponent::m_blackListTime, 'blt', "blacklistTime", "Blacklist Time", nullptr, 10.0f);
}

void CEntityAICoverUserComponent::Register(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CEntityAICoverUserComponent));

		// Functions
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::GetRandomCoverId, "07C39B63-5D8F-4C7B-8730-4BA189E20461"_cry_guid, "GetRandomCover");
			pFunction->SetDescription("Returns random unoccupied CoverId in specified radius from the entity");
			pFunction->BindInput(1, 'rad', "Radius", "Radius to search");
			pFunction->BindOutput(0, 'rv', "CoverId", "Returned Cover Id");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::MoveToCover, "04B97008-7145-43F8-9099-947D376770B1"_cry_guid, "MoveToCover");
			pFunction->SetDescription("Starts move to cover movement request.");
			pFunction->BindInput(1, 'cid', "CoverId", "Cover Id");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::IsInCover, "53E71E95-48DB-4A77-A31D-409E65624D0B"_cry_guid, "IsInCover");
			pFunction->SetDescription("Returns whether the entity is hidden in the cover.");
			pFunction->BindOutput(0, 'rv', "InCover", "True if the entity is positioned in the cover.");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::IsMovingToCover, "5D5CDA5F-EB09-4C47-A563-CF983129EB06"_cry_guid, "IsMovingToCover");
			pFunction->SetDescription("Returns whether the entity is moving to cover.");
			pFunction->BindOutput(0, 'rv', "MovingToCover", "True if the entity is moving to the cover.");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::IsCoverCompromised, "AA5E2D2B-9208-4AF9-91BD-B4820F9A4F94"_cry_guid, "IsCoverCompromised");
			pFunction->SetDescription("Returns whether the actual cover is compromised.");
			pFunction->BindOutput(0, 'rv', "Compromised", "True if the actual cover is compromised.");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::GetCurrentCoverPosition, "D53E7DD9-F0C9-454A-AE95-AEA78B3ADA92"_cry_guid, "GetCurrentCoverPosition");
			pFunction->SetDescription("Returns position of the active cover.");
			pFunction->BindOutput(0, 'rv', "Success", "True if there is active cover and position was set.");
			pFunction->BindOutput(1, 'pos', "Position", "Position of the cover.");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::GetCurrentCoverNormal, "293BB9B6-5EDD-4A01-A53B-388521776FB1"_cry_guid, "GetCurrentCoverNormal");
			pFunction->SetDescription("Returns normal to the active cover surface respecting the current cover user position.");
			pFunction->BindOutput(0, 'rv', "Success", "True if there is active cover and normal was set.");
			pFunction->BindOutput(1, 'nrm', "Normal", "Normal of the cover.");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAICoverUserComponent::AddCoverEye, "E512537F-A637-4210-96AC-DED9FE506DB0"_cry_guid, "AddCoverEye");
			pFunction->SetDescription("Adds eye position that could possibly expose the current cover. Should be only called when responding to RefreshEyes signal. ");
			pFunction->BindInput(1, 'epos', "EyePosition", "Position of the eye.");
			componentScope.Register(pFunction);
		}
		// Signals
		{
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SCoverCompromisedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SCoverEnteredSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SCoverLeftSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SMoveToCoverFailedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SRefreshEyesSignal));
		}
	}
}

void CEntityAICoverUserComponent::Initialize()
{
}

void CEntityAICoverUserComponent::Start()
{
	CoverUser::Params params;
	params.userID = GetEntityId();
	params.inCoverRadius = m_inCoverRadius;
	params.distanceToCover = m_distanceToCover;
	params.minEffectiveCoverHeight = m_effectiveCoverHeight;
	params.fillCoverEyesCustomMethod = functor(*this, &CEntityAICoverUserComponent::FillCoverEyes);
	params.activeCoverInvalidateCallback = functor(*this, &CEntityAICoverUserComponent::OnSetCoverCompromised);
	params.activeCoverCompromisedCallback = functor(*this, &CEntityAICoverUserComponent::OnSetCoverCompromised);

	m_pCoverUser = gEnv->pAISystem->GetCoverSystem()->RegisterEntity(GetEntityId(), params);

	SMovementActionAbilityCallbacks coverAbility;
	coverAbility.addStartMovementBlocksCallback = functor(*this, &CEntityAICoverUserComponent::CreatePlanStartBlocks);
	coverAbility.addEndMovementBlocksCallback = functor(*this, &CEntityAICoverUserComponent::CreatePlanEndBlocks);

	if (!gEnv->pAISystem->GetMovementSystem()->AddActionAbilityCallbacks(GetEntityId(), coverAbility))
	{
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "Entity '%s' wasn't registered to Movement system before trying to add Cover action ability.", GetEntity()->GetName());
	}
}

void CEntityAICoverUserComponent::Stop()
{
	CancelMovementRequest();

	if (m_pCoverUser)
	{
		SMovementActionAbilityCallbacks coverAbility;
		coverAbility.addStartMovementBlocksCallback = functor(*this, &CEntityAICoverUserComponent::CreatePlanStartBlocks);
		coverAbility.addEndMovementBlocksCallback = functor(*this, &CEntityAICoverUserComponent::CreatePlanEndBlocks);

		gEnv->pAISystem->GetMovementSystem()->RemoveActionAbilityCallbacks(GetEntityId(), coverAbility);
		gEnv->pAISystem->GetCoverSystem()->UnregisterEntity(GetEntityId());
		m_pCoverUser = nullptr;
	}
}

void CEntityAICoverUserComponent::OnShutDown()
{
	Stop();
}

bool CEntityAICoverUserComponent::IsGameOrSimulation() const
{
	return gEnv->IsGameOrSimulation();
}

uint64 CEntityAICoverUserComponent::GetEventMask() const 
{ 
	return ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

void CEntityAICoverUserComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_LEVEL_LOADED:
		{
			if (IsGameOrSimulation())
			{
				Start();
			}
			break;
		}
		case ENTITY_EVENT_RESET:
		{
			if (GetEntity()->GetSimulationMode() != EEntitySimulationMode::Game)
			{
				Stop();
			}
			break;
		}
	}
}

void CEntityAICoverUserComponent::MoveToCover(const CoverID& coverId)
{
	ReserveNextCover(coverId);

	if (!coverId)
		return;

	CancelMovementRequest();

	Vec3 targetPosition = gAIEnv.pCoverSystem->GetCoverLocation(coverId);

	MovementRequest request;
	request.destination = targetPosition;
	request.type = MovementRequest::MoveTo;
	request.entityID = GetEntity()->GetId();
	request.callback = functor(*this, &CEntityAICoverUserComponent::MovementRequestCompleted);

	m_moveToCoverRequestId = gEnv->pAISystem->GetMovementSystem()->QueueRequest(request);
}

void CEntityAICoverUserComponent::CancelMovementRequest()
{
	if (m_moveToCoverRequestId != MovementRequestID::Invalid())
	{
		gEnv->pAISystem->GetMovementSystem()->CancelRequest(m_moveToCoverRequestId);
		m_moveToCoverRequestId = MovementRequestID::Invalid();
	}
}

void CEntityAICoverUserComponent::MovementRequestCompleted(const MovementRequestResult& result)
{
	m_moveToCoverRequestId = MovementRequestID();

	if (result.result == MovementRequestResult::Failure)
	{
		ProcessMoveToCoverFailedEvent(m_pCoverUser->GetCoverID());
		ReserveNextCover(CoverID());
		SetCurrentCover(CoverID());
		SetState(ICoverUser::EStateFlags::None);
	}
}

void CEntityAICoverUserComponent::FillCoverEyes(DynArray<Vec3>& eyesContainer)
{
	if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
	{
		m_pCoverEyesTemp = &eyesContainer;
		pSchematycObject->ProcessSignal(SRefreshEyesSignal(), GetGUID());
		m_pCoverEyesTemp = nullptr;
	}
	if (m_refreshEyesCustomFunction)
	{
		m_refreshEyesCustomFunction(eyesContainer);
	}
}

void CEntityAICoverUserComponent::AddCoverEye(const Vec3& eyePos)
{
	if (m_pCoverEyesTemp)
	{
		m_pCoverEyesTemp->push_back(eyePos);
	}
}

void CEntityAICoverUserComponent::OnSetCoverCompromised(CoverID coverId, ICoverUser* pCoverUser)
{
	if (!m_pCoverUser->GetState().IsEmpty())
	{
		if (coverId)
		{
			SetCoverBlacklisted(coverId, true, m_blackListTime);
			ProcessCoverCompromisedEvent(coverId);
		}
	}
}

void CEntityAICoverUserComponent::SetCoverBlacklisted(const CoverID& coverID, bool blacklist, float time)
{
	m_pCoverUser->SetCoverBlacklisted(coverID, blacklist, time);
}

bool CEntityAICoverUserComponent::IsCoverBlackListed(const CoverID& coverId) const
{
	return m_pCoverUser->IsCoverBlackListed(coverId);
}

void CEntityAICoverUserComponent::SetState(ICoverUser::StateFlags state)
{
	if (state.Check(ICoverUser::EStateFlags::InCover))
	{
		CRY_ASSERT(m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::MovingToCover));
		if (!(m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::InCover)))
		{
			ProcessCoverEnteredEvent(m_pCoverUser->GetCoverID());
		}
	}
	else if (state.Check(ICoverUser::EStateFlags::MovingToCover))
	{
		CRY_ASSERT(!m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::InCover));
	}
	else if (state.IsEmpty())
	{
		if (m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::InCover))
		{
			ProcessCoverLeftEvent(m_pCoverUser->GetCoverID());
		}
	}
	m_pCoverUser->SetState(state);
}

bool CEntityAICoverUserComponent::GetCurrentCoverPosition(Vec3& position) const
{
	if (!m_pCoverUser->GetCoverID())
		return false;

	position = m_pCoverUser->GetCoverLocation(m_pCoverUser->GetCoverID());
	return true;
}

bool CEntityAICoverUserComponent::GetCurrentCoverNormal(Vec3& normal) const
{
	if (!m_pCoverUser->GetCoverID())
		return false;

	normal = m_pCoverUser->GetCoverNormal(GetEntity()->GetWorldPos());
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityAICoverUserComponent::ProcessCoverEnteredEvent(const CoverID& coverID)
{
	if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
	{
		pSchematycObject->ProcessSignal(SCoverEnteredSignal({ coverID }), GetGUID());
	}
	if (m_callbacks.coverEnteredCallback)
	{
		m_callbacks.coverEnteredCallback(coverID);
	}
}

void CEntityAICoverUserComponent::ProcessCoverLeftEvent(const CoverID& coverID)
{
	if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
	{
		pSchematycObject->ProcessSignal(SCoverLeftSignal({ coverID }), GetGUID());
	}
	if (m_callbacks.coverLeftCallback)
	{
		m_callbacks.coverLeftCallback(coverID);
	}
}

void CEntityAICoverUserComponent::ProcessCoverCompromisedEvent(const CoverID& coverID)
{
	if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
	{
		pSchematycObject->ProcessSignal(SCoverCompromisedSignal({ coverID }), GetGUID());
	}
	if (m_callbacks.coverCompromisedCallback)
	{
		m_callbacks.coverCompromisedCallback(coverID);
	}
}

void CEntityAICoverUserComponent::ProcessMoveToCoverFailedEvent(const CoverID& coverID)
{
	if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
	{
		pSchematycObject->ProcessSignal(SMoveToCoverFailedSignal({ coverID }), GetGUID());
	}
	if (m_callbacks.moveToCoverFailedCallback)
	{
		m_callbacks.moveToCoverFailedCallback(coverID);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAICoverUserComponent::CreatePlanStartBlocks(DynArray<Movement::BlockPtr>& blocks, const MovementRequest& request)
{
	if (m_pCoverUser)
	{
		if (m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::InCover))
		{
			blocks.push_back(Movement::BlockPtr(new LeaveCoverMovementBlock(this)));
		}
		if (m_pCoverUser->GetNextCoverID())
		{
			blocks.push_back(Movement::BlockPtr(new SetupCoverMovementBlock(this, m_pCoverUser->GetNextCoverID())));
		}
	}
}

void CEntityAICoverUserComponent::CreatePlanEndBlocks(DynArray<Movement::BlockPtr>& blocks, const MovementRequest& request)
{
	if (m_pCoverUser && m_pCoverUser->GetNextCoverID())
	{
		blocks.push_back(Movement::BlockPtr(new EnterCoverMovementBlock(this)));
	}
}

CoverID CEntityAICoverUserComponent::GetRandomCoverId(float radius) const
{
	const Vec3 entityPos = GetEntity()->GetWorldPos();
	
	std::vector<CoverID> covers;
	covers.reserve(16);
	gAIEnv.pCoverSystem->GetCover(GetEntity()->GetWorldPos(), radius, covers);

	std::vector<CoverID> validCovers;
	validCovers.reserve(covers.size());

	for (const CoverID& coverId : covers)
	{
		if (gAIEnv.pCoverSystem->IsCoverOccupied(coverId))
			continue;

		if(IsCoverBlackListed(coverId))
			continue;

		validCovers.push_back(coverId);
	}

	return validCovers.size() ? validCovers[cry_random(size_t(0), validCovers.size() - 1)] : CoverID();
}
