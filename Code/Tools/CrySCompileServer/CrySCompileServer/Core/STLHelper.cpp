// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdTypes.hpp"
#include "Error.hpp"
#include "STLHelper.hpp"
#include <algorithm>
#include <functional>
#include <iterator>
#ifdef _MSC_VER
#include <io.h>
#include <windows.h>
#endif
#include "MD5.hpp"
#include <assert.h>
#include "zlib/zlib.h"


void CSTLHelper::Log(const std::string& rLog)
{
	const std::string Output=rLog+"\n";
  logmessage(Output.c_str());
}

void CSTLHelper::Tokenize(tdEntryVec& rRet,const std::string& Tokens,const std::string& Separator)
{
		rRet.clear();
		std::string::size_type Pt;
		std::string::size_type Start	= 0;
		std::string::size_type SSize	=	Separator.size();

		while((Pt = Tokens.find(Separator,Start)) != std::string::npos)
		{
			std::string  SubStr	=	Tokens.substr(Start,Pt-Start);
			rRet.push_back(SubStr);
			Start = Pt + SSize;
		}

		rRet.push_back(Tokens.substr(Start));
}


void CSTLHelper::Replace(std::string& rRet,const std::string& rSrc,const std::string& rToReplace,const std::string& rReplacement)
{
	std::vector<uint8_t> Out;
	std::vector<uint8_t> In(rSrc.c_str(),rSrc.c_str()+rSrc.size()+1);
	Replace(Out,In,rToReplace,rReplacement);
	rRet	=	std::string(reinterpret_cast<char*>(&Out[0]));
}

void CSTLHelper::Replace(std::vector<uint8_t>& rRet,const std::vector<uint8_t>& rTokenSrc,const std::string& rToReplace,const std::string& rReplacement)
{
	rRet.clear();
	size_t	SSize	=	rToReplace.size();
	for(size_t a=0,Size=rTokenSrc.size();a<Size;a++)
	{
		if(a+SSize<Size && strncmp((const char*)&rTokenSrc[a],rToReplace.c_str(),SSize)==0)
		{
			for(size_t b=0,RSize=rReplacement.size();b<RSize;b++)
				rRet.push_back(rReplacement.c_str()[b]);
			a+=SSize-1;
		}
		else
			rRet.push_back(rTokenSrc[a]);
	}
}

tdToken	CSTLHelper::SplitToken(const std::string& rToken,const std::string& rSeparator)
{
#undef min
	using namespace std;
	string Token;
	Remove(Token,rToken,' ');

	string::size_type Pt=Token.find(rSeparator);
	return tdToken(Token.substr(0,Pt),Token.substr(std::min(Pt+1,Token.size())));
}

void CSTLHelper::Splitizer(tdTokenList& rTokenList,const tdEntryVec& rFilter,const std::string& rSeparator)
{
	rTokenList.clear();
	for(size_t a=0,Size=rFilter.size();a<Size;a++)
		rTokenList.push_back(SplitToken(rFilter[a],rSeparator));
}

void CSTLHelper::Trim(std::string& rStr,char C)
{
	std::string::size_type Pt1 = rStr.find_first_not_of(C);
	if(Pt1==std::string::npos)
		return;

	std::string::size_type Pt2 = rStr.find_last_not_of(C) + 1;

	Pt2		=	Pt2-Pt1;
	rStr	=	rStr.substr(Pt1,Pt2);
}

void CSTLHelper::Remove(std::string& rTokenDst,const std::string& rTokenSrc,const char C)
{
	using namespace std;
	remove_copy_if(rTokenSrc.begin(), rTokenSrc.end(), back_inserter(rTokenDst), bind2nd(equal_to<char>(),C) ) ;
}


bool CSTLHelper::ToFile(const std::string& rFileName,const std::vector<uint8_t>&	rOut)
{
	if (rOut.size() == 0)
	{ 
		//CrySimple_ERROR(std::string("Empty request on filename: ")+rFileName);
		return false;
	}

	HANDLE hFile; 

	hFile = CreateFile(rFileName.c_str(),     // file to create
		GENERIC_WRITE,          // open for writing
		0,        
		NULL,                   // default security
		CREATE_ALWAYS,          // overwrite existing
		FILE_ATTRIBUTE_NORMAL,  // normal file
		NULL);                  // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{ 
		DWORD dError = GetLastError();
		char acTmp[1024];
		sprintf(acTmp, "--- Error opening file with error code: %d \n", dError);
		OutputDebugString(acTmp);
		CrySimple_ERROR(std::string("Could not create: ")+rFileName);
		return false;
	}

	DWORD lpNumberOfBytesWritten;
	WriteFile( hFile,&rOut[0],(DWORD)rOut.size(),&lpNumberOfBytesWritten,NULL );

	CloseHandle(hFile);

		/*
	
	FILE* pFile	=	fopen(rFileName.c_str(),"wb");
	if(!pFile)
	{
		CrySimple_ERROR(std::string("Could not create: ")+rFileName);
		return;
	}
	fwrite(&rOut[0],1,rOut.size(),pFile);
	fclose(pFile);
	*/
	return true;
}

