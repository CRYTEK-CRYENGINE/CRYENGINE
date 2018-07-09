// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ResourceCompilerPC.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include <CryCore/Assert/CryAssert_impl.h>
#include "StatCGFCompiler.h"
#include "CharacterCompiler.h"
#include "ChunkCompiler.h"
#include "ColladaCompiler.h"
#include "CGA/AnimationCompiler.h"
#include "FBX/FbxConverter.h"
#include "Metadata/MetadataCompiler.h"
#include "LuaCompiler.h"
#include "../../CryXML/ICryXML.h"

#include <CryCore/Platform/CryWindows.h>   // HANDLE

#include "ResourceCompilerPC.h"

// Must be included only once in DLL module.
#include <platform_implRC.inl>


static HMODULE g_hInst;


BOOL APIENTRY DllMain(
	HANDLE hModule, 
	DWORD  ul_reason_for_call, 
	LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_hInst = (HMODULE)hModule;
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

ICryXML* LoadICryXML()
{
	HMODULE hXMLLibrary = LoadLibraryA("CryXML.dll");
	if (NULL == hXMLLibrary)
	{
		char szCurrentDirectory[512];
		GetCurrentDirectory(sizeof(szCurrentDirectory),szCurrentDirectory);

		RCLogError("Unable to load xml library (CryXML.dll)");
		RCLogError("  Current Directory: %s",szCurrentDirectory);		// useful to track down errors
		return 0;
	}

	FnGetICryXML pfnGetICryXML = (FnGetICryXML)GetProcAddress(hXMLLibrary, "GetICryXML");
	if (pfnGetICryXML == 0)
	{
		RCLogError("Unable to load xml library (CryXML.dll) - cannot find exported function GetICryXML().");
		return 0;
	}

	return pfnGetICryXML();
}

// This is an example of an exported function.
void __stdcall RegisterConverters(IResourceCompiler* const pRC)
{
	SetRCLog(pRC->GetIRCLog());

	pRC->RegisterConverter("StatCGFCompiler", new CStatCGFCompiler());

	pRC->RegisterConverter("ChunkCompiler", new CChunkCompiler());

	pRC->RegisterConverter("LuaCompiler", new LuaCompiler());

#ifdef TOOLS_ENABLE_FBX_SDK
	pRC->RegisterConverter("FbxConverter", new CFbxConverter());
#endif // TOOLS_ENABLE_FBX_SDK	

	pRC->RegisterConverter("MetadataCompiler", new CMetadataCompiler(pRC));

	ICryXML* const pCryXML = LoadICryXML();

	if (pCryXML == 0)
	{
		RCLogError("Loading xml library failed - not registering collada converter.");
	}
	else
	{
		pRC->RegisterConverter("CharacterCompiler", new CharacterCompiler(pCryXML));
		pRC->RegisterConverter("AnimationConverter", new CAnimationConverter(pCryXML, pRC->GetPakSystem(), pRC));
		pRC->RegisterConverter("ColladaCompiler", new ColladaCompiler(pCryXML, pRC->GetPakSystem()));
	}

	pRC->RegisterKey("createmtl","[DAE] 0=don't create .mtl files (default), 1=create .mtl files");

	pRC->RegisterKey("file","animation file for processing");	
	pRC->RegisterKey("report","report mode");	
	pRC->RegisterKey("SkipDba","skip build dba");	
	pRC->RegisterKey("animConfigFolder", "Path to a folder that contains SkeletonList.xml and DBATable.json");
	pRC->RegisterKey("checkloco",
		"should be used with report mode.\n"
		"Compare locomotion_locator motion with recalculated root motion");	
	pRC->RegisterKey("debugcompression","[I_CAF] show per-bone compression values during CAF-compression");
	pRC->RegisterKey("ignorepresets","[I_CAF] do not apply compression presets");
	pRC->RegisterKey("animSettingsFile","File to use instead of the default animation settings file");
	pRC->RegisterKey("cafAlignTracks", "[I_CAF] Apply padding to animation tracks to make the CAF suitable for in-place streaming");
	pRC->RegisterKey("dbaStreamPrepare", "[DBA] Prepare DBAs so they can be streamed in-place");

	pRC->RegisterKey("qtangents","0=use vectors to represent tangent space(default), 1=use quaternions");

	pRC->RegisterKey("vertexPositionFormat",
		"[CGF] Format of mesh vertex positions:\n"
		"f32 = 32-bit floating point (default)\n"
		"f16 = 16-bit floating point\n"
		"exporter = format specified in exporter");

	pRC->RegisterKey("vertexIndexFormat",
		"[CGF] Format of mesh vertex indices:\n"
		"u32 = 32-bit unsigned integer (default)\n"
		"u16 = 16-bit unsigned integer");

	pRC->RegisterKey("forceVCloth", "Force VCloth pre-preprocessing of mesh");

	pRC->RegisterKey("debugdump","[CGF] dump contents of source .cgf file instead of compiling it");
	pRC->RegisterKey("debugvalidate","[CGF, CHR] validate source file instead of compiling it");

	pRC->RegisterKey("targetversion","[chunk] Convert chunk file to the specified version\n"
		"0x745 = chunk data contain chunk headers\n"
		"0x746 = chunk data has no chunk headers (default)");

	pRC->RegisterKey("StripMesh","[CGF/CHR] Strip mesh chunks from output files\n"
		"0 = No stripping\n"
		"1 = Only strip mesh\n"
		"3 = [CHR] Treat input as a skin file, stripping all unnecessary chunks (including mesh)\n"
		"4 = [CHR] Treat input as a skel file, stripping all unnecessary chunks (including mesh)");
	pRC->RegisterKey("StripNonMesh","[CGF/CHR] Strip non mesh chunks from the output files");
	pRC->RegisterKey("CompactVertexStreams",
		"[CGF] Optimise vertex streams for streaming, by removing those that are unneeded for streaming,\n"
		"and packing those streams that are left into the format used internally by the engine.");
	pRC->RegisterKey("ComputeSubsetTexelDensity", "[CGF] Compute per-subset texel density");
	pRC->RegisterKey("SplitLODs","[CGF] Auto split LODs into the separate files");

	pRC->RegisterKey("maxWeightsPerVertex","[CHR] Maximum number of weights per vertex (default is 4)");

	pRC->RegisterKey("manifest",
		"[FBX] Specifies path to manifest file that will be created\n"
		"containing high-level information about the FBX file.\n"
		"For internal use only.\n"
		"Subject to change without prior notice.");
	pRC->RegisterKey("overwritesourcefile",
		"[FBX] Ignore source file specified in manifest and use this file instead.\n"
		"This does not overwrite the source filename written to the manifest.");

	pRC->RegisterKey("stripMetadata", "[cryasset] Strip metadata chunks from the output files");
	pRC->RegisterKey("assetTypes", "[cryasset] Use it to specify a mapping between file extensions end asset typenames.\n"
		"syntax is /assettypes=<pair[; pair[; pair[...]]]> where\n"
		"<pair> is <ext>,<assetTypeName>.\n"
		"Example: /assettypes=cgf,Geometry;chr,Character");
	pRC->RegisterKey("cryasset", "Use it to overwrite cryasset metadata.\n"
		"syntax is /cryasset=<pair[; pair[; pair[...]]]> where\n"
		"<pair> is <key>,<value>.\n"
		"Allowed keys:\n"
		"source : asset source filename\n"
		"Example: /cryasset=source,mytexture.tif");
}

#include <CryCore/Common_TypeInfo.h>
#include <Cry3DEngine/IIndexedMesh_info.h>
#include <Cry3DEngine/CGF/CGFContent_info.h>
