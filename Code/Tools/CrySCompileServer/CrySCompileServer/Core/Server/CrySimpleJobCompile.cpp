// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#endif
#ifdef UNIX
#include <pthread.h>
#endif
#include <algorithm>
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleSock.hpp"
#include "CrySimpleJobCompile.hpp"
#include "CrySimpleFileGuard.hpp"
#include "CrySimpleServer.hpp"
#include "CrySimpleCache.hpp"
#include "ShaderList.hpp"

#define MAX_COMPILER_WAIT_TIME (60*1000)

volatile long CCrySimpleJobCompile::m_GlobalCompileTasks = 0;
volatile long CCrySimpleJobCompile::m_GlobalCompileTasksMax = 0;
volatile long CCrySimpleJobCompile::m_RemoteServerID = 0;
volatile long long CCrySimpleJobCompile::m_GlobalCompileTime = 0;

struct STimer
{
	__int64 m_freq;
	STimer()
	{
		QueryPerformanceFrequency((LARGE_INTEGER *)&m_freq);
	}
	__int64 GetTime() const
	{
		__int64 t;
		QueryPerformanceCounter((LARGE_INTEGER *)&t);
		return t;
	}

	double TimeToSeconds(__int64 t)
	{
		return ((double)t) / m_freq;
	}
};


STimer g_Timer;

CCrySimpleJobCompile::CCrySimpleJobCompile(uint32_t requestIP, EProtocolVersion Version, std::vector<uint8_t>* pRVec) :
	CCrySimpleJobCache(requestIP), m_Version(Version), m_pRVec(pRVec)
{
	InterlockedIncrement(&m_GlobalCompileTasks);
	if (m_GlobalCompileTasksMax < m_GlobalCompileTasks)
		m_GlobalCompileTasksMax = m_GlobalCompileTasks;
}

CCrySimpleJobCompile::~CCrySimpleJobCompile()
{
	InterlockedDecrement(&m_GlobalCompileTasks);
}

bool CCrySimpleJobCompile::Execute(const TiXmlElement* pElement)
{
	std::vector<uint8_t>& rVec = *m_pRVec;

	size_t Size = SizeOf(rVec);

	CheckHashID(rVec, Size);

	if (State() == ECSJS_CACHEHIT)
	{
		State(ECSJS_DONE);
		return true;
	}

	if (!SEnviropment::Instance().m_FallbackServer.empty() && m_GlobalCompileTasks > SEnviropment::Instance().m_FallbackTreshold)
	{
		tdEntryVec ServerVec;
		CSTLHelper::Tokenize(ServerVec, SEnviropment::Instance().m_FallbackServer, ";");
		uint32_t Idx = m_RemoteServerID++;
		uint32_t Count = (uint32_t)ServerVec.size();
		std::string Server = ServerVec[Idx%Count];
		printf("  Remote Compile on %s ...\n", Server.c_str());
		CCrySimpleSock Sock(Server, SEnviropment::Instance().m_port);
		if (Sock.Valid())
		{
			Sock.Forward(rVec);
			std::vector<uint8_t> Tmp;
			if (Sock.Backward(Tmp))
			{
				rVec = Tmp;
				if (Tmp.size() <= 4 || (m_Version == EPV_V002 && Tmp[4] != ECSJS_DONE))
				{
					State(ECSJS_ERROR_COMPILE);
					CrySimple_ERROR("failed to compile request");
					return false;
				}
				State(ECSJS_DONE);
				//printf("done\n");
			}
			else
				printf("failed, fallback to local\n");
		}
		else
			printf("failed, fallback to local\n");
	}
	if (State() == ECSJS_NONE)
	{
		if (!Compile(pElement, rVec) || rVec.size() == 0)
		{
			State(ECSJS_ERROR_COMPILE);
			CrySimple_ERROR("failed to compile request");
			return false;
		}

		tdDataVector rDataRaw;
		rDataRaw.swap(rVec);
		if (!CSTLHelper::Compress(rDataRaw, rVec))
		{
			State(ECSJS_ERROR_COMPRESS);
			CrySimple_ERROR("failed to compress request");
			return false;
		}
		State(ECSJS_DONE);
	}

	// Cache compiled data
	const char* pCaching = pElement->Attribute("Caching");
	if (State() != ECSJS_ERROR && (!pCaching || std::string(pCaching) == "1"))
		CCrySimpleCache::Instance().Add(HashID(), rVec);

	return true;
}

