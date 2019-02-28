// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#pragma comment(lib, "D3Dcompiler.lib")

#define CRYFXC_VER "1.01"


enum SwitchType
{
	FXC_E, FXC_T, FXC_Help, FXC_CmdOptFile, FXC_Cc, FXC_Compress, FXC_D, FXC_Decompress, FXC_Fc,
	FXC_Fh, FXC_Fo, FXC_Fx, FXC_P, FXC_Gch, FXC_Gdp, FXC_Gec, FXC_Ges, FXC_Gfa, FXC_Gfp, FXC_Gis,
	FXC_Gpp, FXC_I, FXC_LD, FXC_Ni, FXC_NoLogo, FXC_Od, FXC_Op, FXC_O0, FXC_O1, FXC_O2, FXC_O3,
	FXC_Vd, FXC_Vi, FXC_Vn, FXC_Zi, FXC_Zpc, FXC_Zpr,

	FXC_NumArgs
};


struct SwitchEntry
{
	SwitchType type;
	const char* text;
	bool hasValue;
	bool supported;
};


const static SwitchEntry s_switchEntries[] =
{
	{FXC_E, "/E", 1, true},
	{FXC_T, "/T", 1, true},
	{FXC_Fh, "/Fh", 1, true},
	{FXC_Fo, "/Fo", 1, true},

	{FXC_Gec, "/Gec", 0, true},
	{FXC_Ges, "/Ges", 0, true},
	{FXC_Gfa, "/Gfa", 0, true},
	{FXC_Gfp, "/Gfp", 0, true},
	{FXC_Gis, "/Gis", 0, true},
	{FXC_Gpp, "/Gpp", 0, true},
	{FXC_Od, "/Od", 0, true},
	{FXC_O0, "/O0", 0, true},
	{FXC_O1, "/O1", 0, true},
	{FXC_O2, "/O2", 0, true},
	{FXC_O3, "/O3", 0, true},
	{FXC_Op, "/Op", 0, true},
	{FXC_Vd, "/Vd", 0, true},
	{FXC_Vn, "/Vn", 1, true},
	{FXC_Zi, "/Zi", 0, true},
	{FXC_Zpc, "/Zpc", 0, true},
	{FXC_Zpr, "/Zpr", 0, true},
	{FXC_NoLogo, "/nologo", 0, true},

	{FXC_Help, "/?", 0, false},
	{FXC_Help, "/help", 0, false},
	{FXC_Cc, "/Cc", 0, false},
	{FXC_Compress, "/compress", 0, false},
	{FXC_D, "/D", 1, false},
	{FXC_Decompress, "/decompress", 0, false},
	{FXC_Fc, "/Fc", 1, false},
	{FXC_Fx, "/Fx", 1, false},
	{FXC_P, "/P", 1, false},
	{FXC_Gch, "/Gch", 0, false},
	{FXC_Gdp, "/Gdp", 0, false},
	{FXC_I, "/I", 1, false},
	{FXC_LD, "/LD", 0, false},
	{FXC_Ni, "/Ni", 0, false},
	{FXC_Vi, "/Vi", 0, false}
};


bool IsSwitch(const char* p)
{
	assert(p);
	return *p == '/' || *p == '@';
}


const SwitchEntry* GetSwitch(const char* p)
{
	assert(p);
	for (size_t i=0; i<sizeof(s_switchEntries) / sizeof(s_switchEntries[0]); ++i)
	{
		if (_stricmp(s_switchEntries[i].text, p) == 0)
			return &s_switchEntries[i];
	}

	if (*p == '@')
	{
		const static SwitchEntry sw = {FXC_CmdOptFile, "@", 0, false};
		return &sw;
	}

	return 0;
}


struct ParserResults
{
	const char* pProfile;
	const char* pEntry;
	const char* pOutFile;
	const char* pInFile;
	const char* pHeaderVariableName;
	unsigned int compilerFlags;
	bool disassemble;

	void Init()
	{
		pProfile = 0;
		pEntry = 0;
		pOutFile = 0;
		pInFile = 0;
		pHeaderVariableName = 0;
		compilerFlags = 0;
		disassemble = false;
	}
};


