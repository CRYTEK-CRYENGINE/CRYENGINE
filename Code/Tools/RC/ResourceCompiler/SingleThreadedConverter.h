// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IConverter.h"

class CSingleThreadedCompiler : public ICompiler, public IConverter
{
public:
	CSingleThreadedCompiler() : m_refCount(1) {}
	virtual ~CSingleThreadedCompiler() {}

	// Inherited via ICompiler
	virtual IConvertContext* GetConvertContext() override final { return &m_CC; }

	// Inherited via IConverter
	virtual ICompiler* CreateCompiler() override final
	{
		// Only ever return one compiler, since we don't support multithreading. Since
		// the compiler is just this object, we can tell whether we have already returned
		// a compiler by checking the ref count.
		if (m_refCount >= 2)
		{
			return 0;
		}

		// Until we support multithreading for this converter, the compiler and the
		// converter may as well just be the same object.
		++m_refCount;
		return this;
	}

	virtual bool SupportsMultithreading() const override final { return false; }

	// Inherited via ICompiler and IConverter
	virtual void Release() override final
	{
		if (--m_refCount <= 0)
		{
			delete this;
		}
	}

protected:
	ConvertContext m_CC;

private:
	int m_refCount;
};