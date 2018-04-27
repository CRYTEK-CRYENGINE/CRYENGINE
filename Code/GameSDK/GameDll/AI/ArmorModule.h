// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: GameAIModule for assisting an AI using defense (armor) mode
  
 -------------------------------------------------------------------------
  History:
  - 26:04:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __ARMORMODULE_H__
#define __ARMORMODULE_H__

#include <unordered_map>
#include "GameAISystem.h"

struct HitInfo;
struct IAttachmentManager;

class ArmorModule : public IGameAIModule
{
public:
	ArmorModule();

	void LoadParamsXml();

	virtual void EntityEnter(EntityId entityID);
	virtual void EntityLeave(EntityId entityID);
	virtual void EntityPause(EntityId entityID);
	virtual void EntityResume(EntityId entityID);
	virtual void Reset(bool bUnload);
	virtual void Update(float dt);
	virtual const char* GetName() const { return "ArmorModule"; }
	virtual bool HandleResumeAll() const { return false; }
	
	void SetBlockedMode(EntityId entityID, bool bBlocked);
	void ResumeDelayed(EntityId entityID, float fDelay);

	bool IsShieldActive(EntityId entityID) const;

	// Handle damage received by entity to update armor logic
	// In: fDamage - Damage received, for updating spore shield strength
	// Returns TRUE if the spore shield absorbed this damage
	bool HandleHit(const HitInfo &hitInfo);

#ifndef _RELEASE
	static void	CmdArmorModuleReload(IConsoleCmdArgs *pArgs);
#endif //_RELEASE

private:
	struct SShaderParamEntry
	{
		string name;
		UParamVal currValue;
		float peak;
		float decay;

		SShaderParamEntry() : peak(0.f), decay(0.f) {}
	};
	
	typedef std::tr1::unordered_map<uint32, SShaderParamEntry> TShaderParams;

	struct SShaderParamEntries
	{
		TShaderParams NormalMode;
		TShaderParams FailingMode;

		TShaderParams PowerUpModifiers;
		TShaderParams HitReactionModifiers;
		TShaderParams *pModifierValues;

		SShaderParamEntries() : pModifierValues() {}
	};

	struct SActiveParams
	{
		enum EState
		{
			STATE_OFF,
			STATE_TRANSITION,
			STATE_BLOCKED,
			STATE_ON,
		};

		bool bBlocked;
		bool bFailing;
		EState state;
		float fShieldStrengthMax;
		float fShieldStrengthCurrent;
		float fSpeedScale;
		float fTransitionEnd;

		// Shader params
		int32 bodyShieldAttachmentId;
		SShaderParamEntries shaderParams;
		TShaderParams appliedShaderParams;

		SActiveParams();

		void BeginTransition(float fCurrTime, float fDelay)
		{
			state = STATE_TRANSITION;
			fTransitionEnd = fCurrTime + fDelay;
		}
	};

private:
	// Load params helpers
	void LoadParamsXml_WeaponInfo(XmlNodeRef &pNode);
	void LoadParamsXml_Mode(TShaderParams &outShaderParams, XmlNodeRef &pNode, const char* szDebugName);
	void LoadParamsXml_Modifiers(TShaderParams &outShaderParams, XmlNodeRef &pNode, const char* szDebugName);

	void RestartTransitionTimer(EntityId entityID, float fTimer);
	void StartEffect(EntityId entityID, SActiveParams &params, bool bForce = false);
	void KillEffect(EntityId entityID, SActiveParams &params);

	// Shader param helpers
	static void TriggerShaderParamModifiers(SActiveParams &params, TShaderParams &ApplyModifier);
	static void ApplyShaderParamModifiers(SActiveParams &params, TShaderParams &ModeParams, float dt);
	void UpdateShaderParams(EntityId entityID, SActiveParams &params, float dt) const;

	void InitActiveParams(EntityId entityID, SActiveParams &params);

	static IAttachmentManager* GetAttachmentManager(EntityId entityID);
	static void HideAttachment(EntityId entityID, const SActiveParams &params, bool bHide);

	float GetShieldDamageMultiplier(const SActiveParams &params, const HitInfo &hitInfo, bool &bOutInstantDamage);

private:
	// Global params loaded from Xml
	SShaderParamEntries globalShaderParams;
	TShaderParams m_ShaderParams_PowerUpModifiers;
	TShaderParams m_ShaderParams_HitReactionModifiers;

	string m_sAttachmentName;
	float m_fFailingPercent;

	typedef std::map<EntityId, SActiveParams> TActiveIds;
	TActiveIds m_ActiveIds;

	typedef std::map<uint16, float> TWeaponDamageMods;
	TWeaponDamageMods m_WeaponDamageMods;
};

#endif //__ARMORMODULE_H__
