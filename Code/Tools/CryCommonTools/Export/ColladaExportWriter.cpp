// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ColladaExportWriter.h"
#include "ColladaWriter.h"
#include "IExportSource.h"
#include "PathHelpers.h"
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <CryString/CryPath.h>
#include "IExportContext.h"
#include "ProgressRange.h"
#include "XMLWriter.h"
#include "XMLPakFileSink.h"
#include "ISettings.h"
#include "SingleAnimationExportSourceAdapter.h"
#include "GeometryExportSourceAdapter.h"
#include "ModelData.h"
#include "MaterialData.h"
#include "GeometryFileData.h"
#include "FileUtil.h"
#include "ModuleHelpers.h"
#include "PropertyHelpers.h"
#include "StringHelpers.h"
#include <ctime>
#include <list>

namespace
{
	class ResourceCompilerLogListener
		: public IResourceCompilerListener
	{
	public:
		explicit ResourceCompilerLogListener(IExportContext* context)
			: m_context(context)
		{
		}

		virtual void OnRCMessage(IResourceCompilerListener::MessageSeverity severity, const char* text)
		{
			ILogger::ESeverity outSeverity;
			switch (severity)
			{
			case IResourceCompilerListener::MessageSeverity_Debug:
			case IResourceCompilerListener::MessageSeverity_Info:   // normal RC text should just be debug
				outSeverity = ILogger::eSeverity_Debug;
				break;
			case IResourceCompilerListener::MessageSeverity_Warning:
				outSeverity = ILogger::eSeverity_Warning;
				break;
			case IResourceCompilerListener::MessageSeverity_Error:
				outSeverity = ILogger::eSeverity_Error;
				break;
			default:
				outSeverity = ILogger::eSeverity_Error;
				break;
			}

			m_context->Log(outSeverity, "%s", text);
		}

	private:
		IExportContext* m_context;
	};
}