bool ParseCommandLine(const char* const* args, size_t numargs, ParserResults& parserRes)
{
	parserRes.Init();

	if (numargs < 4)
	{
		fprintf(stderr, "Failed to specify all required arguments: infile, outfile, profile and entry point\n");
		return false;
	}

	for (size_t i=1; i<numargs; ++i)
	{
		if (IsSwitch(args[i]))
		{
			const SwitchEntry* sw = GetSwitch(args[i]);
			if (!sw)
			{
				fprintf(stderr, "Unknown switch: %s\n", args[i]);
				return false;
			}
			
			if (!sw->supported)
			{
				fprintf(stderr, "Unsupported switch: %s\n", sw->text);
				return false;
			}

			if (sw->hasValue)
			{
				if (i+1 == numargs || IsSwitch(args[i+1]))
				{
					fprintf(stderr, "Missing value for switch: %s\n", sw->text);
					return false;
				}

				const char* pValue = args[i+1];
				switch (sw->type)
				{
				case FXC_E:
					parserRes.pEntry = pValue;
					break;
				case FXC_T:
					parserRes.pProfile = pValue;
					break;
				case FXC_Fh:
					parserRes.pOutFile = pValue;
					parserRes.disassemble = true;
					break;
				case FXC_Fo:
					parserRes.pOutFile = pValue;
					break;
				case FXC_Vn:
					parserRes.pHeaderVariableName = pValue;
					break;
				default:
					fprintf(stderr, "Failed assigning switch: %s | value: %s\n", sw->text, pValue);
					return false;
				}

				++i;
			}
			else
			{
				switch (sw->type)
				{
				case FXC_Gec:
					parserRes.compilerFlags |= D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
					break;
				case FXC_Od:
					parserRes.compilerFlags |= D3D10_SHADER_SKIP_OPTIMIZATION;
					break;
				case FXC_O0:
					parserRes.compilerFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL0;
					break;
				case FXC_O1:
					parserRes.compilerFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL1;
					break;
				case FXC_O2:
					parserRes.compilerFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL2;
					break;
				case FXC_O3:
					parserRes.compilerFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL3;
					break;
				case FXC_Zi:
					parserRes.compilerFlags |= D3D10_SHADER_DEBUG;
					break;
				case FXC_Zpc:
					parserRes.compilerFlags |= D3D10_SHADER_PACK_MATRIX_COLUMN_MAJOR;
					break;
				case FXC_Zpr:
					parserRes.compilerFlags |= D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
					break;
				case FXC_Ges:
					parserRes.compilerFlags |= D3D10_SHADER_ENABLE_STRICTNESS;
					break;
				case FXC_Gfa:
					parserRes.compilerFlags |= D3D10_SHADER_AVOID_FLOW_CONTROL;
					break;
				case FXC_Gfp:
					parserRes.compilerFlags |= D3D10_SHADER_PREFER_FLOW_CONTROL;
					break;
				case FXC_Gis:
					parserRes.compilerFlags |= D3D10_SHADER_IEEE_STRICTNESS;
					break;
				case FXC_Gpp:
					parserRes.compilerFlags |= D3D10_SHADER_PARTIAL_PRECISION;
					break;
				case FXC_Op:
					parserRes.compilerFlags |= D3D10_SHADER_NO_PRESHADER;
					break;
				case FXC_Vd:
					parserRes.compilerFlags |= D3D10_SHADER_SKIP_VALIDATION;
					break;
				case FXC_NoLogo:
					break;
				default:
					fprintf(stderr, "Failed assigning switch: %s\n", sw->text);
					return false;
				}
			}
		}
		else if (i == numargs - 1)
		{
			parserRes.pInFile = args[i];
		}
		else
		{
			fprintf(stderr, "Error in command line at token: %s\n", args[i]);
			return false;
		}
	}

	const bool successful = parserRes.pProfile && parserRes.pEntry && parserRes.pInFile && parserRes.pOutFile;
	if (!successful)
	{
		fprintf(stderr, "Failed to specify all required arguments: infile, outfile, profile and entry point\n");
	}

	return successful;
}


bool ReadInFile(const char* pInFile, std::vector<char>& data)
{
	if (!pInFile)
		return false;

	bool read = false;

	FILE* fin =0;
	fopen_s(&fin, pInFile, "rb");
	if (fin)
	{
		fseek(fin, 0, SEEK_END);
		const long l = ftell(fin);
		if (l >= 0)
		{
			fseek(fin, 0, SEEK_SET);
			const size_t len = l > 0 ? (size_t) l : 0;
			data.resize(len);
			fread(&data[0], 1, len, fin);
			read = true;
		}

		fclose(fin);
	}

	return read;
}


bool WriteByteCode(const char* pFileName, const void* pCode, size_t codeSize)
{
	if (!pFileName || !pCode && codeSize)
		return false;

	bool written = false;

	FILE* fout =0;
	fopen_s(&fout, pFileName, "wb");
	if (fout)
	{
		fwrite(pCode, 1, codeSize, fout);
		fclose(fout);
		written = true;
	}

	return written;
}


