// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "NativeToManagedInterfaces/IMonoNativeToManagedInterface.h"

class CAudioInterface final : public IMonoNativeToManagedInterface
{
	//change class name
	virtual const char* GetClassName() const override { return "IAudioSystem"; }
	virtual void RegisterFunctions(std::function<void(const void* pMethod, const char* methodName)> func) override;
};