// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryAISystem/INavigationSystem.h>

#include "Navigation/NavigationSystemSchematyc.h"

class CAINavigationMarkupShapeComponent final 
	: public IEntityComponent
	, public INavigationSystem::INavigationSystemListener
{
public:
	static const CryGUID& IID()
	{
		static CryGUID id = "3ECDA25A-CCD6-4A2F-A0E4-31E3EB61E74E"_cry_guid;
		return id;
	}

	CAINavigationMarkupShapeComponent();

	// IEntityComponent
	virtual void          Initialize() override;
	virtual void          OnShutDown() override;
	virtual uint64        GetEventMask() const override;
	virtual void          ProcessEvent(const SEntityEvent& event) override;
	virtual IEntityComponentPreviewer* GetPreviewer() override;
	// ~IEntityComponent

	// INavigationSystem::INavigationSystemListener
	virtual void OnNavigationEvent(const INavigationSystem::ENavigationEvent event) override;
	// ~INavigationSystem::INavigationSystemListener

	static void           ReflectType(Schematyc::CTypeDesc<CAINavigationMarkupShapeComponent>& desc);
	static void           Register(Schematyc::IEnvRegistrar& registrar);

	void RenderVolume() const;

private:
	void UpdateAnnotation();
	void UpdateVolume();

	void SetAnnotationFlag(const NavigationAreaFlagID& flagId, bool bEnable);

	//////////////////////////////////////////////////////////////////////////
	void ChangeAreaAnotation(const NavigationComponentHelpers::SAnnotationFlagsMask& flags);
	void ResetAnotation();
	//////////////////////////////////////////////////////////////////////////

	NavigationAreaTypeID m_areaTypeId;
	NavigationComponentHelpers::SAgentTypesMask m_affectedAgentTypesMask;
	Vec3 m_size;
	bool m_bStoreTriangles = false;
	bool m_bExpandByAgentRadius = false;

	MNM::AreaAnnotation m_defaultAreaAnotation;
	MNM::AreaAnnotation m_currentAreaAnotation;

	NavigationVolumeID m_volumeId;
};
