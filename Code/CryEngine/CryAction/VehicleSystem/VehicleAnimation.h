// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 22:03:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEANIMATION_H__
#define __VEHICLEANIMATION_H__

#include <IVehicleSystem.h>

class CVehiclePartAnimated;

class CVehicleAnimation : public IVehicleAnimation
{
public:

	CVehicleAnimation();
	virtual ~CVehicleAnimation() {}

	virtual bool                Init(IVehicle* pVehicle, const CVehicleParams& table);
	virtual void                Reset();
	virtual void                Release() { delete this; }

	virtual bool                StartAnimation();
	virtual void                StopAnimation();

	virtual bool                ChangeState(TVehicleAnimStateId stateId);
	virtual TVehicleAnimStateId GetState();

	virtual string              GetStateName(TVehicleAnimStateId stateId);
	virtual TVehicleAnimStateId GetStateId(const string& name);

	virtual void                SetSpeed(float speed);

	virtual void                ToggleManualUpdate(bool isEnabled);
	virtual void                SetTime(float time, bool force = false);

	virtual float               GetAnimTime(bool raw = false);
	virtual bool                IsUsingManualUpdates();

	virtual void                GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_animationStates);
	}
protected:

	struct SAnimationStateMaterial
	{
		string material, setting;
		float  _min, _max;
		bool   invertValue;

		void   GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(material);
			pSizer->AddObject(setting);
		}
	};

	typedef std::vector<SAnimationStateMaterial> TAnimationStateMaterialVector;

	struct SAnimationState
	{
		string                        name;
		string                        animation;

		string                        sound;
		//tSoundID soundId;
		IVehicleHelper*               pSoundHelper;

		float                         speedDefault;
		float                         speedMin;
		float                         speedMax;
		bool                          isLooped;
		bool                          isLoopedEx;

		TAnimationStateMaterialVector materials;

		void                          GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(name);
			pSizer->AddObject(animation);
			pSizer->AddObject(sound);
			pSizer->AddObject(pSoundHelper);
			pSizer->AddObject(materials);
		}
	};

	typedef std::vector<SAnimationState> TAnimationStateVector;

protected:

	bool       ParseState(const CVehicleParams& table, IVehicle* pVehicle);
	IMaterial* FindMaterial(const SAnimationStateMaterial& animStateMaterial, IMaterial* pMaterial);

	CVehiclePartAnimated* m_pPartAnimated;
	int                   m_layerId;

	TAnimationStateVector m_animationStates;
	TVehicleAnimStateId   m_currentStateId;

	bool                  m_currentAnimIsWaiting;
};

#endif
