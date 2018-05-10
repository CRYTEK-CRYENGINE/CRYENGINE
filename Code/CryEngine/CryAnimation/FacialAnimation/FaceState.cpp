// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FaceState.h"

//////////////////////////////////////////////////////////////////////////
void CFaceState::SetNumWeights(int n)
{
	m_weights.resize(n);
	m_balance.resize(n);
	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CFaceState::Reset()
{
	if (!m_weights.empty())
		memset(&m_weights[0], 0, sizeof(float) * m_weights.size());
	if (!m_balance.empty())
		memset(&m_balance[0], 0, sizeof(float) * m_balance.size());
}
