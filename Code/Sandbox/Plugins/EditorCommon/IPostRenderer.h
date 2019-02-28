// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IPOSTRENDERER_H__
#define __IPOSTRENDERER_H__

#pragma once

class IPostRenderer
{
public:
	IPostRenderer() : m_refCount(0){}

	virtual void OnPostRender() const = 0;

	void         AddRef()  { m_refCount++; }
	void         Release() { if (--m_refCount <= 0) delete this; }

protected:
	virtual ~IPostRenderer(){}

	int m_refCount;
};

#endif//__IPOSTRENDERER_H__

