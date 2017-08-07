// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include "resource.h"
#include <CryCore/Platform/CryLibrary.h>
#include <CryGame/IGameFramework.h>
#include <CrySystem/IConsole.h>

#include <CryCore/Platform/CryWindows.h>
#include <ShellAPI.h> // requires <windows.h>

// We need shell api for Current Root Extruction.
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

static unsigned key[4] = {1339822019,3471820962,4179589276,4119647811};
static unsigned text[4] = {4114048726,1217549643,1454516917,859556405};
static unsigned hash[4] = {324609294,3710280652,1292597317,513556273};

#ifdef _LIB
extern "C" IGameFramework* CreateGameFramework();
#endif //_LIB

// src and trg can be the same pointer (in place encryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit:  int key[4] = {n1,n2,n3,n4};
// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE( src,trg,len,key ) {\
	unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) {\
	unsigned int y=v[0],z=v[1],n=32,sum=0; \
	while(n-->0) { sum += delta; y += (z << 4)+a ^ z+sum ^ (z >> 5)+b; z += (y << 4)+c ^ y+sum ^ (y >> 5)+d; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

// src and trg can be the same pointer (in place decryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit: int key[4] = {n1,n2,n3,n4};
// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE( src,trg,len,key ) {\
	unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) { \
	unsigned int y=v[0],z=v[1],sum=0xC6EF3720,n=32; \
	while(n-->0) { z -= (y << 4)+c ^ y+sum ^ (y >> 5)+d; y -= (z << 4)+a ^ z+sum ^ (z >> 5)+b; sum -= delta; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

ILINE unsigned Hash( unsigned a )
{
	a = (a+0x7ed55d16) + (a<<12);
	a = (a^0xc761c23c) ^ (a>>19);
	a = (a+0x165667b1) + (a<<5);
	a = (a+0xd3a2646c) ^ (a<<9);
	a = (a+0xfd7046c5) + (a<<3);
	a = (a^0xb55a4f09) ^ (a>>16);
	return a;
}

// encode size ignore last 3 bits of size in bytes. (encode by 8bytes min)
#define TEA_GETSIZE( len ) ((len) & (~7))

ILINE int RunGame(const char* szCommandLine)
{
	SSystemInitParams startupParams;
	const char* frameworkDLLName = "CryAction.dll";

	CryFindRootFolderAndSetAsCurrentWorkingDirectory();

	//restart parameters
	static const char logFileName[] = "Server.log";

	unsigned buf[4];
	TEA_DECODE((unsigned*)text,buf,16,(unsigned*)key);

	HMODULE frameworkDll = 0;

#if !defined(_LIB)
	// load the framework dll
	frameworkDll = CryLoadLibrary(frameworkDLLName);

	if (!frameworkDll)
	{
		string errorStr;
		errorStr.Format("Failed to load the Game DLL! %s", frameworkDLLName);

		MessageBox(0, errorStr.c_str(), "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
		// failed to load the dll

		return EXIT_FAILURE;
	}

	// get address of startup function
	IGameFramework::TEntryFunction CreateGameFramework = (IGameFramework::TEntryFunction)CryGetProcAddress(frameworkDll, "CreateGameFramework");

	if (!CreateGameFramework)
	{
		// dll is not a compatible game dll
		CryFreeLibrary(frameworkDll);

		MessageBox(0, "Specified Game DLL is not valid! Please make sure you are running the correct executable", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		return EXIT_FAILURE;
	}
#endif //!defined(_LIB)

	cry_strcpy(startupParams.szSystemCmdLine, szCommandLine);
	cry_strcat(startupParams.szSystemCmdLine, (const char*)buf);

	startupParams.sLogFileName = logFileName;

	for (int i=0; i<4; i++)
		if (Hash(buf[i])!=hash[i])
			return EXIT_FAILURE;

	// create the startup interface
	IGameFramework* pFramework = CreateGameFramework();
	if (!pFramework)
	{
		// failed to create the startup interface
		CryFreeLibrary(frameworkDll);

		MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		return EXIT_FAILURE;
	}

	// main game loop
	pFramework->StartEngine(startupParams);

	// The main engine loop has exited at this point, shut down
	pFramework->ShutdownEngine();

	CryFreeLibrary(frameworkDll);

	return EXIT_SUCCESS;
}


///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// we need pass the full command line, including the filename
	// lpCmdLine does not contain the filename.
	string cmdLine = CryStringUtils::ANSIToUTF8(lpCmdLine);
#if CAPTURE_REPLAY_LOG
#ifndef _LIB
	CryLoadLibrary("CrySystem.dll");
#endif
	CryGetIMemReplay()->StartOnCommandLine(cmdLine.c_str());
#endif

	// Read the full command line, including executable line
	cmdLine = CryStringUtils::ANSIToUTF8(GetCommandLineA());
	
/*
	unsigned buf[4];
                 //  0123456789abcdef
	char secret[16] = "  -dedicated   ";
	TEA_ENCODE((unsigned int*)secret, (unsigned int*)buf, 16, key);
	for (int i=0; i<4; i++)
		hash[i] = Hash(((unsigned*)secret)[i]);
*/

	return RunGame(cmdLine.c_str());
}

#ifdef _LIB
// Include common type defines for static linking
// Manually instantiate templates as needed here.
#include <CryCore/Common_TypeInfo.h>
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec4_tpl, <short>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Ang3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Plane_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Matrix33_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <uint8>)
#endif