bool CSTLHelper::FromFile(const std::string& rFileName,std::vector<uint8_t>&	rIn)
{
	FILE* pFile	=	fopen(rFileName.c_str(),"rb");
	if(!pFile)
	{
		return false;
	}
	uint32_t FileLen	=	_filelength(_fileno(pFile));
	if(FileLen<0)
	{
		return false;
	}
	size_t nNumRead = 0;
	rIn.resize(FileLen);
	if(FileLen>0)
	{
		nNumRead = fread(&rIn[0],1,FileLen,pFile);
	}
	fclose(pFile);
	return nNumRead == FileLen;
}

bool CSTLHelper::ToFileCompressed(const std::string& rFileName,const std::vector<uint8_t>&	rOut)
{
	std::vector<uint8_t> buf;

	unsigned long destLen,sourceLen = (unsigned long)rOut.size();
	destLen = compressBound(sourceLen) + 16;
	buf.resize( destLen );
	compress( &buf[0],&destLen,&rOut[0],sourceLen );
	
	if (destLen > 0)
	{
		HANDLE hFile; 

		hFile = CreateFile(rFileName.c_str(),     // file to create
			GENERIC_WRITE,          // open for writing
			0,        
			NULL,                   // default security
			CREATE_ALWAYS,          // overwrite existing
			FILE_ATTRIBUTE_NORMAL | // normal file
			0/*FILE_FLAG_NO_BUFFERING*/,
			NULL);                  // no attr. template

		if (hFile == INVALID_HANDLE_VALUE)
		{ 
			DWORD dError = GetLastError();
			char acTmp[1024];
			sprintf(acTmp, "--- Error opening file with error code: %d \n", dError);
			OutputDebugString(acTmp);
			CrySimple_ERROR(std::string("Could not create: ")+rFileName);
			return false;
		}

		DWORD lpNumberOfBytesWritten;

		if (WriteFile( hFile,&sourceLen,4,&lpNumberOfBytesWritten,NULL ) != TRUE || lpNumberOfBytesWritten != 4)
		{
			CloseHandle(hFile);
			CrySimple_ERROR(std::string("Could not save: ")+rFileName);
			return false;
		}
		if (WriteFile( hFile,&buf[0],(DWORD)destLen,&lpNumberOfBytesWritten,NULL ) != TRUE || lpNumberOfBytesWritten != destLen)
		{
			CloseHandle(hFile);
			CrySimple_ERROR(std::string("Could not save: ")+rFileName);
			return false;
		}

		CloseHandle(hFile);
		return true;
	}
	else
		return false;
}

bool CSTLHelper::FromFileCompressed(const std::string& rFileName,std::vector<uint8_t>&	rIn)
{
	/*
	HANDLE hFile; 

	hFile = CreateFile(rFileName.c_str(),     // file to create
		GENERIC_READ,          // open for writing
		FILE_SHARE_READ,        
		NULL,                   // default security
		OPEN_EXISTING,          // overwrite existing
		FILE_FLAG_SEQUENTIAL_SCAN, // normal file		FILE_FLAG_NO_BUFFERING,
		NULL);                  // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{ 
		printf( "Could not open for read: %s",rFileName.c_str() );
		return false;
	}

	DWORD dwFileSize = GetFileSize(hFile,NULL);
	if (dwFileSize <= 4 || dwFileSize == -1)
	{
		CloseHandle(hFile);
		return false;
	}

	DWORD dwNumberOfBytesRead = 0;
	
	std::vector<uint8_t> buf;
	buf.resize(dwFileSize);

	if (ReadFile( hFile,&buf[0],dwFileSize,&dwNumberOfBytesRead,NULL ) != TRUE)
	{
		CloseHandle(hFile);
		return false;
	}
	if (dwNumberOfBytesRead != dwFileSize)
	{
		CloseHandle(hFile);
		return false;
	}

	CloseHandle(hFile);

	unsigned long uncompressedLen = 0;
	unsigned long sourceLen = dwFileSize-4;

	uncompressedLen = *(unsigned long*)(&buf[0]);

	unsigned long nUncompressedBytes = uncompressedLen;
	rIn.resize(uncompressedLen);
	int nRes = uncompress( &rIn[0],&nUncompressedBytes,&buf[4],sourceLen );

	return nRes == Z_OK && nUncompressedBytes == uncompressedLen;
	*/


	std::vector<uint8_t> buf;
	FILE* pFile	=	fopen(rFileName.c_str(),"rb");
	if(!pFile)
	{
		CrySimple_ERROR(std::string("Could not read: ")+rFileName);
		return false;
	}
	uint32_t FileLen	=	_filelength(_fileno(pFile));
	if(FileLen<0)
	{
		CrySimple_ERROR(std::string("Error getting file-size of ")+rFileName);
		return false;
	}
	unsigned long uncompressedLen=0;
	unsigned long sourceLen = FileLen-4;
	if(FileLen>0)
	{
		buf.resize(FileLen-4);
		fread(&uncompressedLen,1,4,pFile);
		fread(&buf[0],1,sourceLen,pFile);
	}
	fclose(pFile);

	unsigned long nUncompressedBytes = uncompressedLen;
	rIn.resize(uncompressedLen);
	int nRes = uncompress( &rIn[0],&nUncompressedBytes,&buf[0],sourceLen );

	return nRes == Z_OK && nUncompressedBytes == uncompressedLen;
}

