// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LOGFILE_H__
#define __LOGFILE_H__

#include "ILogger.h"

class LogFile
	: public ILogger
{
public:
	explicit LogFile(const char* filename);
	~LogFile();

	bool IsOpen() const;
	bool HasWarningsOrErrors() const;

	// ILogger
	virtual void LogImpl(ESeverity eSeverity, const char* message);

private:
	std::FILE* m_file;
	bool m_hasWarnings;
	bool m_hasErrors;
};

#endif //__LOGFILE_H__
