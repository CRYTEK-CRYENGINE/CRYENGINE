// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __STRINGTABLE_H__
#define __STRINGTABLE_H__

#pragma once

#include "Streams/CommStream.h"

class CArithModel;

class CStringTableRep;
typedef _smart_ptr<CStringTableRep> CStringTableRepPtr;

class CStringTable
{
public:
	CStringTable();
	~CStringTable();
	CStringTable(const CStringTable&);
	CStringTable& operator=(const CStringTable&);

	void          WriteString(CCommOutputStream& out, const string& s, CArithModel* pModel);
	void          ReadString(CCommInputStream& in, string& s, CArithModel* pModel);

private:
	CStringTableRepPtr m_pRep;
};

#endif
