// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  console variables list processor
   -------------------------------------------------------------------------
   History:
   - 05/01/2007   00:31 : Created by Stas Spivakov
*************************************************************************/

#ifndef __CVARLISTPROCESSOR_H__
#define __CVARLISTPROCESSOR_H__

struct ICVar;

struct ICVarListProcessorCallback
{
	virtual ~ICVarListProcessorCallback(){}
	virtual void OnCVar(ICVar*) = 0;
};

class CCVarListProcessor
{
public:
	CCVarListProcessor(const char* path);
	~CCVarListProcessor();

	void Process(ICVarListProcessorCallback* cb);
private:
	string m_fileName;
};

#endif
