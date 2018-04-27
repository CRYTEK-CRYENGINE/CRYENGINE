// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(__AIFLYING_VEHICLE__)
#define __AIFLYING_VEHICLE__

#include "Puppet.h"

class CAIFlyingVehicle : public CPuppet
{
	typedef CPuppet Base;

public:

	CAIFlyingVehicle();
	virtual ~CAIFlyingVehicle();

	void        SetObserver(bool observer);
	static void OnVisionChanged(const VisionID& observerID,
	                            const ObserverParams& observerParams,
	                            const VisionID& observableID,
	                            const ObservableParams& observableParams,
	                            bool visible);

	//virtual void  Reset               (EObjectResetType type);

	virtual void Serialize(TSerialize ser);
	virtual void PostSerialize();
	virtual void SetSignal(int nSignalID, const char* szText, IEntity* pSender = 0, IAISignalExtraData* pData = NULL, uint32 crcCode = 0);

private:
	bool m_combatModeEnabled;
	bool m_firingAllowed;
};

#endif //#if !defined(__AIFLYING_VEHICLE__)
