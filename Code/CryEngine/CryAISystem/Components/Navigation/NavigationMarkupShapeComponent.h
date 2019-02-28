// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

public:
	struct SMarkupShapeProperties
	{
		typedef Schematyc::CStringHashWrapper<Schematyc::CFastStringHash, Schematyc::CEmptyStringConversion, string> Name;
		
		static void ReflectType(Schematyc::CTypeDesc<SMarkupShapeProperties>& desc);
		void Serialize(Serialization::IArchive& archive);
		bool operator==(const SMarkupShapeProperties& other) const
		{
			return position == other.position
				&& position == other.rotation
				&& areaTypeId == other.areaTypeId
				&& affectedAgentTypesMask == other.affectedAgentTypesMask
				&& size == other.size
				&& bIsStatic == other.bIsStatic
				&& bExpandByAgentRadius == other.bExpandByAgentRadius
				&& name == other.name;
		}

		Vec3 position = ZERO;
		Ang3 rotation = ZERO;
		NavigationAreaTypeID areaTypeId;
		NavigationComponentHelpers::SAgentTypesMask affectedAgentTypesMask;
		Vec3 size = Vec3(2.0f, 2.0f, 2.0f);
		bool bIsStatic = false;
		bool bExpandByAgentRadius = false;
		Name name;
	};

	struct SShapesArray
	{
		static void ReflectType(Schematyc::CTypeDesc<SShapesArray>& typeInfo)
		{
			typeInfo.SetGUID("AFCBE3EC-7A62-4233-8E88-BB5768498DF2"_cry_guid);
		}
		bool operator==(const SShapesArray& other) const { return shapes == other.shapes; }

		std::vector<SMarkupShapeProperties> shapes;
	};

private:
	struct SRuntimeData
	{
		MNM::AreaAnnotation m_defaultAreaAnotation;
		MNM::AreaAnnotation m_currentAreaAnotation;

		NavigationVolumeID m_volumeId;
	};

	void RegisterAndUpdateComponent();
	void UpdateAnnotations();
	void UpdateVolumes();

	void SetAnnotationFlag(const Schematyc::CSharedString& shapeName, const NavigationAreaFlagID& flagId, bool bEnable);

	void ToggleAnnotationFlags(const Schematyc::CSharedString& shapeName, const NavigationComponentHelpers::SAnnotationFlagsMask& flags);
	void ResetAnotations();

	SShapesArray m_shapesProperties;
	bool m_isGeometryIgnoredInNavMesh = true;

	std::vector<SRuntimeData> m_runtimeData;
};

inline bool Serialize(Serialization::IArchive& archive, CAINavigationMarkupShapeComponent::SShapesArray& value, const char* szName, const char* szLabel)
{
	return archive(value.shapes, szName, szLabel);
}
