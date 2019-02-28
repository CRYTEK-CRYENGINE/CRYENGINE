// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// GnmShaderCompiler.cpp implements the shader compiler binary ran by RemoteShaderCompiler
// Essentially, it performs HLSL to PSSL conversion, then runs Wave PSSLC and writes out the SB and SDB files.
// The output files are in the vanilla format, so the SCE tools can be used on the input and output.

#include "GnmShaderCompiler.hpp"
#include "HlslParser.h"
#include "PsslGenerator.h"

#include <windows.h>

#include "../include/sdk_version.h"
#if !defined(SCE_ORBIS_SDK_VERSION) || SCE_ORBIS_SDK_VERSION != 0x04508001u
	#error This GnmShaderCompiler only support SDK 4.508.001
#endif

#include "shader/wave_psslc.h"
#pragma comment(lib, "libSceShaderWavePsslc.lib")

// Stream for error messages
#include <iostream>
static bool bOnlyStdOut = false;
static std::ostream& errorStream()
{
	return bOnlyStdOut ? std::cout : std::cerr;
}

static bool WriteFile(const char* szFile, const void* pData, size_t bytes)
{
	bool bResult = false;
	FILE* const hFile = fopen(szFile, "wb");
	if (hFile)
	{
		bResult = fwrite(pData, bytes, 1, hFile) == 1;
		fclose(hFile);
	}
	return bResult;
}

static bool WriteFile(const char* szFile, const std::string& contents)
{
	return WriteFile(szFile, contents.c_str(), contents.size());
}

