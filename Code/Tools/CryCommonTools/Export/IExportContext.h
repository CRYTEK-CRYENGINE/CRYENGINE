// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IEXPORTCONTEXT_H__
#define __IEXPORTCONTEXT_H__

#include <cstdarg>
#include "Exceptions.h"
#include "ILogger.h"

struct IPakSystem;
class ISettings;

class IExportContext
	: public ILogger
{
public:
	// Declare an exception type to report the case where the scene must be saved before exporting.
	struct NeedSaveErrorTag {};
	typedef Exception<NeedSaveErrorTag> NeedSaveError;
	struct PakSystemErrorTag {};
	typedef Exception<PakSystemErrorTag> PakSystemError;

	virtual void SetProgress(float progress) = 0;
	virtual void SetCurrentTask(const std::string& id) = 0;
	virtual IPakSystem* GetPakSystem() = 0;
	virtual ISettings* GetSettings() = 0;
	virtual void GetRootPath(char* buffer, int bufferSizeInBytes) = 0;

protected:
	// ILogger
	virtual void LogImpl(ILogger::ESeverity eSeverity, const char* message) = 0;
};

struct CurrentTaskScope
{
	CurrentTaskScope(IExportContext* context, const std::string& id): context(context) {context->SetCurrentTask(id);}
	~CurrentTaskScope() {context->SetCurrentTask("");}
	IExportContext* context;
};

#endif //__IEXPORTCONTEXT_H__
