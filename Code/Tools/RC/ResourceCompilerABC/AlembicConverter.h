// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   AlembicConverter.h
//  Created:     20/7/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IConverter.h"

class ICryXML;

class AlembicConverter : public IConverter
{
public:
	AlembicConverter(ICryXML *pXMLParser, IPakSystem* pPakSystem);
	virtual ~AlembicConverter();

	// IConverter methods.
	virtual void Release();
	virtual void DeInit() {}
	virtual ICompiler *CreateCompiler();
	virtual bool SupportsMultithreading() const;
	virtual const char *GetExt(int index) const;

private:
	int m_refCount;
	ICryXML *m_pXMLParser;
	IPakSystem *m_pPakSystem;
};