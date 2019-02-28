// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RemoteCompiler.h : socket wrapper for shader compile server connections

   Revision history:
* Created by Michael Kopietz

   =============================================================================*/

#ifndef REMOTECOMPILER_H
#define REMOTECOMPILER_H

#include <CryNetwork/CrySocks.h>

namespace NRemoteCompiler
{
typedef std::vector<string> tdEntryVec;

enum EServerError
{
	ESOK,
	ESFailed,
	ESInvalidState,
	ESCompileError,
	ESNetworkError,
	ESSendFailed,
	ESRecvFailed,
};

class CShaderSrv
{
protected:
	static uint32 m_LastWorkingServer;

	// root path added to each requestline to store the data per game (eg. MyGame\)
	string m_RequestLineRootFolder;

	CShaderSrv();

	bool         Send(CRYSOCKET Socket, const char* pBuffer, uint32 Size) const;
	bool         Send(CRYSOCKET Socket, std::vector<uint8>& rCompileData) const;
	EServerError Recv(CRYSOCKET Socket, std::vector<uint8>& rCompileData) const;
	EServerError Send(std::vector<uint8>& rCompileData) const;

	void         Tokenize(tdEntryVec& rRet, const string& Tokens, const string& Separator) const;
	string       TransformToXML(const string& rIn) const;
	string       CreateXMLNode(const string& rTag, const string& rValue)  const;
	//	string							CreateXMLDataNode(const string& rTag,const string& rValue)	const;

	bool RequestLine(const SCacheCombination& cmb,
	                 const string& rLine) const;
	bool CreateRequest(std::vector<uint8>& rVec,
	                   std::vector<std::pair<string, string>>& rNodes) const;

	void         Init();
public:
	EServerError Compile(std::vector<uint8>& rVec,
	                     const char* pProfile,
	                     const char* pProgram,
	                     const char* pEntry,
	                     const char* pCompileFlags,
	                     const char* pIdent) const;
	bool               CommitPLCombinations(std::vector<SCacheCombination>& rVec);
	bool               RequestLine(const string& rList,
	                               const string& rString)  const;
	const char*        GetPlatform() const;

	static CShaderSrv& Instance();
};
}

#endif
