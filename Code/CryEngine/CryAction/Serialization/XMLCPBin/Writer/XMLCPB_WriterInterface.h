// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once

#ifndef XMLCPB_WRITERINTERFACE_H
	#define XMLCPB_WRITERINTERFACE_H

	#include "XMLCPB_Writer.h"

namespace XMLCPB {

// this class is just to give a clear access to the intended interface functions and only them, without using a bunch of friend declarations
// CWriter should not be used directly from outside
class CWriterInterface
{
public:

	void               Init(const char* pRootName, const char* pFileName)  { m_writer.Init(pRootName, pFileName); }
	CNodeLiveWriterRef GetRoot()                                           { return m_writer.GetRoot(); }
	void               Done()                                              { m_writer.Done(); } // should be called when everything is added and finished.
	bool               FinishWritingFile()                                 { return m_writer.FinishWritingFile(); }
	bool               WriteAllIntoMemory(uint8*& rpData, uint32& outSize) { return m_writer.WriteAllIntoMemory(rpData, outSize); }

private:

	CWriter m_writer;
};

}  // end namespace

#endif
