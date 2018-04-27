// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COLLADAEXPORTWRITER_H__
#define __COLLADAEXPORTWRITER_H__

#include "IExportWriter.h"

class ColladaExportWriter : public IExportWriter
{
public:
	// IExportWriter
	virtual void Export(IExportSource* source, IExportContext* context);
};

#endif //__COLLADAEXPORTWRITER_H__
