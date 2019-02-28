// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XMLSCRIPTLOADER_H__
#define __XMLSCRIPTLOADER_H__

#pragma once

SmartScriptTable XmlScriptLoad(const char* definitionFile, const char* dataFile);
SmartScriptTable XmlScriptLoad(const char* definitionFile, XmlNodeRef data);
XmlNodeRef       XmlScriptSave(const char* definitionFile, SmartScriptTable scriptTable);
bool             XmlScriptSave(const char* definitionFile, const char* dataFile, SmartScriptTable scriptTable);

#endif
