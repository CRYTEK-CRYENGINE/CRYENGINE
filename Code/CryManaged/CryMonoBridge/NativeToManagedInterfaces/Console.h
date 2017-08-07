#pragma once

#include <NativeToManagedInterfaces/IMonoNativeToManagedInterface.h>

class CConsoleCommandInterface final : public IMonoNativeToManagedInterface
{
	//change class name
	virtual const char* GetClassName() const override { return "IConsole"; }
	virtual void RegisterFunctions(std::function<void(const void* pMethod, const char* methodName)> func) override;
};