bool CCrySimpleJobCompile::Compile(const TiXmlElement* pElement, std::vector<uint8_t>& rVec)
{
	const char* pProfile = pElement->Attribute("Profile");
	const char* pProgram = pElement->Attribute("Program");
	const char* pEntry = pElement->Attribute("Entry");
	const char* pCompileFlags = pElement->Attribute("CompileFlags");
	const char* pShaderRequestLine = pElement->Attribute("ShaderRequest");


	if (!pProfile)
	{
		State(ECSJS_ERROR_INVALID_PROFILE);
		CrySimple_ERROR("failed to extract Profile of the request");
		return false;
	}
	if (!pProgram)
	{
		State(ECSJS_ERROR_INVALID_PROGRAM);
		CrySimple_ERROR("failed to extract Program of the request");
		return false;
	}
	if (!pEntry)
	{
		State(ECSJS_ERROR_INVALID_ENTRY);
		CrySimple_ERROR("failed to extract Entry of the request");
		return false;
	}
	if (!pCompileFlags)
	{
		State(ECSJS_ERROR_INVALID_COMPILEFLAGS);
		CrySimple_ERROR("failed to extract Compile+Flags of the request");
		return false;
	}

	const char* pArgs = strchr(pCompileFlags, ' ');

	if (!pArgs || *(pArgs + 1) == '\0')
	{
		State(ECSJS_ERROR_INVALID_COMPILEFLAGS);
		CrySimple_ERROR("failed to split CompileFlags into exectuable and arguments");
		return false;
	}

	static long volatile sTmpCounter = 0;
	long tmpCounter = InterlockedIncrement(&sTmpCounter);

	char tmpstr[64];
	sprintf(tmpstr, "%d", tmpCounter);

	const std::string TmpIn = SEnviropment::Instance().m_Temp + tmpstr + ".In";
	const std::string TmpOut = SEnviropment::Instance().m_Temp + tmpstr + ".Out";
	CCrySimpleFileGuard FGTmpIn(TmpIn);
	CCrySimpleFileGuard FGTmpOut(TmpOut);
	CSTLHelper::ToFile(TmpIn, std::vector<uint8_t>(pProgram, &pProgram[strlen(pProgram)]));

	char args[1024];
	sprintf(args, pArgs + 1, pEntry, pProfile, ("\"" + TmpOut + "\"").c_str(), ("\"" + TmpIn + "\"").c_str());

	const std::string Cmd = SEnviropment::Instance().m_Compiler + std::string(pCompileFlags, pArgs);

	__int64 t0 = g_Timer.GetTime();

	std::string outError;
	if (!ExecuteCommand(Cmd, args, outError))
	{
		unsigned char* nIP = (unsigned char*)&RequestIP();
		char sIP[128];
		sprintf(sIP, "%d.%d.%d.%d", nIP[0], nIP[1], nIP[2], nIP[3]);

		const char* pPlatform = pElement->Attribute("Platform");
		const char* pProject = pElement->Attribute("Project");
		const char* pTags = pElement->Attribute("Tags");

		const char* pEmailCCs = pElement->Attribute("EmailCCs");

		std::string project = pProject ? pProject : "Unk/";
		std::string ccs = pEmailCCs ? pEmailCCs : "";
		std::string platform = pPlatform ? pPlatform : "Unknown";
		std::string tags = pTags ? pTags : "";

		std::string filteredError;
		CSTLHelper::Replace(filteredError, outError, TmpIn + ".patched", "%filename%"); // DXPS does its own patching
		CSTLHelper::Replace(filteredError, filteredError, TmpIn, "%filename%");
		// replace any that don't have the full path
		CSTLHelper::Replace(filteredError, filteredError, std::string(tmpstr) + ".In.patched", "%filename%"); // DXPS does its own patching
		CSTLHelper::Replace(filteredError, filteredError, std::string(tmpstr) + ".In", "%filename%");

		CSTLHelper::Replace(filteredError, filteredError, "\r\n", "\n");

		State(ECSJS_ERROR_COMPILE);
		throw new CCompilerError(pEntry, filteredError, ccs, sIP, pShaderRequestLine, pProgram, project, platform, tags, pProfile);
		return false;
	}

	if (!CSTLHelper::FromFile(TmpOut, rVec))
	{
		State(ECSJS_ERROR_FILEIO);
		CrySimple_ERROR(std::string("Could not read: ") + TmpOut);
		return false;
}

	__int64 t1 = g_Timer.GetTime();
	__int64 dt = t1 - t0;
#ifdef _WIN64
	InterlockedAdd64(&m_GlobalCompileTime, dt);
#endif

	const char* pPlatform = pElement->Attribute("Platform");
	if (pPlatform == NULL)
		pPlatform = "Unk";

	int millis = (int)(g_Timer.TimeToSeconds(dt) * 1000.0);
	int secondsTotal = (int)g_Timer.TimeToSeconds(m_GlobalCompileTime);
	logmessage("Compiled [%5dms|%8ds] (% 5s %s) %s\n", millis, secondsTotal, pPlatform, pProfile, pEntry);

	return true;
}

