// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   AlembicConverter.cpp
//  Created:     20/7/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AlembicConverter.h"
#include "AlembicCompiler.h"

AlembicConverter::AlembicConverter(ICryXML *pXMLParser, IPakSystem* pPakSystem)
	: m_refCount(1), m_pXMLParser(pXMLParser), m_pPakSystem(pPakSystem)
{
}

AlembicConverter::~AlembicConverter()
{
}

void AlembicConverter::Release()
{
	if (--m_refCount <= 0)
	{
		delete this;
	}
}

ICompiler *AlembicConverter::CreateCompiler()
{
	return new AlembicCompiler(m_pXMLParser);
}

bool AlembicConverter::SupportsMultithreading() const
{
	return false;
}

const char *AlembicConverter::GetExt(int index) const
{
	switch (index)
	{
	case 0:
		return "abc";
	default:
		return 0;
	}
}