// Entrypoint for the tool
int main(int argc, char* argv[])
{
	if (argc < 7)
	{
		errorStream() <<
		  "Usage: GnmShaderCompiler entryFunction (ignored) outputFile inputFile "
		  "-HwStage=V2P|V2H|V2G|V2L|C|C2V2P|C2I2P|H2D|H2M|D2P|M2P|G2C2P|L2C2P|P "
		  "-HwISA=B|C|N|R "                  // ISA flag (Base, Common, Neo, Neo /w RealTypes)
		  "[-PsColor=0xMASK] "               // MRT output format mask, one hex char per target
		  "[-PsDepth=BITS] "                 // Depth bits (either 0, 16 or 32)
		  "[-PsStencil=BITS] "               // Stencil bits (either 0 or 8)
		  "[--OnlyStdOut]|[-SO] "            // Redirect StdErr to StdOut (RemoteShaderCompiler does not read StdErr)
		  "[--ParserDump]|[-PD] "            // Dump parser tree to StdOut, useful for debugging parser logic
		  "[--RowMajorMatrixStorage][-RM] "  // Store matrices in row-major order by default
		  "[--UpgradeLegacySamplers][-ULS] " // Upgrade samplerXXX types to pair of SamplerState + TextureXXX
		  "[--DebugHelperFiles][-DHF] "      // Keep files on error
		  "\n";
		return -1;
	}

	bool bParserDebug = false;
	bool bRowMajorMatrices = false;
	bool bSupportLegacySamplers = false;
	bool bHelperFiles = false;
#ifdef _DEBUG
	bHelperFiles = true;
#endif
	const char* szEntryFunction = argv[1];
	const char* szOutput = argv[3];
	const char* szInput = argv[4];
	const char* szSdbCacheDir = getenv("PS4_SDB_CACHE_DIR");
	const char* szStage = nullptr;
	enum EHwTarget : char
	{
		kTargetUnknown = 0,
		kTargetBase    = 'B',
		kTargetCommon  = 'C',
		kTargetNeo     = 'N',
		kTargetNeoReal = 'R',
	} hwTarget = kTargetUnknown;
	uint32_t psColorMask = 0;
	uint8_t psDepthBits = 0;
	uint8_t psStencilBits = 0;

	for (int i = 5; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			char* pKey = argv[i];
			char* pValue = strchr(pKey + 1, '=');
			if (pKey && *pKey && pValue && *pValue)
			{
				*pValue++ = 0;
				if (_stricmp("-HwStage", pKey) == 0)
				{
					szStage = pValue;
				}
				else if (_stricmp("-HwISA", pKey) == 0)
				{
					hwTarget = (EHwTarget)pValue[0];
				}
				else if (_stricmp("-PsColor", pKey) == 0)
				{
					sscanf(pValue, "0x%x", &psColorMask);
				}
				else if (_stricmp("-PsDepth", pKey) == 0)
				{
					psDepthBits = atoi(pValue);
				}
				else if (_stricmp("-PsStencil", pKey) == 0)
				{
					psStencilBits = atoi(pValue);
				}
			}
			else if (pKey && *pKey)
			{
				if (_stricmp("--OnlyStdOut", pKey) == 0 || _stricmp("-SO", pKey) == 0)
				{
					bOnlyStdOut = true;
				}
				else if (_stricmp("--ParserDump", pKey) == 0 || _stricmp("-PD", pKey) == 0)
				{
					bParserDebug = true;
				}
				else if (_stricmp("--RowMajorMatrixStorage", pKey) == 0 || _stricmp("-RM", pKey) == 0)
				{
					bRowMajorMatrices = true;
				}
				else if (_stricmp("--UpgradeLegacySamplers", pKey) == 0 || _stricmp("-ULS", pKey) == 0)
				{
					bSupportLegacySamplers = true;
				}
				else if (_stricmp("--DebugHelperFiles", pKey) == 0 || _stricmp("-DHF", pKey) == 0)
				{
					bHelperFiles = true;
				}
			}
		}
	}

	if (szStage == nullptr || (hwTarget != kTargetBase && hwTarget != kTargetCommon && hwTarget != kTargetNeo && hwTarget != kTargetNeoReal))
	{
		errorStream() << "error: Invalid or missing -HwStage/-HwISA argument" << std::endl;
		return -1;
	}

	int profileId = -1;
	struct
	{
		const char*                             szKey;
		sce::Shader::Wave::Psslc::TargetProfile profile;
		HlslParser::EShaderType                 shaderType;
	} hwProfile[] =
	{
		// Key     SCE profile                                                  HLSL profile
		{ "P",     sce::Shader::Wave::Psslc::kTargetProfilePs,                  HlslParser::kShaderTypePixel    },
		{ "V2P",   sce::Shader::Wave::Psslc::kTargetProfileVsVs,                HlslParser::kShaderTypeVertex   },
		{ "V2H",   sce::Shader::Wave::Psslc::kTargetProfileVsLs,                HlslParser::kShaderTypeVertex   },
		{ "V2G",   sce::Shader::Wave::Psslc::kTargetProfileVsEs,                HlslParser::kShaderTypeVertex   },
		{ "V2L",   sce::Shader::Wave::Psslc::kTargetProfileVsEsOnChip,          HlslParser::kShaderTypeVertex   },
		{ "C2V2P", sce::Shader::Wave::Psslc::kTargetProfileDdTriCull,           HlslParser::kShaderTypeVertex   },
		{ "C2I2P", sce::Shader::Wave::Psslc::kTargetProfileDdTriCullInstancing, HlslParser::kShaderTypeVertex   },
		{ "G2C2P", sce::Shader::Wave::Psslc::kTargetProfileGs,                  HlslParser::kShaderTypeGeometry },
		{ "L2C2P", sce::Shader::Wave::Psslc::kTargetProfileGsOnChip,            HlslParser::kShaderTypeGeometry },
		{ "H2D",   sce::Shader::Wave::Psslc::kTargetProfileHs,                  HlslParser::kShaderTypeHull     },
		{ "H2M",   sce::Shader::Wave::Psslc::kTargetProfileHsOffChip,           HlslParser::kShaderTypeHull     },
		{ "D2P",   sce::Shader::Wave::Psslc::kTargetProfileDsVs,                HlslParser::kShaderTypeDomain   },
		{ "M2P",   sce::Shader::Wave::Psslc::kTargetProfileDsVsOffChip,         HlslParser::kShaderTypeDomain   },
		{ "C",     sce::Shader::Wave::Psslc::kTargetProfileCs,                  HlslParser::kShaderTypeCompute  },
	};
	for (int i = 0; i < sizeof(hwProfile) / sizeof(*hwProfile); ++i)
	{
		if (_stricmp(szStage, hwProfile[i].szKey) == 0)
		{
			profileId = i;
			break;
		}
	}
	if (profileId < 0)
	{
		errorStream() << "error: Failed to parse -HwStage=" << szStage << std::endl;
		return -1;
	}

	sce::Shader::Wave::Psslc::Options options;
	sce::Shader::Wave::Psslc::CallbackList callbacks;
	bool bOk = sce::Shader::Wave::Psslc::initializeOptions(&options, SCE_WAVE_API_VERSION) == SCE_WAVE_RESULT_OK;
	bOk = bOk && sce::Shader::Wave::Psslc::initializeCallbackList(&callbacks, sce::Shader::Wave::Psslc::kCallbackDefaultsSystemFiles) == SCE_WAVE_RESULT_OK;
	if (!bOk)
	{
		errorStream() << "error: Failed to initialize PSSLC library" << std::endl;
		return -1;
	}

	// Set up options
	options.inlinefetchshader = 1;
	options.maxEudCount = GnmShaderCompiler::kNumExtendedUserDataRegisters;
	options.maxUserDataRegs = GnmShaderCompiler::kNumUserDataRegisters;
	options.psMrtFormat = psColorMask;
	options.disablePicLiteralBuffer = 1;
	options.optimizationLevel = 4;
	options.hardwareTarget = hwTarget == kTargetBase ? sce::Shader::Wave::Psslc::kHardwareTargetBase : hwTarget == kTargetCommon ? sce::Shader::Wave::Psslc::kHardwareTargetCommon : sce::Shader::Wave::Psslc::kHardwareTargetNeo;
	options.realTypes = hwTarget == kTargetNeoReal;
	options.targetProfile = hwProfile[profileId].profile;
	options.entryFunctionName = szEntryFunction;
	options.sdbCache = (szSdbCacheDir != nullptr);

	/*
	   options.macroDefinitionCount = 0;
	   uint32_t suppressedWarnings[] = { 5203, 5206, 5581 }; // Unreferenced x2 + half is full
	   options.suppressedWarnings = suppressedWarnings;
	   options.suppressedWarningsCount = sizeof(suppressedWarnings) / sizeof(suppressedWarnings[0]);
	 */

	const std::string patchedFile = std::string(szInput) + ".GnmCE.pssl";
	if (strstr(szInput, ".pssl"))
	{
		// Already has a PSSL extension, so no conversion needed
		options.mainSourceFile = szInput;
	}
	else
	{
		// Need to do HLSL conversion
		options.mainSourceFile = patchedFile.c_str();

		// HLSL-to-PSSL
		auto hlsl = HlslParser::SProgram::Create(szInput, hwProfile[profileId].shaderType, bParserDebug);
		if (!hlsl.errorMessage.empty())
		{
			if (bHelperFiles)
			{
				const std::string errorFile = std::string(szInput) + ".GnmPE.hlsl";
				::CopyFileA(szInput, errorFile.c_str(), false);
			}
			errorStream() << hlsl.errorMessage << std::endl;
			return -1;
		}
		SPsslConversion conversion;
		conversion.bIndentWithTabs = true;
		conversion.bMatrixStorageRowMajor = bRowMajorMatrices;
		conversion.bSupportLegacySamplers = bSupportLegacySamplers;
		conversion.bSupportSmallTypes = (hwTarget == kTargetNeoReal);
		conversion.shaderType = hwProfile[profileId].shaderType;
		conversion.entryPoint = szEntryFunction;
		char buf[11] = "<unknown>";
#if defined(SCE_ORBIS_SDK_VERSION) && SCE_ORBIS_SDK_VERSION < 0x10000000U
		{
			sprintf_s(buf, "%07X", SCE_ORBIS_SDK_VERSION);
			buf[9] = 0;
			buf[8] = buf[6];
			buf[7] = buf[5];
			buf[6] = buf[4];
			buf[5] = '.';
			buf[4] = buf[3];
			buf[3] = buf[2];
			buf[2] = buf[1];
			buf[1] = '.';
			buf[0] = buf[0];
		}
#endif
		conversion.comment = "/*with Wave-Psslc version: " + std::to_string(options.version) + " / PS4 SDK " + buf + "*/\n";
		conversion.comment = conversion.comment + "/*entrypoint=" + szEntryFunction + (bRowMajorMatrices ? ", storage=row-major" : ", storage=column-major") + (bSupportLegacySamplers ? ", DX9-support enabled*/\n" : "*/\n");
		if (hwProfile[profileId].profile == sce::Shader::Wave::Psslc::kTargetProfilePs)
		{
			sprintf_s(buf, "0x%08X", psColorMask);
			conversion.comment += "/*pipeline: -HwStage=P -HwISA=" + std::string(1, (char)hwTarget) + " -PsColor=" + buf + " -PsDepth=" + std::to_string(psDepthBits) + " -PsStencil=" + std::to_string(psStencilBits) + "*/";
		}
		else
		{
			conversion.comment += "/*pipeline: -HwStage=" + std::string(hwProfile[profileId].szKey) + " -HwISA=" + std::string(1, (char)hwTarget) + "*/";
		}
		auto pssl = ToPssl(hlsl, conversion);
		if (pssl.front() != '/')
		{
			if (bHelperFiles)
			{
				std::string errorFile = std::string(szInput) + ".GnmGE.hlsl";
				::CopyFileA(szInput, errorFile.c_str(), false);
			}
			errorStream() << pssl << std::endl;
			return -1;
		}

		if (!WriteFile(options.mainSourceFile, pssl))
		{
			errorStream() << "error (I/O): Cannot write generated PSSL to " << options.mainSourceFile << std::endl;
			return -1;
		}
	}

	// PSSLC step
	bool bSuccess = false;
	const sce::Shader::Wave::Psslc::Output* pOutput = sce::Shader::Wave::Psslc::run(&options, &callbacks);
	if (pOutput)
	{
		for (int i = 0; i < pOutput->diagnosticCount; i++)
		{
			const sce::Shader::Wave::Psslc::DiagnosticMessage& message = pOutput->diagnostics[i];

			if (message.level == sce::Shader::Wave::Psslc::kDiagnosticLevelWarning)
				errorStream() << "warning (Wave): " << message.message << std::endl;
			else if (message.level == sce::Shader::Wave::Psslc::kDiagnosticLevelError)
				errorStream() << "error (Wave): " << message.message << std::endl;
			else
				errorStream() << "info (Wave): " << message.message << std::endl;
		}

		if (pOutput->programData && pOutput->programSize)
		{
			bSuccess = WriteFile(szOutput, pOutput->programData, pOutput->programSize);
		}

		if (bSuccess && szSdbCacheDir && pOutput->sdbData && pOutput->sdbDataSize)
		{
			const std::string sdbFile = std::string(szSdbCacheDir) + "\\" + pOutput->sdbExt;
			WriteFile(sdbFile.c_str(), pOutput->sdbData, pOutput->sdbDataSize);
		}

		sce::Shader::Wave::Psslc::destroyOutput(pOutput);
	}
	if (!bHelperFiles || bSuccess)
	{
		::DeleteFile(options.mainSourceFile);
	}
	else if (!bSuccess)
	{
		const std::string errorFile = std::string(szInput) + ".GnmIN.hlsl";
		::CopyFileA(szInput, errorFile.c_str(), false);
	}
	return bSuccess ? 0 : -1;
}
