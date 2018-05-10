// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <windows.h>
#include <vector>
#include "Log.h"
#include "DXBC.h"
#include "CompilerData.h"
#include "GLSLTranspiler.h"
#include "..\HLSLCrossCompiler\include\hlslcc.hpp"

bool ShellExecute(const std::string& file, const std::string& parameters)
{
	HANDLE childStdInRead;
	HANDLE childStdInWrite;
	HANDLE childStdOutRead;
	HANDLE childStdOutWrite;

	SECURITY_ATTRIBUTES securityAttributes;
	securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	securityAttributes.bInheritHandle = true;
	securityAttributes.lpSecurityDescriptor = nullptr;

	CreatePipe(&childStdInRead, &childStdInWrite, &securityAttributes, 0);
	SetHandleInformation(childStdInWrite, HANDLE_FLAG_INHERIT, 0);

	CreatePipe(&childStdOutRead, &childStdOutWrite, &securityAttributes, 0);
	SetHandleInformation(childStdOutRead, HANDLE_FLAG_INHERIT, 0);

	PROCESS_INFORMATION processInfo = { 0 };
	STARTUPINFO startUpInfo = { 0 };

	startUpInfo.cb = sizeof(STARTUPINFO);
	startUpInfo.hStdInput = childStdInRead;
	startUpInfo.hStdOutput = childStdOutWrite;
	startUpInfo.hStdError = childStdOutWrite;
	startUpInfo.dwFlags |= STARTF_USESTDHANDLES;

	char parametersTemp[4096];
	sprintf_s(parametersTemp, 4096, "%s %s", file.c_str(), parameters.c_str());

	std::string output;

	CreateProcess(nullptr, parametersTemp, nullptr, nullptr, true, 0, nullptr, nullptr, &startUpInfo, &processInfo);
	while (WaitForSingleObject(processInfo.hProcess, 100) == WAIT_TIMEOUT)
	{
		unsigned long size = GetFileSize(childStdOutRead, nullptr);

		if (size != 0)
		{
			std::string outputTemp;
			outputTemp.resize(size);

			unsigned long readBytes;
			ReadFile(childStdOutRead, &outputTemp[0], size, &readBytes, nullptr);

			output += outputTemp;
		}
	}

	unsigned long size = GetFileSize(childStdOutRead, nullptr);

	if (size != 0)
	{
		std::string outputTemp;
		outputTemp.resize(size);

		unsigned long readBytes;
		ReadFile(childStdOutRead, &outputTemp[0], size, &readBytes, nullptr);

		output += outputTemp;
	}

	unsigned long exitCode;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);

	if (exitCode != 0)
	{
		// convert 'ERROR' to 'error' as remote compiler 
		// doesn't report upper case version
		size_t curPos = output.find("ERROR:", 0);
		while (curPos != std::string::npos)
		{
			output.replace(curPos, 5, "error");
			curPos = output.find("ERROR:", curPos + 1);
		}

		DumpError(output.c_str());
	}

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	CloseHandle(childStdInRead);
	CloseHandle(childStdInWrite);
	CloseHandle(childStdOutRead);
	CloseHandle(childStdOutWrite);

	return exitCode == 0;
}

bool RunHLSLcc(const SCompilerData& compilerData, bool remoteCompile)
{
	std::string sourceFilename = compilerData.filename + SourceFileExtension;
	std::string outputFilename = compilerData.filename + DXBCFileExtension;

	//LogWrite("Converting: %s (entry: %s, profile: %s)\n", sourceFilename.c_str(), compilerData.entryName.c_str(), compilerData.profileName.c_str());
	//__debugbreak();

	char flags[16];
	sprintf_s(flags, "%d",
		HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT |
		HLSLCC_FLAG_INVERT_CLIP_SPACE_Y |
		HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS |
		HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING |
		HLSLCC_FLAG_NO_VERSION_STRING |
		HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES |
		HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS |
		HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS);

	if (remoteCompile)
	{
		std::string parameters = std::string("-lang=440 -flags=") + flags + " -fxc=\"..\\..\\PCD3D11\\v007\\fxc.exe /nologo /E " + compilerData.request.entryName + " /T " + compilerData.request.profileName + " /Zpr /Gec /Fo\" -out=\"" + outputFilename + "\" -in=\"" + sourceFilename + "\"";
		return ShellExecute("..\\..\\PCGL\\V013\\HLSLcc.exe", parameters);
	}
	else
	{
		std::string parameters = std::string("-lang=440 -flags=") + flags + " -fxc=\"..\\..\\Tools\\RemoteShaderCompiler\\Compiler\\PCD3D11\\v007\\fxc.exe /nologo /E " + compilerData.request.entryName + " /T " + compilerData.request.profileName + " /Zpr /Gec /Fo\" -out=\"" + outputFilename + "\" -in=\"" + sourceFilename + "\"";
		return ShellExecute("..\\..\\Tools\\RemoteShaderCompiler\\Compiler\\PCGL\\V013\\HLSLcc.exe", parameters);
	}
}