//////////////////////////////////////////////////////////////////////////
inline bool SortByLinenum(const std::pair<int, std::string> &f1, const std::pair<int, std::string> &f2)
{
	return f1.first < f2.first;
}

CCompilerError::CCompilerError(std::string entry, std::string errortext, std::string ccs, std::string IP,
	std::string requestLine, std::string program, std::string project,
	std::string platform, std::string tags, std::string profile)
	: ICryError(COMPILE_ERROR),
	m_entry(entry), m_errortext(errortext), m_IP(IP),
	m_program(program), m_project(project),
	m_platform(platform), m_tags(tags), m_profile(profile),
	m_uniqueID(0)
{
	m_requests.push_back(requestLine);
	Init();

	CSTLHelper::Tokenize(m_CCs, ccs, ";");
}

void CCompilerError::Init()
{
	while (!m_errortext.empty() && (m_errortext.back() == '\r' || m_errortext.back() == '\n'))
		m_errortext.pop_back();

	if (m_requests[0].size())
	{
		m_shader = m_requests[0];
		size_t offs = m_shader.find('>');
		if (offs != std::string::npos)
			m_shader.erase(0, m_shader.find('>') + 1); // remove <2> version

		offs = m_shader.find('@');
		if (offs != std::string::npos)
			m_shader.erase(m_shader.find('@')); // remove everything after @

		offs = m_shader.find('/');
		if (offs != std::string::npos)
			m_shader.erase(m_shader.find('/')); // remove everything after / (used on xenon)
	}
	else
	{
		// default to entry function
		m_shader = m_entry;
		size_t len = m_shader.length();

		// if it ends in ?S then trim those two characters
		if (m_shader[len - 1] == 'S')
		{
			m_shader.pop_back();
			m_shader.pop_back();
		}
	}

	std::vector<std::string> lines;
	CSTLHelper::Tokenize(lines, m_errortext, "\n");

	for (uint32_t i = 0; i < lines.size(); i++)
	{
		std::string &line = lines[i];

		if (line.substr(0, 5) == "error")
		{
			m_errors.push_back(std::pair<int, std::string>(-1, line));
			m_hasherrors += line;

			continue;
		}

		if (line.find(": error") == std::string::npos)
			continue;

		if (line.substr(0, 10) != "%filename%")
			continue;

		if (line[10] != '(')
			continue;

		uint32_t c = 11;

		int linenum = 0;
		{
			bool ln = true;
			while (c < line.length() &&
				((line[c] >= '0' && line[c] <= '9') || line[c] == ',' || line[c] == '-')
				)
			{
				if (line[c] == ',') ln = false; // reached column, don't save the value - just keep reading to the end

				if (ln)
				{
					linenum *= 10;
					linenum += line[c] - '0';
				}
				c++;
			}

			if (c >= line.length())
				continue;

			if (line[c] != ')')
				continue;

			c++;
		}

		while (c < line.length() && (line[c] == ' ' || line[c] == ':'))
			c++;

		if (line.substr(c, 5) != "error")
			continue;

		m_errors.push_back(std::pair<int, std::string>(linenum, line));
		m_hasherrors += line.substr(c);
	}

	std::sort(m_errors.begin(), m_errors.end(), SortByLinenum);
}

std::string CCompilerError::GetErrorLines() const
{
	std::string ret = "";

	for (uint32_t i = 0; i < m_errors.size(); i++)
	{
		if (m_errors[i].first < 0)
		{
			ret += m_errors[i].second + "\n";
		}
		else if (i > 0 && m_errors[i - 1].first < 0)
		{
			ret += "\n" + GetContext(m_errors[i].first) + "\n" + m_errors[i].second + "\n\n";
		}
		else if (i > 0 && m_errors[i - 1].first == m_errors[i].first)
		{
			ret.pop_back(); // pop extra newline
			ret += m_errors[i].second + "\n\n";
		}
		else
			ret += GetContext(m_errors[i].first) + "\n" + m_errors[i].second + "\n\n";
	}

	return ret;
}

