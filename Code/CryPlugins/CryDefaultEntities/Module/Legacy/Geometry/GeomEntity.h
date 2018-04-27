#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

#include <CrySerialization/Decorators/Resources.h>

class CGeomEntity final 
	: public CDesignerEntityComponent<IGeometryEntityComponent>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CGeomEntity, IGeometryEntityComponent, "GeomEntity", "ec0cd266-a6d1-4774-b499-690bd6fb61ee"_cry_guid);

	virtual ~CGeomEntity() {}

public:
	enum EInputPorts
	{
		eInputPort_Hide = 0,
		eInputPort_UnHide,
		eInputPort_Geometry
	};

	enum EOutputPorts
	{
		eOutputPort_OnHide = 0,
		eOutputPort_OnUnHide,
		eOutputPort_OnCollision,
		eOutputPort_CollisionSurfaceName,
		eOutputPort_OnGeometryChanged
	};

	enum EPhysicalizationType
	{
		ePhysicalizationType_None = 0,
		ePhysicalizationType_Static,
		ePhysicalizationType_Rigid,
		ePhysicalizationType_Area
	};

	struct SPhysicsProperties
	{
		bool operator==( const SPhysicsProperties &rhs ) const
		{
			return m_physicalizationType == rhs.m_physicalizationType &&
							m_mass == rhs.m_mass &&
							m_density == rhs.m_density &&
							m_bReceiveCollisionEvents == rhs.m_bReceiveCollisionEvents;
		}
	
		EPhysicalizationType m_physicalizationType = ePhysicalizationType_Static;
		float m_mass = 1.f;
		float m_density = 1.f;
		bool m_bReceiveCollisionEvents = false;
	};

public:
	// ISimpleExtension
	virtual void Initialize() final;

	virtual void ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final;

	virtual void OnResetState() final;

	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }
	// ~ISimpleExtension

	// IGeometryEntityComponent
	virtual void SetGeometry(const char* szFilePath) override;
	// IGeometryEntityComponent

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "Geometry Properties"; }
	virtual void SerializeProperties(Serialization::IArchive& archive) override;
	// ~IEntityPropertyGroup

public:
	static void OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode);

protected:
	IPhysicalEntity* m_pPhysEnt;

	Schematyc::GeomFileName m_model;

	SPhysicsProperties m_physicsProperties;

	string m_animation;
	float m_animationSpeed = 1.f;
	bool m_bLoopAnimation = true;
};
