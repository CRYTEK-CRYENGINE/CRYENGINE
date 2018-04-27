// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  provide a simple text based input stream in the spirit of
               iostreams
   -------------------------------------------------------------------------
   History:
   - 13/08/2004   09:29 : Created by Craig Tiller
*************************************************************************/
#ifndef __SIMPLEINPUTSTREAM_H__
#define __SIMPLEINPUTSTREAM_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include "SimpleStreamDefs.h"

class CSimpleInputStream
{
public:
	CSimpleInputStream(size_t size);
	virtual ~CSimpleInputStream();

	void                 ForceEof() { m_eof = true; }
	const SStreamRecord* Next(bool peek = false);

	void                 GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CSimpleInputStream");

		pSizer->Add(*this);
		if (m_records)
			pSizer->Add(m_records, m_maxRecords * sizeof(SStreamRecord));
	}

private:
	virtual void Underflow(SStreamRecord* pStream, size_t& count) = 0;

	size_t         m_maxRecords;
	size_t         m_numRecords;
	size_t         m_curRecord;
	bool           m_eof;
	SStreamRecord* m_records;
};

#endif
