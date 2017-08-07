// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     29/01/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

namespace pfx2
{

template<typename T>
ILINE T* CParticleComponentRuntime::GetSubInstanceData(size_t instanceId, TInstanceDataOffset offset)
{
	const SComponentParams& params = GetComponentParams();
	CRY_PFX2_ASSERT(offset < params.m_instanceDataStride);            // very likely to be an invalid offset
	CRY_PFX2_ASSERT(instanceId < m_subInstances.size());
	byte* pBytes = m_subInstanceData.data();
	pBytes += params.m_instanceDataStride * instanceId + offset;
	return reinterpret_cast<T*>(pBytes);
}

}
