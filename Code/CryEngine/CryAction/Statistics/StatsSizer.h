// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   File name:		StatsSizer.h
   Version:			v1.00
   Created:			23/10/2009 by Sergey Mikhtonyuk
   Description:  Sizer implementation that used by game statistics
   -------------------------------------------------------------------------
   History:
*************************************************************************/
#ifndef   _STATSSIZER_H__
#define   _STATSSIZER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

class CStatsSizer : public ICrySizer
{
public:
	CStatsSizer();
	virtual void                Release();
	virtual size_t              GetTotalSize();
	virtual size_t              GetObjectCount();
	virtual bool                AddObject(const void* pIdentifier, size_t nSizeBytes, int nCount);
	virtual IResourceCollector* GetResourceCollector();
	virtual void                SetResourceCollector(IResourceCollector* pColl);
	virtual void                Push(const char* szComponentName);
	virtual void                PushSubcomponent(const char* szSubcomponentName);
	virtual void                Pop();
	virtual void                Reset();
	virtual void                End();

private:
	size_t m_count, m_size;
};

#endif // __STATSSIZER_H__
