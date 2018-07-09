#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

#include <CrySerialization/Decorators/Resources.h>

class CLocalGridEntity final 
	: public CDesignerEntityComponent<IEntityComponent>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CLocalGridEntity, IEntityComponent, "LocalGrid", "c7b59a1c-7d7a-4ad9-b965-c80a40467089"_cry_guid);
	virtual ~CLocalGridEntity() {}

public:
	virtual void OnResetState() final;
	virtual void ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final { return CDesignerEntityComponent::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) | ENTITY_EVENT_BIT(ENTITY_EVENT_ATTACH) | ENTITY_EVENT_BIT(ENTITY_EVENT_DETACH); }
	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }
	virtual const char* GetLabel() const override { return "Grid Properties"; }

	enum EGridDim
	{
		eDim_1    = 1,
		eDim_2    = 2,
		eDim_4    = 4,
		eDim_8    = 8,
		eDim_16   = 16,
		eDim_32   = 32,
		eDim_64   = 64,
		eDim_128  = 128,
		eDim_256  = 256,
		eDim_512  = 512,
		eDim_1024 = 1024
	};
	virtual void SerializeProperties(Serialization::IArchive& archive) override
	{
		archive(m_size.x, "NumCellsX", "NumCellsX");
		archive(m_size.y, "NumCellsY", "NumCellsY");
		archive(m_cellSize.x, "CellSizeX", "CellSizeX");
		archive(m_cellSize.y, "CellSizeY", "CellSizeY");
		archive(m_height, "Height", "Height");
		archive(m_accThresh, "AccelThresh", "AccelThresh");

		if (archive.isInput())
		{
			GetEntity()->SetLocalBounds(AABB(Vec3(0), Vec3(m_size.x*m_cellSize.x, m_size.y*m_cellSize.y, m_height)), true);
		}
	}

protected:
	Vec2_tpl<EGridDim> m_size = Vec2_tpl<EGridDim>(eDim_8,eDim_8);
	Vec2  m_cellSize = Vec2(1,1);
	float m_height = 2.0f;
	float m_accThresh = 3.0f;

	bool m_shouldApply = true;

	_smart_ptr<IPhysicalEntity> m_pGrid;
};
