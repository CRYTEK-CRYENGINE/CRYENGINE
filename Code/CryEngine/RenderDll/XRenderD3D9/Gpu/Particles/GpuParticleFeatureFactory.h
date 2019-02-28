// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     09/04/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GpuParticleFeatureBase.h"

namespace gpu_pfx2
{

template<class T>
CFeature* CreateInterfaceFunction() { return new T(); }

class CGpuInterfaceFactory
{
private:
	typedef CFeature* (* FactoryFuncPtr)();
	FactoryFuncPtr m_functions[eGpuFeatureType_COUNT];

public:
	CFeature* CreateInstance(EGpuFeatureType type)
	{
		return m_functions[static_cast<size_t>(type)]();
	}
	template<class T> void RegisterClass()
	{
		m_functions[static_cast<size_t>(T::type)] = &CreateInterfaceFunction<T>;
	}
	CGpuInterfaceFactory();
};
}
