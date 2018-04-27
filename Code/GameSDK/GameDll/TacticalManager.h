// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TACTICALMANAGER_H__
#define __TACTICALMANAGER_H__

//////////////////////////////////////////////////////////////////////////

#include "UI/UITypes.h"

class CTacticalManager
{

public:

	enum ETacticalEntityType // Value is also index in m_allTacticalPoints
	{
		eTacticalEntity_Story					= 0,
		eTacticalEntity_Item					= 1,
		eTacticalEntity_Unit					= 2,
		eTacticalEntity_Ammo					= 3,
		eTacticalEntity_Custom				= 4,
		eTacticalEntity_Objectives		= 5,
		eTacticalEntity_Prompt				= 6,
		eTacticalEntity_Vehicle				= 7,
		eTacticalEntity_Hazard				= 8,
		eTacticalEntity_Explosive			= 9,
		eTacticalEntity_MapIcon				= 10,
		eTacticalEntity_Last					= 11
	};

	enum EScanningCount
	{
		eScanned_None,
		eScanned_Once,
		eScanned_Max = eScanned_Once
	};

	typedef uint8 TScanningCount;

	struct STacticalInterestPoint
	{
		STacticalInterestPoint()
		{
		}

		STacticalInterestPoint(const EntityId id)
			: m_entityId(id)
			, m_scanned(eScanned_None)
			, m_overrideIconType(eIconType_NumIcons)
			, m_tagged(false)
			, m_visible(false)
			, m_pinged(false)
		{

		}

		void Reset();
		void Serialize(TSerialize ser);

		EntityId				m_entityId;
		TScanningCount	m_scanned;
		uint8						m_overrideIconType;
		bool						m_tagged;
		bool						m_visible;
		bool						m_pinged;
	};

	typedef std::vector<STacticalInterestPoint> TInterestPoints;
	typedef std::map<IEntityClass*, TScanningCount> TInterestClasses;
	typedef CryFixedArray<TInterestPoints, eTacticalEntity_Last> TAllTacticalPoints;
	typedef std::map<EntityId, EntityId> TTacticalEntityToOverrideEntities;
public:

														CTacticalManager();
	virtual										~CTacticalManager();

	void											Init();
	void											Reset();
	void											Serialize(TSerialize ser);
	void											PostSerialize();

	void											AddEntity(const EntityId id, ETacticalEntityType type);
	void											RemoveEntity(const EntityId id, ETacticalEntityType type);
	void											SetEntityScanned(const EntityId id);
	void											SetEntityScanned(const EntityId id, ETacticalEntityType type);
	TScanningCount						GetEntityScanned(const EntityId id) const;
	void											SetEntityTagged(const EntityId id, const bool bTagged);
	void											SetEntityTagged(const EntityId id, ETacticalEntityType type, const bool bTagged);
	bool											IsEntityTagged(const EntityId id) const;

	void											SetClassScanned(IEntityClass* pClass, const TScanningCount scanned);
	TScanningCount						GetClassScanned(IEntityClass* pClass) const;

	void											ClearAllTacticalPoints();
	void											ResetAllTacticalPoints();
	void											ResetClassScanningData();

	TAllTacticalPoints&				GetAllTacticalPoints() { return m_allTacticalPoints; }
	const TAllTacticalPoints& GetAllTacticalPoints() const { return m_allTacticalPoints; }
	TInterestPoints&					GetTacticalPoints(const ETacticalEntityType type);
	const TInterestPoints&		GetTacticalPoints(const ETacticalEntityType type) const;

	void											Ping(const ETacticalEntityType type, const float fPingDistance);
	void											Ping(const float fPingDistance);

	void											AddOverrideEntity(const EntityId tacticalEntity, const EntityId overrideEntity);
	void											RemoveOverrideEntity(const EntityId tacticalEntity, const EntityId overrideEntity);
	void											RemoveOverrideEntity(const EntityId overrideEntity);
	EntityId									GetOverrideEntity(const EntityId tacticalEntity) const;

	const Vec3&								GetTacticalIconWorldPos(const EntityId tacticalEntityId, IEntity* pTacticalEntity, bool& inOutIsHeadBone); // Processes in order: Override entity position if exists -> head bone position if exists -> center of bounding box

	void											SetEntityOverrideIcon(const EntityId id, const uint8 overrideIconType);
	uint8											GetEntityOverrideIcon(const EntityId id) const;
	uint8											GetOverrideIconType(const char* szOverrideIconName) const;

private:
	const Vec3&								GetTacticalIconCenterBBoxWorldPos(IEntity* pTacticalEntity);
	void											AddTacticalInfoPointData(const ETacticalEntityType type, const STacticalInterestPoint& point);

	TAllTacticalPoints m_allTacticalPoints;
	TInterestClasses	 m_classes;
	TTacticalEntityToOverrideEntities m_tacEntityToOverrideEntities;
	Vec3							 m_tempVec3; // To be more efficient with GetTacticalIconWorldPos
};


//////////////////////////////////////////////////////////////////////////

#endif

