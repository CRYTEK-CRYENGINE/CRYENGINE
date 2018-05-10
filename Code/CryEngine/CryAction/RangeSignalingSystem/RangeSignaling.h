// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   RangeSignaling.h
   $Id$
   $DateTime$
   Description: Signal entities based on ranges from other entities
   ---------------------------------------------------------------------
   History:
   - 09:04:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#ifndef _RANGE_SIGNALING_H_
#define _RANGE_SIGNALING_H_

#include "IRangeSignaling.h"

class CPersonalRangeSignaling;

class CRangeSignaling : public IRangeSignaling
{

public:

	/*$1- Singleton Stuff ----------------------------------------------------*/
	static CRangeSignaling& ref();
	static bool             Create();
	static void             Shutdown();
	void                    Reset();

	/*$1- IEditorSetGameModeListener -----------------------------------------*/
	void OnEditorSetGameMode(bool bGameMode);
	void OnProxyReset(EntityId IdEntity);

	/*$1- Basics -------------------------------------------------------------*/
	void Init();
	bool Update(float fElapsedTime);

	/*$1- Utils --------------------------------------------------------------*/
	bool AddRangeSignal(EntityId IdEntity, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);
	bool AddTargetRangeSignal(EntityId IdEntity, EntityId IdTarget, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);
	bool AddAngleSignal(EntityId IdEntity, float fAngle, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);
	bool DestroyPersonalRangeSignaling(EntityId IdEntity);
	void ResetPersonalRangeSignaling(EntityId IdEntity);
	void EnablePersonalRangeSignaling(EntityId IdEntity, bool bEnable);
	void SetDebug(bool bDebug);
	bool GetDebug() const;

protected:

	/*$1- Creation and destruction via singleton -----------------------------*/
	CRangeSignaling();
	virtual ~CRangeSignaling();

	/*$1- Utils --------------------------------------------------------------*/
	CPersonalRangeSignaling* GetPersonalRangeSignaling(EntityId IdEntity) const;
	CPersonalRangeSignaling* CreatePersonalRangeSignaling(EntityId IdEntity);

private:
	typedef std::map<EntityId, CPersonalRangeSignaling*> MapPersonals;

	/*$1- Members ------------------------------------------------------------*/
	bool                    m_bInit;
	bool                    m_bDebug;
	static CRangeSignaling* m_pInstance;
	MapPersonals            m_Personals;
};
#endif // _RANGE_SIGNALING_H_