bool WriteHexListing(const char* pFileName, const char* pHdrVarName, const char* pDisassembly, const void* pCode, size_t codeSize)
{
	if (!pFileName || !pHdrVarName || !pDisassembly || !pCode && codeSize)
		return false;

	bool written = false;

	FILE* fout =0;
	fopen_s(&fout, pFileName, "w");
	if (fout)
	{
		fprintf(fout, "#if 0\n%s#endif\n\n", pDisassembly);
		fprintf(fout, "const BYTE g_%s[] = \n{", pHdrVarName);

		const size_t blockSize = 6;
		const size_t numBlocks = codeSize / blockSize;

		const unsigned char* p = (const unsigned char*) pCode;

		size_t i = 0;
		for (; i < numBlocks * blockSize; i += blockSize)
		{
			fprintf(fout, "\n    %3d, %3d, %3d, %3d, %3d, %3d", p[i], p[i+1], p[i+2], p[i+3], p[i+4], p[i+5]);
			if (i + blockSize < codeSize)
				fprintf(fout, ",");
		}

		if (i < codeSize)
		{
			fprintf(fout, "\n    ");

			for (; i < codeSize; ++i)
			{
				fprintf(fout, "%3d", p[i]);
				if (i < codeSize - 1)
					fprintf(fout, ", ");
			}
		}

		fprintf(fout, "\n};\n");

		fclose(fout);
		written = true;
	}

	return written;
}


void DisplayInfo()
{
	fprintf(stdout, "FXC stub for remote shader compile server\n(C) 2012 Crytek. All rights reserved.\n\nVersion "CRYFXC_VER" for %d bit, linked against D3DCompiler_%d.dll\n\n", sizeof(void*) * 8, D3DX11_SDK_VERSION);
	fprintf(stdout, "Syntax: fxc SwitchOptions Filename\n\n");
	fprintf(stdout, "Supported switches: ");

	bool firstSw = true;
	for (size_t i=0; i<sizeof(s_switchEntries)/sizeof(s_switchEntries[0]); ++i)
	{
		if (s_switchEntries[i].supported)
		{
			fprintf(stdout, "%s%s", firstSw ? "" : ", ", s_switchEntries[i].text);
			firstSw = false;
		}
	}

	fprintf(stdout, "\n");
}


int _tmain(int argc, _TCHAR* argv[])
{
	if (argc == 1)
	{
		DisplayInfo();
		return 0;
	}

	ParserResults parserRes;
	if (!ParseCommandLine(argv, argc, parserRes))
		return 1;

	std::vector<char> program;
	if (!ReadInFile(parserRes.pInFile, program))
	{
		fprintf(stderr, "Failed to read input file: %s\n", parserRes.pInFile);
		return 1;
	}

	ID3D10Blob* pShader = 0;
	ID3D10Blob* pErr = 0;

	bool successful = SUCCEEDED(D3DCompile(&program[0], program.size(), parserRes.pInFile, 0, 0, parserRes.pEntry, parserRes.pProfile, parserRes.compilerFlags, 0, &pShader, &pErr)) && pShader;

	if (successful)
	{
		const unsigned char* pCode = (unsigned char*) pShader->GetBufferPointer();
		const size_t codeSize = pShader->GetBufferSize();

		if (!parserRes.disassemble)
		{
			successful = WriteByteCode(parserRes.pOutFile, pCode, codeSize);
			if (!successful)
			{
				fprintf(stderr, "Failed to write output file: %s\n", parserRes.pOutFile);
			}
		}
		else
		{
			ID3D10Blob* pDisassembled = 0;
			successful = SUCCEEDED(D3DDisassemble(pCode, codeSize, 0, 0, &pDisassembled)) && pDisassembled;

			if (successful)
			{
				const char* pDisassembly = (char*) pDisassembled->GetBufferPointer();
				const char* pHdrVarName = parserRes.pHeaderVariableName ? parserRes.pHeaderVariableName : parserRes.pEntry;
				successful = WriteHexListing(parserRes.pOutFile, pHdrVarName, pDisassembly, pCode, codeSize);
				if (!successful)
				{
					fprintf(stderr, "Failed to write output file: %s\n", parserRes.pOutFile);
				}
			}
			else
			{
				fprintf(stderr, "Failed to disassemble shader code\n", parserRes.pOutFile);
			}

			if (pDisassembled)
			{
				pDisassembled->Release();
				pDisassembled = 0;
			}
		}
	}
	else
	{
		if (pErr)
		{
			const char* pMsg = (const char*) pErr->GetBufferPointer();
			fprintf(stderr, "%s\n", pMsg);
		}
	}

	if (pShader)
	{
		pShader->Release();
		pShader = 0;
	}

	if (pErr)
	{
		pErr->Release();
		pErr = 0;
	}

	return successful ? 0 : 1;
}
