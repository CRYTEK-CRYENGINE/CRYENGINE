#pragma once

#include <NativeToManagedInterfaces/IMonoNativeToManagedInterface.h>

class CManagedEntityInterface final : public IMonoNativeToManagedInterface
{
	// IMonoNativeToManagedInterface
	virtual const char* GetClassName() const override { return "Entity"; }
	virtual void RegisterFunctions(std::function<void(const void* pMethod, const char* methodName)> func) override;
	// ~IMonoNativeToManagedInterface
};