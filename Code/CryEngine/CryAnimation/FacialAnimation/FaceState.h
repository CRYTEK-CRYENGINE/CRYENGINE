// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IFacialAnimation.h>
#include "FaceEffector.h"

class CFacialModel;

//////////////////////////////////////////////////////////////////////////
// FacialState represents full state of all end effectors.
//////////////////////////////////////////////////////////////////////////
class CFaceState : public IFaceState, public _i_reference_target_t
{
public:
	CFaceState(CFacialModel* pFacialModel) { m_pFacialModel = pFacialModel; };

	//////////////////////////////////////////////////////////////////////////
	// IFaceState
	//////////////////////////////////////////////////////////////////////////
	virtual float GetEffectorWeight(int nIndex)
	{
		if (nIndex >= 0 && nIndex < (int)m_weights.size())
			return GetWeight(nIndex);
		return 0;
	}
	virtual void SetEffectorWeight(int nIndex, float fWeight)
	{
		if (nIndex >= 0 && nIndex < (int)m_weights.size())
			SetWeight(nIndex, fWeight);
	}
	//////////////////////////////////////////////////////////////////////////

	CFacialModel* GetModel() const { return m_pFacialModel; };

	float         GetWeight(int nIndex) const
	{
		assert(nIndex >= 0 && nIndex < (int)m_weights.size());
		return m_weights[nIndex];
	}
	void SetWeight(int nIndex, float fWeight)
	{
		assert(nIndex >= 0 && nIndex < (int)m_weights.size());
		m_weights[nIndex] = fWeight;
	}
	void SetWeight(IFacialEffector* pEffector, float fWeight)
	{
		int nIndex = ((CFacialEffector*)pEffector)->m_nIndexInState;
		if (nIndex >= 0 && nIndex < (int)m_weights.size())
			m_weights[nIndex] = fWeight;
	}
	float GetBalance(int nIndex) const
	{
		assert(nIndex >= 0 && nIndex < (int)m_balance.size());
		return m_balance[nIndex];
	}
	void SetBalance(int nIndex, float fBalance)
	{
		assert(nIndex >= 0 && nIndex < (int)m_balance.size());
		m_balance[nIndex] = fBalance;
	}
	void   Reset();

	void   SetNumWeights(int n);
	int    GetNumWeights() { return m_weights.size(); };

	size_t SizeOfThis()
	{
		return sizeofVector(m_weights) * sizeof(float) + sizeofVector(m_balance);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_weights);
		pSizer->AddObject(m_balance);
	}

public:
	// Weights of the effectors in state.
	std::vector<float> m_weights;
	// Balances of the effectors in state.
	std::vector<float> m_balance;
	CFacialModel*      m_pFacialModel;
};
