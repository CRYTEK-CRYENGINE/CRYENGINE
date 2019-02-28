// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   OcclQuery.h : Occlusion queries unified interface

   Revision history:
* Created by Tiago Sousa
* Todo: replace flares/CREOcclusionQuery with common queries

   =============================================================================*/

#ifndef __OCCLQUERY_H__
#define __OCCLQUERY_H__

class COcclusionQuery
{
public:
	COcclusionQuery() : m_nVisSamples(~0), m_nCheckFrame(0), m_nDrawFrame(0), m_nOcclusionID(0)
	{
	}

	~COcclusionQuery()
	{
		Release();
	}

	void   Create();
	void   Release();

	void   BeginQuery();
	void   EndQuery();

	uint32 GetVisibleSamples(bool bAsynchronous);

	int    GetDrawFrame() const
	{
		return m_nDrawFrame;
	}

	bool IsReady();

private:

	int      m_nVisSamples;
	int      m_nCheckFrame;
	int      m_nDrawFrame;

	UINT_PTR m_nOcclusionID; // This will carry a pointer D3DOcclusionQuery, so it needs to be 64-bit on Windows 64
};

#endif
