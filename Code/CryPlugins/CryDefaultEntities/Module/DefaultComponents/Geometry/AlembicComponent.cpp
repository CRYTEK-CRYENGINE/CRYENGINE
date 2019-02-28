#include "StdAfx.h"
#include "AlembicComponent.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
	namespace DefaultComponents
	{
		void CAlembicComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAlembicComponent::Enable, "{97B6CD06-8409-4C86-8AE4-E13F1969CEDD}"_cry_guid, "Enable");
				pFunction->SetDescription("Enables playback of the alembic component");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'enab', "Enable");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAlembicComponent::SetLooping, "{712B5FDD-96BC-4C2A-ACB7-A35252ACD9D2}"_cry_guid, "SetLooping");
				pFunction->SetDescription("Makes the alembic component loop playback");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'loop', "Loop");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAlembicComponent::IsLooping, "{221752A5-051D-4C90-B51F-C669C0EC14AD}"_cry_guid, "IsLooping");
				pFunction->SetDescription("Is the alembic component looping plyback?");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindOutput(0, 'loop', "Looping");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAlembicComponent::SetPlaybackTime, "{5B435614-8E6C-428A-8641-7CD13442B054}"_cry_guid, "SetPlaybackTime");
				pFunction->SetDescription("Skips to a certain time in the animation");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'time', "Time");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAlembicComponent::GetPlaybackTime, "{C5ECE743-88CF-4A51-AAE0-105012AC7453}"_cry_guid, "GetPlaybackTime");
				pFunction->SetDescription("Gets the current playback time");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindOutput(0, 'time', "Time");
				componentScope.Register(pFunction);
			}
		}

		void CAlembicComponent::Initialize()
		{
			if (m_filePath.value.size() > 0)
			{
				m_pEntity->LoadGeomCache(GetOrMakeEntitySlotId(), m_filePath.value);
			}
		}

		void CAlembicComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				Initialize();
			}
		}

		uint64 CAlembicComponent::GetEventMask() const
		{
			return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
		}

		void CAlembicComponent::Enable(bool bEnable)
		{
			if (IGeomCacheRenderNode* pRenderNode = m_pEntity->GetGeomCacheRenderNode(GetEntitySlotId()))
			{
				pRenderNode->SetDrawing(bEnable);
			}
		}

		void CAlembicComponent::SetLooping(bool bLooping)
		{
			if (IGeomCacheRenderNode* pRenderNode = m_pEntity->GetGeomCacheRenderNode(GetEntitySlotId()))
			{
				pRenderNode->SetLooping(bLooping);
			}
		}

		bool CAlembicComponent::IsLooping() const
		{
			if (IGeomCacheRenderNode* pRenderNode = m_pEntity->GetGeomCacheRenderNode(GetEntitySlotId()))
			{
				return pRenderNode->IsLooping();
			}

			return false;
		}

		void CAlembicComponent::SetPlaybackTime(float time)
		{
			if (IGeomCacheRenderNode* pRenderNode = m_pEntity->GetGeomCacheRenderNode(GetEntitySlotId()))
			{
				pRenderNode->SetPlaybackTime(time);
			}
		}

		float CAlembicComponent::GetPlaybackTime() const
		{
			if (IGeomCacheRenderNode* pRenderNode = m_pEntity->GetGeomCacheRenderNode(GetEntitySlotId()))
			{
				return pRenderNode->GetPlaybackTime();
			}

			return 0.f;
		}

		void CAlembicComponent::SetFilePath(const char* szFilePath)
		{
			m_filePath = szFilePath;
		}
	}
}