// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIHUD3D.h
//  Version:     v1.00
//  Created:     22/11/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UIHUD3D_H__
#define __UIHUD3D_H__

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>

class CUIHUD3D
	: public IUIGameEventSystem
	, public ISystemEventListener
	, public IEntityEventListener
	, public IViewSystemListener
	, public IUIModule
{
public:
	CUIHUD3D();
	~CUIHUD3D();
	// IUIGameEventSystem
	UIEVENTSYSTEM( "UIHUD3D" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;
	virtual void UpdateView( const SViewParams &viewParams ) override;

	// ISystemEventListener
	virtual void OnSystemEvent( ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam ) override;
	// ~ISystemEventListener

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event ) override;
	// ~IEntityEventListener

	// IViewSystemListener
	virtual bool OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX) override { return true; }
	virtual bool OnEndCutScene(IAnimSequence* pSeq) override { return true; }
	virtual bool OnCameraChange(const SCameraParams& cameraParams) override;
	// ~IViewSystemListener

	// IUIModule
	void Reload() override;
	void Update(float fDeltaTime);
	// ~IUIModule

	void SetVisible(bool visible);
	bool IsVisible() const;

private:
	void SpawnHudEntities();
	void RemoveHudEntities();


	static void OnVisCVarChange( ICVar * );

private:
	IEntity* m_pHUDRootEntity;
	EntityId m_HUDRootEntityId;
	typedef std::vector< EntityId > THUDEntityList;
	THUDEntityList m_HUDEnties;

	struct SHudOffset
	{
		SHudOffset(float aspect, float dist, float offset)
			: Aspect(aspect)
			, HudDist(dist)
			, HudZOffset(offset)
		{
		}

		float Aspect;
		float HudDist;
		float HudZOffset;
	};
	typedef std::vector< SHudOffset > THudOffset;
	THudOffset m_Offsets;

	float m_fHudDist;
	float m_fHudZOffset;
	//IAnimSequence* m_pSequence;
};

#endif // __UIHUD3D_H__

