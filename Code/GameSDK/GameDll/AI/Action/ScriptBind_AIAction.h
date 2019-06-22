// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h>
#include <IViewSystem.h>

class CGameAISystem;

class CScriptBind_AIAction : public CScriptableBase
{
public:
	CScriptBind_AIAction(CGameAISystem* pGameAISystem);
	virtual ~CScriptBind_AIAction() 
	{
		m_sGlobalName[0] = 0;
	}

	virtual void Init(IScriptSystem* pSS, ISystem* pSystem, int nParamIdOffset = 0)
	{
		m_pSS = pSS;
		m_pMethodsTable = gEnv->pGameFramework->GetActionScriptBindTable();
		CRY_ASSERT_MESSAGE(m_pMethodsTable != nullptr, "Failed to retrieve CryAction script table!");
		if (m_pMethodsTable)
		{
			m_pMethodsTable->AddRef();
		}
		m_nParamIdOffset = nParamIdOffset;
	}

	void Release() { delete this; };

	void SetGlobalName(const char* sGlobalName)
	{
		CRY_ASSERT(strlen(sGlobalName) < sizeof(m_sGlobalName));
		std::strcpy(m_sGlobalName, sGlobalName);
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:
	//! <code>Action.EnableSignalTimer( entityId, sText )</code>
	//!		<param name="entityId">Identifier for the entity.
	//!		<param name="sText">Text for the signal.</param>
	//! <description>Enables the signal timer.</description>
	int EnableSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText);

	//! <code>Action.DisableSignalTimer( entityId, sText )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="sText">Text for the signal.</param>
	//! <description>Disables the signal timer.</description>
	int DisableSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText);

	//! <code>Action.SetSignalTimerRate( entityId, sText, fRateMin, fRateMax )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="sText">Text for the signal.</param>
	//!		<param name="fRateMin">Minimum rate for the signal timer.</param>
	//!		<param name="fRateMax">Maximum rate for the signal timer.</param>
	//! <description>Sets the rate for the signal timer.</description>
	int SetSignalTimerRate(IFunctionHandler* pH, ScriptHandle entityId, const char* sText, float fRateMin, float fRateMax);

	//! <code>Action.ResetSignalTimer( entityId, sText )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="sText">Text for the signal.</param>
	//! <description>Resets the rate for the signal timer.</description>
	int ResetSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText);

	//! <code>Action.EnableRangeSignaling( entityId, bEnable )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="bEnable">Enable/Disable range signalling.</param>
	//! <description>Enable/Disable range signalling for the specified entity.</description>
	int EnableRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId, bool bEnable);

	//! <code>Action.DestroyRangeSignaling( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	int DestroyRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.ResetRangeSignaling( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	int ResetRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.AddRangeSignal( entityId, fRadius, fFlexibleBoundary, sSignal )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="fRadius">Radius of the range area.</param>
	//!		<param name="fFlexibleBoundary">Flexible boundary size.</param>
	//!		<param name="sSignal">String for signal.</param>
	//! <description>Adds a range for the signal.</description>
	int AddRangeSignal(IFunctionHandler* pH, ScriptHandle entityId, float fRadius, float fFlexibleBoundary, const char* sSignal);

	//! <code>Action.AddTargetRangeSignal( entityId, targetId, fRadius, fFlexibleBoundary, sSignal )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="targetId">Identifier for the target.</param>
	//!		<param name="fRadius">Radius of the range area.</param>
	//!		<param name="fFlexibleBoundary">Flexible boundary size.</param>
	//!		<param name="sSignal">String for signal.</param>
	int AddTargetRangeSignal(IFunctionHandler* pH, ScriptHandle entityId, ScriptHandle targetId, float fRadius, float fFlexibleBoundary, const char* sSignal);

	//! <code>Action.AddRangeSignal( entityId, fAngle, fFlexibleBoundary, sSignal )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="fAngle">Angle value.</param>
	//!		<param name="fFlexibleBoundary">Flexible boundary size.</param>
	//!		<param name="sSignal">String for signal.</param>
	//! <description>Adds an angle for the signal.</description>
	int AddAngleSignal(IFunctionHandler* pH, ScriptHandle entityId, float fAngle, float fFlexibleBoundary, const char* sSignal);

	//! <code>Action.SetAimQueryMode( entityId, mode )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="mode">QueryAimFromMovementController or OverriddenAndAiming or OverriddenAndNotAiming</param>
	//! <description>
	//!		Set the aim query mode for the ai proxy. Normally the ai proxy
	//!		asks the movement controller if the character is aiming.
	//!		You can override that and set your own 'isAiming' state.
	//! </description>
	int SetAimQueryMode(IFunctionHandler* pH, ScriptHandle entityId, int mode);

	//! <code>Action.RegisterWithAI()</code>
	//! <description>Registers the entity to AI System, creating an AI object associated to it.</description>
	int RegisterWithAI(IFunctionHandler* pH);

	CGameAISystem* m_pGameAISystem;
};
