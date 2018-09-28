// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryNetwork/SerializeFwd.h>
#include <CrySystem/TimeValue.h>

struct IAIObject;
struct IAIActor;

class CPerceptionActor
{
public:
	CPerceptionActor();
	CPerceptionActor(IAIActor* pAIActor);

	void Update(const CTimeValue& frameDelta);
	void Reset();

	void Serialize(TSerialize ser);

	void Enable(bool enable);
	bool IsEnabled() const;

	void CheckCloseContact(IAIObject* pTarget);
	bool CloseContactEnabled() { return m_closeContactTimeOut == 0; }

	void SetMeleeRange(float meleeRange) { m_meleeRange = meleeRange; }

private:
	IAIActor* m_pAIActor;

	int m_perceptionDisabled;

	float m_meleeRange;
	CTimeValue m_closeContactTimeOut; // used to prevent the signal OnCloseContact being sent repeatedly to same object
};