// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

//! Used to collect the assets needed for streaming and to gather statistics.
struct IResourceCollector
{
	// <interfuscator:shuffle>
	//! \param dwMemSize 0xffffffff if size is unknown.
	//! \return true if new resource was added, false if resource was already registered
	virtual bool AddResource(const char* szFileName, const uint32 dwMemSize) = 0;

	//! \param szFileName Needs to be registered before with AddResource().
	//! \param pInstance Must not be 0.
	virtual void AddInstance(const char* szFileName, void* pInstance) = 0;

	//! \param szFileName Needs to be registered before with AddResource().
	virtual void OpenDependencies(const char* szFileName) = 0;

	virtual void CloseDependencies() = 0;

	//! Resets the internal data structure for the resource collector.
	virtual void Reset() = 0;
	// </interfuscator:shuffle>
protected:
	virtual ~IResourceCollector() {}
};

class NullResCollector : public IResourceCollector
{
public:
	virtual bool AddResource(const char* szFileName, const uint32 dwMemSize) { return true; }
	virtual void AddInstance(const char* szFileName, void* pInstance)        {}
	virtual void OpenDependencies(const char* szFileName)                    {}
	virtual void CloseDependencies()                                         {}
	virtual void Reset()                                                     {}

	virtual ~NullResCollector() {}
};

//! \endcond