std::string CCompilerError::GetContext(int linenum, int context, std::string prefix) const
{
	std::vector<std::string> lines;
	CSTLHelper::Tokenize(lines, m_program, "\n");

	std::string ret = "";

	linenum--; // line numbers start at one

	char sLineNum[16];

	for (uint32_t i = max(0U, (uint32_t)(linenum - context)); i <= min((uint32_t)lines.size() - 1U, (uint32_t)(linenum + context)); i++)
	{
		sprintf(sLineNum, "% 3d", i + 1);

		ret += sLineNum;
		ret += " ";

		if (prefix.size())
		{
			if (i == linenum)
				ret += "*";
			else
				ret += " ";

			ret += prefix;

			ret += " ";
		}

		ret += lines[i] + "\n";
	}

	return ret;
}

void CCompilerError::AddDuplicate(ICryError *err)
{
	ICryError::AddDuplicate(err);

	if (err->GetType() == COMPILE_ERROR)
	{
		CCompilerError *comperr = (CCompilerError *)err;
		m_requests.insert(m_requests.end(), comperr->m_requests.begin(), comperr->m_requests.end());
	}
}

bool CCompilerError::Compare(const ICryError *err) const
{
	if (GetType() != err->GetType())
		return GetType() < err->GetType();

	CCompilerError *e = (CCompilerError *)err;

	if (m_platform != e->m_platform)
		return m_platform < e->m_platform;

	if (m_shader != e->m_shader)
		return m_shader < e->m_shader;

	if (m_entry != e->m_entry)
		return m_entry < e->m_entry;

	return Hash() < err->Hash();
}

bool CCompilerError::CanMerge(const ICryError *err) const
{
	if (GetType() != err->GetType()) // don't merge with non compile errors
		return false;

	CCompilerError *e = (CCompilerError *)err;

	if (m_platform != e->m_platform || m_shader != e->m_shader)
		return false;

	if (m_CCs.size() != e->m_CCs.size())
		return false;

	for (size_t a = 0, S = m_CCs.size(); a < S; a++)
		if (m_CCs[a] != e->m_CCs[a])
			return false;

	return true;
}

void CCompilerError::AddCCs(std::set<std::string> &ccs) const
{
	for (size_t a = 0, S = m_CCs.size(); a < S; a++)
		ccs.insert(m_CCs[a]);
}

std::string CCompilerError::GetErrorName() const
{
	return std::string("[") + m_tags + "] Shader Compile Errors in " + m_shader + " on " + m_platform;
}

std::string CCompilerError::GetErrorDetails(EOutputFormatType outputType) const
{
	std::string errorString("");

	char sUniqueID[16], sNumDuplicates[16];
	sprintf(sUniqueID, "%d", m_uniqueID);
	sprintf(sNumDuplicates, "%d", NumDuplicates());

	std::string errorOutput;
	CSTLHelper::Replace(errorOutput, GetErrorLines(), "%filename%", std::string(sUniqueID) + "-" + GetFilename());

	std::string fullOutput;
	CSTLHelper::Replace(fullOutput, m_errortext, "%filename%", std::string(sUniqueID) + "-" + GetFilename());

	if (outputType == OUTPUT_HASH)
	{
		errorString = GetFilename() + m_IP + m_platform + m_project + m_entry + m_tags + m_profile + m_hasherrors /*+ m_requestline*/;
	}
	else if (outputType == OUTPUT_EMAIL)
	{
		errorString = std::string("=== Shader compile error in ") + m_entry + " (" + sNumDuplicates + " duplicates)\n\n";

		/////
		errorString += std::string("* From:                  ") + m_IP + " on " + m_platform + " " + m_project;
		if (m_tags != "")
			errorString += std::string(" (Tags: ") + m_tags + ")";
		errorString += "\n";

		/////
		errorString += std::string("* Target profile:        ") + m_profile + "\n";

		/////
		bool hasrequests = false;
		for (uint32_t i = 0; i < m_requests.size(); i++)
		{
			if (m_requests[i].size())
			{
				errorString += std::string("* Shader request line:   ") + m_requests[i] + "\n";
				hasrequests = true;
			}
		}

		errorString += "\n";

		if (hasrequests)
			errorString += "* Shader source from first listed request\n";

		errorString += std::string("* Reported error(s) from ") + sUniqueID + "-" + GetFilename() + "\n\n";
		errorString += errorOutput + "\n\n";

		errorString += std::string("* Full compiler output:\n\n");
		errorString += fullOutput + "\n";
	}
	else if (outputType == OUTPUT_TTY)
	{
		errorString = std::string("===  Shader compile error in ") + m_entry + " { " + m_requests[0] + " }\n";
		// errors only
		errorString += std::string("* Reported error(s):\n\n");
		errorString += errorOutput;
	}

	return errorString;
}
