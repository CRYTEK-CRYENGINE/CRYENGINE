// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Interface for exposing native functions to managed code manually
struct IMonoNativeToManagedInterface
{
	virtual ~IMonoNativeToManagedInterface() {}

	// The managed namespace in which the managed representation of this interface resides in.
	virtual const char* GetNamespace() const { return "CryEngine.NativeInternals"; }

	// The managed interface name we are tied to.
	virtual const char* GetClassName() const = 0;

	// Called when it is time to register all callbacks
	virtual void RegisterFunctions(std::function<void(const void* pMethod, const char* methodName)> func) = 0;
};