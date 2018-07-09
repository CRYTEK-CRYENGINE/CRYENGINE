// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COLLADAWRITER_H__
#define __COLLADAWRITER_H__

#include <string>

class IExportSource;
class IExportContext;
class ProgressRange;
class IXMLSink;

class ColladaWriter
{
public:
	static bool Write(IExportSource* source, IExportContext* context, IXMLSink* sink, ProgressRange& progressRange);
};

#endif //__COLLADAWRITER_H__
