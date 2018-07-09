// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEJOB__
#define __CRYSIMPLEJOB__

#include <vector>

class TiXmlDocument;

enum ECrySimpleJobState
{
	ECSJS_NONE,
	ECSJS_DONE = 1,                              //this is checked on client side, don't change!
	ECSJS_JOBNOTFOUND,
	ECSJS_CACHEHIT,
	ECSJS_ERROR,
	ECSJS_ERROR_COMPILE = 5,                     //this is checked on client side, don't change!
	ECSJS_ERROR_COMPRESS,
	ECSJS_ERROR_FILEIO,
	ECSJS_ERROR_INVALID_PROFILE,
	ECSJS_ERROR_INVALID_PLATFORM,
	ECSJS_ERROR_INVALID_PROGRAM,
	ECSJS_ERROR_INVALID_ENTRY,
	ECSJS_ERROR_INVALID_COMPILEFLAGS,
	ECSJS_ERROR_INVALID_SHADERREQUESTLINE,
};

class CCrySimpleJob
{
	ECrySimpleJobState     m_State;
	uint32_t               m_RequestIP;
	static volatile long   m_GlobalRequestNumber;

protected:
	virtual bool           ExecuteCommand(const std::string& rCmd, const std::string& args, std::string &outError);
public:
	CCrySimpleJob(uint32_t requestIP);
	virtual ~CCrySimpleJob();

	virtual bool           Execute(const TiXmlElement* pElement) = 0;

	void                   State(ECrySimpleJobState S) { if (m_State < ECSJS_ERROR || S >= ECSJS_ERROR) m_State = S; }
	ECrySimpleJobState     State() const { return m_State; }
	const uint32_t&        RequestIP() const { return m_RequestIP; }
	static volatile long   GlobalRequestNumber() { return m_GlobalRequestNumber; }
};

#endif