bool RunGlslangValidator(const SCompilerData& compilerData, const std::string& outputFilename, bool remoteCompile)
{
	std::string sourceFilename = compilerData.filename + GLSLTranspiler::GetFileExtension(compilerData.request.shaderStage);
	std::string parameters = std::string("-V ") + "\"" + sourceFilename + "\" -o \"" + outputFilename + "\"";

	if (remoteCompile)
	{
		return ShellExecute("..\\..\\SPIRV\\V001\\glslangValidator.exe", parameters);
	}
	else
	{
		return ShellExecute("..\\..\\Tools\\RemoteShaderCompiler\\Compiler\\SPIRV\\V001\\glslangValidator.exe", parameters);
	}
}

#include <iostream>
bool ProcessFile(const std::string& dataFilename, const std::string& outputFilename)
{
	SCompilerData compilerData;
	compilerData.filename = dataFilename.substr(0, dataFilename.size() - SourceFileExtension.length());

	if (!SRequestData::Load(dataFilename, compilerData.request))
	{
		ReportError("error: Cannot load shader compilation request description (%s).\n", compilerData.filename.c_str());
		return false;
	}

	if (!RunHLSLcc(compilerData, outputFilename.length() != 0))
	{
		ReportError("error: HLSLcc failed (%s).\n", compilerData.filename.c_str());
		return false;
	}

	if (!DXBC::Load(compilerData))
	{
		ReportError("error: Cannot load DXBC (%s).\n", compilerData.filename.c_str());
		return false;
	}

	if (!GLSLReflection::ProcessDXBC(compilerData))
	{
		ReportError("error: Cannot process DXBC (%s).\n", compilerData.filename.c_str());
		return false;
	}

	std::string vulkanGLSL;
	if (!GLSLTranspiler::TranspileToVulkanGLSL(compilerData, vulkanGLSL))
	{
		ReportError("error: Transpiler failed (%s).\n", compilerData.filename.c_str());
		return false;
	}

	if (!GLSLTranspiler::SaveVulkanGLSL(compilerData, vulkanGLSL))
	{
		ReportError("error: Cannot save VulkanGLSL file (%s).\n", compilerData.filename.c_str());
		return false;
	}

	if (!RunGlslangValidator(compilerData, compilerData.filename + OutFileExtension, outputFilename.length() != 0))
	{
		ReportError("error: SPIRV compilation failed.\n", compilerData.filename.c_str());
		return false;
	}

	return true;
}

bool ProcessFiles(const std::string& path, const std::string& name)
{
	WIN32_FIND_DATA findFileData;
	HANDLE hFindFile = FindFirstFile((path + "\\" + name).c_str(), &findFileData);

	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (findFileData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY && strcmp(findFileData.cFileName, "..") && strcmp(findFileData.cFileName, "."))
			{
				if (!ProcessFiles(path + "\\" + findFileData.cFileName, name))
					return false;
			}
			else if (strlen(findFileData.cFileName) > SourceFileExtension.length() && !strcmp(findFileData.cFileName + strlen(findFileData.cFileName) - SourceFileExtension.length(), SourceFileExtension.c_str()))
			{
				if (!ProcessFile(path + "\\" + findFileData.cFileName, ""))
					return false;
			}
		}
		while (FindNextFile(hFindFile, &findFileData));

		FindClose(hFindFile);
		return true;
	}

	return false;
}

#define ERROR_COMPILATION_FAILED 1

int main(int argc, char** argv)
{
	if (argc == 1)
	{
		if (!ProcessFiles(".", "*.*"))
			return ERROR_COMPILATION_FAILED;
	}
	else if (argc == 5)
	{
		std::string inputFilename = argv[4];
		std::string outputFilename = argv[3];
		if (!ProcessFile(inputFilename, outputFilename))
			return ERROR_COMPILATION_FAILED;
	}
	else
	{
		ReportError("error: Invalid number of parameters");
		return ERROR_COMPILATION_FAILED;
	}

	return 0;
}