//////////////////////////////////////////////////////////////////////////
bool CSTLHelper::AppendToFile(const std::string& rFileName,const std::vector<uint8_t>&	rOut)
{
	HANDLE hFile; 

	hFile = CreateFile(rFileName.c_str(),     // file to create
		GENERIC_WRITE,          // open for writing
		0,        
		NULL,                   // default security
		OPEN_ALWAYS,						// overwrite existing
		FILE_ATTRIBUTE_NORMAL,  // normal file
		NULL);                  // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{ 
		DWORD dError = GetLastError();
		char acTmp[1024];
		sprintf(acTmp, "--- Error opening file with error code: %d \n", dError);
		OutputDebugString(acTmp);
		CrySimple_ERROR(std::string("Could not create: ")+rFileName);
		return false;
	}

	DWORD dwAddSize = (DWORD)rOut.size();
	DWORD lpNumberOfBytesWritten = 0;
	LONG dwPosLow = 0;
	LONG dwPosHigh = 0;
	dwPosLow = SetFilePointer(hFile,0,&dwPosHigh,FILE_END);
	uint64_t filePos = (((uint64_t)dwPosHigh) << 32) | ((uint64_t)dwPosLow);
	LockFile( hFile,dwPosLow,dwPosHigh,dwAddSize,0);
	WriteFile( hFile,&rOut[0],dwAddSize,&lpNumberOfBytesWritten,NULL );
	UnlockFile( hFile,dwPosLow,dwPosHigh,dwAddSize,0);

	CloseHandle(hFile);
	return true;
}

//////////////////////////////////////////////////////////////////////////
tdHash CSTLHelper::Hash(const uint8_t*	pData,const size_t Size)
{
	tdHash CheckSum;
	cvs_MD5Context MD5Context;	
	cvs_MD5Init(MD5Context);
	cvs_MD5Update(MD5Context,pData,static_cast<uint32_t>(Size));
	cvs_MD5Final(CheckSum.hash,MD5Context);
	return CheckSum;
}

static char C2A[17]="0123456789ABCDEF";

std::string	CSTLHelper::Hash2String(const tdHash& rHash)
{
	std::string Ret;
	for(size_t a=0,Size=std::min<size_t>(sizeof(rHash.hash),16u);a<Size;a++)
	{
		const uint8_t	C1=rHash[a]&0xf;
		const uint8_t	C2=rHash[a]>>4;
		Ret+=C2A[C1];
		Ret+=C2A[C2];
	}
	return Ret;
}

tdHash CSTLHelper::String2Hash(const std::string& rStr)
{
	assert(rStr.size()==32);
	tdHash	Ret;
	for(size_t a=0,Size=std::min<size_t>(rStr.size(),32u);a<Size;a+=2)
	{
		const uint8_t	C1=rStr.c_str()[a];
		const uint8_t	C2=rStr.c_str()[a+1];
		Ret[a>>1]=C1-(C1>='0'&&C1<='9'?'0':'A'-10);
		Ret[a>>1]|=(C2-(C2>='0'&&C2<='9'?'0':'A'-10))<<4;
	}
	return Ret;
}

//////////////////////////////////////////////////////////////////////////
bool	CSTLHelper::Compress(const std::vector<uint8_t>& rIn,std::vector<uint8_t>& rOut)
{
	unsigned long destLen,sourceLen = (unsigned long)rIn.size();
	destLen = compressBound(sourceLen) + 16;
	rOut.resize( destLen+4 );
	compress( &rOut[4],&destLen,&rIn[0],sourceLen );
	rOut.resize( destLen+4 );
	*(uint32_t*)(&rOut[0]) = sourceLen;
	return true;
}

bool	CSTLHelper::Uncompress(const std::vector<uint8_t> &rIn,std::vector<uint8_t>& rOut)
{
	unsigned long sourceLen = (unsigned long)rIn.size()-4;
	unsigned long nUncompressed = *(uint32_t*)(&rIn[0]);
	unsigned long nUncompressedBytes = nUncompressed;
	rOut.resize(nUncompressed);
	int nRes = uncompress( &rOut[0],&nUncompressedBytes,&rIn[4],sourceLen );
	return nRes == Z_OK && nUncompressed == nUncompressedBytes;
}
