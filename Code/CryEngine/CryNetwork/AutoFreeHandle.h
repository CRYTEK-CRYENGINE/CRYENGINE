// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __AUTOFREEHANDLE_H__
#define __AUTOFREEHANDLE_H__

#pragma once

class CAutoFreeHandle
{
public:
	CAutoFreeHandle(TMemHdl& hdl) : m_hdl(hdl) {}
	~CAutoFreeHandle()
	{
		MMM().FreeHdl(m_hdl);
		m_hdl = CMementoMemoryManager::InvalidHdl;
	}

	TMemHdl Grab()
	{
		TMemHdl out = m_hdl;
		m_hdl = CMementoMemoryManager::InvalidHdl;
		return out;
	}

	TMemHdl Peek()
	{
		return m_hdl;
	}

private:
	TMemHdl& m_hdl;
};

#endif