void ColladaExportWriter::Export(IExportSource* source, IExportContext* context)
{
	// Create an object to report on our progress to the export context.
	ProgressRange progressRange(context, &IExportContext::SetProgress);

	// Log build information.
	context->Log(ILogger::eSeverity_Info, "Exporter build created on " __DATE__);

#if defined(_DEBUG)
	context->Log(ILogger::eSeverity_Info, "******DEBUG BUILD******");
#else //_DEBUG
	context->Log(ILogger::eSeverity_Info, "Release build.");
#endif //_DEBUG

	context->Log(ILogger::eSeverity_Debug, "Bit count == %d.", (sizeof(void*) * 8));

	std::string exePath = StringHelpers::ConvertString<string>(ModuleHelpers::GetCurrentModulePath(ModuleHelpers::CurrentModuleSpecifier_Executable));
	context->Log(ILogger::eSeverity_Debug, "Application path: %s", exePath.c_str());

	std::string exporterPath = StringHelpers::ConvertString<string>(ModuleHelpers::GetCurrentModulePath(ModuleHelpers::CurrentModuleSpecifier_Library));
	context->Log(ILogger::eSeverity_Debug, "Exporter path: %s", exporterPath.c_str());

	bool const bExportCompressed = (GetSetting<int>(context->GetSettings(), "ExportCompressedCOLLADA", 1)) != 0;
	context->Log(ILogger::eSeverity_Debug, "ExportCompressedCOLLADA key: %d", (bExportCompressed ? 1 : 0));

	std::string const exportExtension = bExportCompressed ? ".dae.zip" : ".dae";

	// Log the start time.
	{
		char buf[1024];
		std::time_t t = std::time(0);
		std::strftime(buf, sizeof(buf) / sizeof(buf[0]), "%H:%M:%S on %a, %d/%m/%Y", std::localtime(&t));
		context->Log(ILogger::eSeverity_Info, "Export begun at %s", buf);
	}

	// Select the name of the directory to export to.
	std::string const originalExportDirectory = source->GetExportDirectory();
	if (originalExportDirectory.empty())
	{
		throw IExportContext::NeedSaveError("Scene must be saved before exporting.");
	}

	GeometryFileData geometryFileData;
	std::vector<std::string> colladaGeometryFileNameList;
	std::vector<std::string> assetGeometryFileNameList;
	typedef std::vector<std::pair<std::pair<int, int>, std::string> > AnimationFileNameList;
	AnimationFileNameList animationFileNameList;
	{
		CurrentTaskScope currentTask(context, "dae");

		// Choose the files to which to export all the animations.
		std::list<SingleAnimationExportSourceAdapter> animationExportSources;
		std::list<GeometryExportSourceAdapter> geometryExportSources;
		typedef std::vector<std::pair<std::string, IExportSource*> > ExportList;
		ExportList exportList;
		std::vector<int> geometryFileIndices;
		{
			ProgressRange readProgressRange(progressRange, 0.2f);

			source->ReadGeometryFiles(context, &geometryFileData);

			for (int geometryFileIndex = 0; geometryFileIndex < geometryFileData.GetGeometryFileCount(); ++geometryFileIndex)
			{
				IGeometryFileData::SProperties properties = geometryFileData.GetProperties(geometryFileIndex);

				if (properties.filetypeInt & CRY_FILE_TYPE_CAF)
				{
					properties.filetypeInt &= ~CRY_FILE_TYPE_CAF;
					properties.filetypeInt |= CRY_FILE_TYPE_INTERMEDIATE_CAF;
					geometryFileData.SetProperties(geometryFileIndex, properties);
				}
			}

			for (int geometryFileIndex = 0; geometryFileIndex < geometryFileData.GetGeometryFileCount(); ++geometryFileIndex)
			{
				const IGeometryFileData::SProperties& properties = geometryFileData.GetProperties(geometryFileIndex);

				const std::string geometryFileName = geometryFileData.GetGeometryFileName(geometryFileIndex);

				if (!geometryFileName.empty() && properties.filetypeInt != CRY_FILE_TYPE_INTERMEDIATE_CAF)
				{
					geometryFileIndices.push_back(geometryFileIndex);
				}
			}

			if (!geometryFileIndices.empty())
			{
				std::string name = PathUtil::RemoveExtension(PathUtil::GetFile(source->GetDCCFileName()));
				std::replace(name.begin(), name.end(), ' ', '_');
				std::string const colladaPath = PathUtil::Make(originalExportDirectory, name + exportExtension);
				colladaGeometryFileNameList.push_back(colladaPath);

				geometryExportSources.push_back(GeometryExportSourceAdapter(source, &geometryFileData, geometryFileIndices));
				exportList.push_back(std::make_pair(colladaPath, &geometryExportSources.back()));
			}

			for (int geometryFileIndex = 0; geometryFileIndex < geometryFileData.GetGeometryFileCount(); ++geometryFileIndex)
			{
				const IGeometryFileData::SProperties& properties = geometryFileData.GetProperties(geometryFileIndex);

				std::string const geometryFileName = geometryFileData.GetGeometryFileName(geometryFileIndex);

				if (!geometryFileName.empty() && properties.filetypeInt != CRY_FILE_TYPE_INTERMEDIATE_CAF)
				{
					std::string extension;
					if (properties.filetypeInt == CRY_FILE_TYPE_CGF)
					{
						extension = "cgf";
					}
					else if ((properties.filetypeInt == CRY_FILE_TYPE_CGA) ||
						(properties.filetypeInt == (CRY_FILE_TYPE_CGA | CRY_FILE_TYPE_ANM)))
					{
						extension = "cga";
					}
					else if (properties.filetypeInt == CRY_FILE_TYPE_ANM)
					{
						extension = "anm";
					}
					else if (properties.filetypeInt == CRY_FILE_TYPE_CHR ||
						(properties.filetypeInt == (CRY_FILE_TYPE_CHR | CRY_FILE_TYPE_INTERMEDIATE_CAF)))
					{
						extension = "chr";
					}
					else if (properties.filetypeInt == CRY_FILE_TYPE_SKIN)
					{
						extension = "skin";
					}
					else
					{
						context->Log(ILogger::eSeverity_Error, "Unsupported file type (%08x) for exporting %s", properties.filetypeInt, geometryFileName.c_str());
						return;
					}

					std::string safeGeometryFileName = geometryFileName;
					std::replace(safeGeometryFileName.begin(), safeGeometryFileName.end(), ' ', '_');
					std::string assetPath = PathUtil::Make(originalExportDirectory, safeGeometryFileName + "." + extension);

					if (!properties.customExportPath.empty())
					{
						assetPath = PathUtil::Make(properties.customExportPath, safeGeometryFileName + "." + extension);
					}

					assetGeometryFileNameList.push_back(assetPath);					
				}

				if (properties.filetypeInt & CRY_FILE_TYPE_INTERMEDIATE_CAF)
				{
					for (int animationIndex = 0; animationIndex < source->GetAnimationCount(); ++animationIndex)
					{
						std::string const animationName = source->GetAnimationName(&geometryFileData, geometryFileIndex, animationIndex);

						// Animations beginning with an underscore should be ignored.
						bool const ignoreAnimation = animationName.empty() || (animationName[0] == '_');

						if (!ignoreAnimation)
						{
							std::string safeAnimationName = animationName;
							std::replace(safeAnimationName.begin(), safeAnimationName.end(), ' ', '_');

							std::string exportPath = PathUtil::Make(originalExportDirectory, safeAnimationName + exportExtension);
							animationFileNameList.push_back(std::make_pair(std::make_pair(animationIndex, geometryFileIndex), exportPath));
							animationExportSources.push_back(SingleAnimationExportSourceAdapter(source, &geometryFileData, geometryFileIndex, animationIndex));
							exportList.push_back(std::make_pair(exportPath, &animationExportSources.back()));
						}
					}
				}
			}
		}

		// Export the COLLADA file to the chosen file.
		{
			ProgressRange exportProgressRange(progressRange, 0.6f);

			size_t const daeCount = exportList.size();
			float const daeProgressRangeSlice = 1.0f / (daeCount > 0 ? daeCount : 1);
			for (ExportList::iterator itFile = exportList.begin(); itFile != exportList.end(); ++itFile)
			{
				const std::string& colladaFileName = (*itFile).first;
				IExportSource* fileExportSource = (*itFile).second;

				ProgressRange animationExportProgressRange(exportProgressRange, daeProgressRangeSlice);

				try
				{
					context->Log(ILogger::eSeverity_Info, "Exporting to file '%s'", colladaFileName.c_str());

					// Try to create the directory for the file.
					if (!FileUtil::EnsureDirectoryExists(PathHelpers::GetDirectory(colladaFileName).c_str()))
					{
						context->Log(ILogger::eSeverity_Error, "Unable to create directory for %s", colladaFileName.c_str());
						return;
					}

					bool ok;

					if (bExportCompressed)
					{
						IPakSystem* pakSystem = (context ? context->GetPakSystem() : 0);	
						if (!pakSystem)
						{
							throw IExportContext::PakSystemError("No pak system provided.");
						}

						std::string const archivePath = colladaFileName;
						std::string archiveRelativePath = colladaFileName.substr(0, colladaFileName.length() - exportExtension.length()) + ".dae";
						archiveRelativePath = PathUtil::GetFile(archiveRelativePath);

						XMLPakFileSink sink(pakSystem, archivePath, archiveRelativePath);
						ok = ColladaWriter::Write(fileExportSource, context, &sink, animationExportProgressRange);
					}
					else
					{
						XMLFileSink fileSink(colladaFileName);
						ok = ColladaWriter::Write(fileExportSource, context, &fileSink, animationExportProgressRange);
					}

					if (!ok)
					{
						// FIXME: erase the resulting file somehow
						context->Log(ILogger::eSeverity_Error, "Failed to export '%s'", colladaFileName.c_str());
						return;
					}
				}
				catch (const IXMLSink::OpenFailedError& e)
				{
					context->Log(ILogger::eSeverity_Error, "Unable to open output file: %s", e.what());
					return;
				}
				catch (...)
				{
					context->Log(ILogger::eSeverity_Error, "Unexpected crash in COLLADA exporter");
					return;
				}
			}
		}
	}

	const CResourceCompilerHelper::ERcExePath resourceCompilerPathType = CResourceCompilerHelper::eRcExePath_registry;

	// Run the resource compiler on the COLLADA file to generate uncompressed CAFs.
	{
		ProgressRange compilerProgressRange(progressRange, 0.075f);

		CurrentTaskScope currentTask(context, "rc");

		size_t const daeCount = animationFileNameList.size();
		float const animationProgressRangeSlice = 1.0f / (daeCount > 0 ? daeCount : 1);
		for (AnimationFileNameList::iterator itFile = animationFileNameList.begin(); itFile != animationFileNameList.end(); ++itFile)
		{
			const std::string colladaFileName = (*itFile).second;
			const std::string expectedCafPath = colladaFileName.substr(0, colladaFileName.length() - exportExtension.length()) + ".i_caf";

			if (FileUtil::FileExists(expectedCafPath.c_str()))
			{
				if (!DeleteFileA(expectedCafPath.c_str()))
				{
					context->Log(ILogger::eSeverity_Error, "Failed to remove existing animation file: %s", expectedCafPath.c_str());
					continue;
				}
			}

			ProgressRange animationCompileProgressRange(compilerProgressRange, animationProgressRangeSlice);
			ResourceCompilerLogListener listener(context);
			context->Log(ILogger::eSeverity_Info, "Calling RC to generate uncompressed CAF file: %s", colladaFileName.c_str());
			CResourceCompilerHelper::ERcCallResult result = CResourceCompilerHelper::CallResourceCompiler(
				colladaFileName.c_str(), 
				"/refresh", 
				&listener,
				true, resourceCompilerPathType,false,false,0);

			if (result != CResourceCompilerHelper::eRcCallResult_success)
			{
				context->Log(ILogger::eSeverity_Error, "%s", CResourceCompilerHelper::GetCallResultDescription(result));
				continue;
			}

			context->Log(ILogger::eSeverity_Debug, "RC finished: %s", colladaFileName.c_str());
			if (!FileUtil::FileExists(expectedCafPath.c_str()))
			{
				context->Log(ILogger::eSeverity_Error, "Following Animation file is expected to be created by RC: %s", expectedCafPath.c_str());
				context->Log(ILogger::eSeverity_Error, "Do you have an old RC version?");
			}

#if !defined(_DEBUG)
			// Delete the Collada file.
			DeleteFileA(colladaFileName.c_str());
#endif
		}
	}

	// Run the resource compiler on the COLLADA file to generate the geometry assets.
	{
		ProgressRange compilerProgressRange(progressRange, 0.075f);

		CurrentTaskScope currentTask(context, "rc");

		size_t const daeCount = colladaGeometryFileNameList.size();
		float const assetProgressRangeSlice = 1.0f / (daeCount > 0 ? daeCount : 1);
		for (size_t i = 0; i < daeCount; ++i)
		{
			const std::string& colladaFileName = colladaGeometryFileNameList[i];

			ProgressRange assetCompileProgressRange(compilerProgressRange, assetProgressRangeSlice);
			ResourceCompilerLogListener listener(context);
			context->Log(ILogger::eSeverity_Info, "Calling RC to generate raw asset file: %s", colladaFileName.c_str());
			CResourceCompilerHelper::ERcCallResult result = CResourceCompilerHelper::CallResourceCompiler(
				colladaFileName.c_str(), 
				"/refresh", 
				&listener,
				true, resourceCompilerPathType,false,false,0);

#if !defined(_DEBUG)
			// Delete the Collada file.
			DeleteFileA(colladaFileName.c_str());
#endif

			if (result == CResourceCompilerHelper::eRcCallResult_success)
			{
				context->Log(ILogger::eSeverity_Debug, "RC finished: %s", colladaFileName.c_str());
			}
			else
			{
				context->Log(ILogger::eSeverity_Error, "%s", CResourceCompilerHelper::GetCallResultDescription(result));
				return;
			}
		}
	}

	{
		// Create an RC helper - do it outside the loop, since it queries the registry on construction.
		ResourceCompilerLogListener listener(context);

		// Check the registry to see whether we should optimize the geometry files or not.
		int optimizeGeometry = GetSetting<int>(context->GetSettings(), "OptimizeAssets", 1);

		// Run the resource compiler again on the generated geometry files to compress/process them.
		// TODO: This should not be necessary, the RC should be modified so that assets are automatically
		// compressed when exported from COLLADA.
		if (!optimizeGeometry)
		{
			context->Log(ILogger::eSeverity_Warning, "OptimizeAssets registry key set to 0 - not compressing CAFs");
		}
		else
		{
			context->Log(ILogger::eSeverity_Debug, "OptimizeAssets not set or set to 1 - optimizing geometry");

			CurrentTaskScope currentTask(context, "compress");
			ProgressRange compressRange(progressRange, 0.025f);

			size_t const assetCount = assetGeometryFileNameList.size();
			float const assetProgressRangeSlice = 1.0f / (assetCount > 0 ? assetCount : 1);
			for (size_t i = 0; i < assetCount; ++i)
			{
				const std::string& assetFileName = assetGeometryFileNameList[i];
				ProgressRange animationProgressRange(compressRange, assetProgressRangeSlice);

				// note: we skip some asset types because we know that they are "optimized" already
				if (StringHelpers::EndsWithIgnoreCase(assetFileName, ".anm") || StringHelpers::EndsWithIgnoreCase(assetFileName, ".chr") || StringHelpers::EndsWithIgnoreCase(assetFileName, ".skin"))
				{
					context->Log(ILogger::eSeverity_Info, "Calling RC to optimize asset \"%s\"", assetFileName.c_str());
					CResourceCompilerHelper::ERcCallResult result = CResourceCompilerHelper::CallResourceCompiler(assetFileName.c_str(), "/refresh", &listener,true, resourceCompilerPathType,false,false,0);
					if (result == CResourceCompilerHelper::eRcCallResult_success)
					{
						context->Log(ILogger::eSeverity_Debug, "RC finished: %s", assetFileName.c_str());
					}
					else
					{
						context->Log(ILogger::eSeverity_Error, "%s", CResourceCompilerHelper::GetCallResultDescription(result));
						return;
					}
				}
			}
		}
	}

	// Log the end time.
	{
		char buf[1024];
		std::time_t t = std::time(0);
		std::strftime(buf, sizeof(buf) / sizeof(buf[0]), "%H:%M:%S on %a, %d/%m/%Y", std::localtime(&t));
		context->Log(ILogger::eSeverity_Info, "Export finished at %s", buf);
	}
}
