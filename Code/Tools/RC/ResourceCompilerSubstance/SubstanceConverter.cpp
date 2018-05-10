// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "IRCLog.h"
#include "ICfgFile.h"
#include "SubstanceConverter.h"
#include "SubstanceCompiler.h"
#include "IAssetManager.h"

#include "ISubstanceManager.h"
#include "SubstanceCommon.h"
#include "ISubstanceInstanceRenderer.h"
#include "substance/framework/output.h"
#include "substance/texture.h"
#include "substance/pixelformat.h"
#include "ResourceCompiler.h"
#include <tiffio.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/ToolsHelpers/SettingsManagerHelpers.h>
#include <FileUtil.h>
#include <CryMath/LCGRandom.h>


class CFileReader : public IFileManipulator
{
public:
	CFileReader(string root, IPakSystem* pakSystem)
		: m_root(root), m_pPakSystem(pakSystem)
	{

	}

	virtual bool ReadFile(const string& filePath, std::vector<char>& buffer, size_t& readSize, const string& mode) override
	{
		string absPath = GetAbsolutePath(filePath);
		PakSystemFile* f = m_pPakSystem->Open(absPath, mode);
		if (!f)
			return false;
		readSize= m_pPakSystem->GetLength(f);
		buffer.resize(readSize);
		bool result = m_pPakSystem->Read(f, &(buffer)[0], readSize) == readSize;
		m_pPakSystem->Close(f);
		return result;
	}

	virtual string GetAbsolutePath(const string& filename) const override
	{
		if (PathHelpers::IsRelative(filename))
		{
			return PathUtil::Make(m_root, filename);
		}
		return filename;
	}

private:
	string m_root;
	IPakSystem* m_pPakSystem;

};
struct SGeneratedOutputData
{
	string source;
	string path;
	string texturePreset;
};


typedef std::map<string, std::shared_ptr<SGeneratedOutputData>> OutputDataMap;
typedef std::map<SubstanceAir::UInt, OutputDataMap> PresetOutputsDataMap;

CRndGen gConverterRandomGenerator;
class CSubstanceRenderer : public ISubstanceInstanceRenderer
{
public:	

	CSubstanceRenderer(IResourceCompiler* pRC, CSubstanceConverter* converter)
		: m_pRC(pRC)
		, m_coverter(converter)
	{}

	static void FormPhotoshopDataBlock(std::vector<char>& data, const string& str)
	{
		data.clear();

		// Use temporary variable to prevent failure in case of "merge strings"
		// compiler option turned off, because then every parameter has a different address.
#define ADD(ref) { const char* const cstr = ref; std::copy(cstr, cstr + sizeof(ref) - 1, std::back_inserter(data)); }
		ADD("8BIM");
		ADD("\x04\x04");
		ADD("\0\0");
		int irbSizePosition = int(data.size());
		ADD("\0\0\0\0");
		int irbDataStart = int(data.size());
		ADD("\x1C\x02");
		ADD("\0");
		ADD("\x00\x02");
		ADD("\x00\x02");
		ADD("\x1C\x02");
		ADD("\x28");
		int captionSizePosition = int(data.size());
		ADD("\0\0");
		int captionStartPosition = int(data.size());
		std::copy(str.c_str(), str.c_str() + str.size(), std::back_inserter(data));
#undef ADD

		int captionSize = int(data.size()) - captionStartPosition;
		char* captionSizePointer = &data[0] + captionSizePosition;
		captionSizePointer[0] = (captionSize >> 8) & 0xFF;
		captionSizePointer[1] = captionSize & 0xFF;

		int irbSize = int(data.size()) - irbDataStart;
		char* irbSizePointer = &data[0] + irbSizePosition;
		irbSizePointer[0] = (irbSize >> 24) & 0xFF;
		irbSizePointer[1] = (irbSize >> 16) & 0xFF;
		irbSizePointer[2] = (irbSize >> 8) & 0xFF;
		irbSizePointer[3] = irbSize & 0xFF;
	}

	static bool SaveByUsingTIFFSaver(const char *filenameWrite, const string& settings, const SubstanceTexture& substanceTexture)
	{
		uint32 dwWidth, dwHeight;
		dwWidth = substanceTexture.level0Width;
		dwHeight = substanceTexture.level0Height;
		
		wstring widePath;
		Unicode::Convert(widePath, filenameWrite);

		TIFF* const pTiffFile = TIFFOpenW(widePath.c_str(), "w");
		if (!pTiffFile)
		{
			return false;
		}

		{
			const uint32 dwPlanes = 4;
			const uint32 dwBitsPerSample = 16;

			// We need to set some values for basic tags before we can add any data
			TIFFSetField(pTiffFile, TIFFTAG_IMAGEWIDTH, dwWidth);
			TIFFSetField(pTiffFile, TIFFTAG_IMAGELENGTH, dwHeight);
			TIFFSetField(pTiffFile, TIFFTAG_SAMPLESPERPIXEL, dwPlanes);
			TIFFSetField(pTiffFile, TIFFTAG_BITSPERSAMPLE, dwBitsPerSample);
			TIFFSetField(pTiffFile, TIFFTAG_PHOTOMETRIC, (dwPlanes == 1 ? PHOTOMETRIC_MINISBLACK : (dwPlanes == 2 ? PHOTOMETRIC_SEPARATED : PHOTOMETRIC_RGB)));
			TIFFSetField(pTiffFile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
#ifdef CRYTIF_DISABLE_COMPRESSION
			TIFFSetField(pTiffFile, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			TIFFSetField(pTiffFile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(pTiffFile, TIFFTAG_PREDICTOR, PREDICTOR_NONE);
			TIFFSetField(pTiffFile, TIFFTAG_ROWSPERSTRIP, 1);
#else
			TIFFSetField(pTiffFile, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
			TIFFSetField(pTiffFile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(pTiffFile, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);

			tsize_t dstLineSize = dwWidth * (dwBitsPerSample / 8);
			tsize_t dstPitch = dstLineSize * dwPlanes;
			uint32 dstNumRowsPerStrip = 16 * uint32((32768 + dstPitch - 1) / dstPitch);

			dstNumRowsPerStrip = TIFFDefaultStripSize(pTiffFile, dstNumRowsPerStrip);
			TIFFSetField(pTiffFile, TIFFTAG_ROWSPERSTRIP, dstNumRowsPerStrip);
#endif // CRYTIF_DISABLE_COMPRESSION

			// CryEngine expects a "source DCC filename" stored TIFFTAG_IMAGEDESCRIPTION,
			// but I have no idea what name should we write, so write empty string
			TIFFSetField(pTiffFile, TIFFTAG_IMAGEDESCRIPTION, "");

			// update meta data without re-opening the file again, as 
			// this causes problems with file-change listeners locking onto
			// the file between the close and the subsequent update
			if (!settings.empty())
			{
				std::vector<char> data;
				FormPhotoshopDataBlock(data, settings);

				// set resource compiler settings
				assert(!data.empty());
				TIFFSetField(pTiffFile, TIFFTAG_PHOTOSHOP, (uint32)data.size(), (const uchar*)&data[0]);  // 34377 IPTC TAG
			}

			TIFFSetField(pTiffFile, TIFFTAG_BITSPERSAMPLE, 16);

			std::vector<short> raster(dwWidth * dwPlanes);
			char* buffer = (char*)substanceTexture.buffer;
			size_t rowLenght = sizeof(uint16) * dwWidth * 4;
			for (uint32 row = 0; row < dwHeight; ++row)
			{				
				const int err = TIFFWriteScanline(pTiffFile, buffer, row, 0);
				assert(err >= 0);
				buffer += rowLenght;
			}
		}

		TIFFClose(pTiffFile);

		return true;
	}

	virtual void OnOutputAvailable(SubstanceAir::UInt runUid,const SubstanceAir::GraphInstance *graphInstance, 
		SubstanceAir::OutputInstanceBase * outputInstance) override
	{
		SubstanceAir::OutputInstance::Result result(outputInstance->grabResult());
		if (result.get() != NULL)
		{
			SGeneratedOutputData* data = (SGeneratedOutputData*)(outputInstance->mUserData);
			string commandline;
			commandline.Format("/autooptimizefile=0 /preset=%s /reduce=0 /cryasset=source,%s", data->texturePreset, data->source);
			// TODO for now dissabling directds as nested calls have issues.
			//if (m_coverter->GetConfig()->GetAsBool("directdds", false, true))
			//{
			//	// when writing directly dds, tif is generated to temp directory, and converted there
			//	string tempFileName = std::to_string(gConverterRandomGenerator.GenerateUint64()).c_str();
			//	string tempPath = PathUtil::Make(m_pRC->GetTmpPath(), tempFileName + ".tif");
			//	SaveByUsingTIFFSaver(tempPath, commandline, result->getTexture());
			//	std::vector<string> args = { "/refresh", string().Format("/overwritefilename=%s", PathUtil::GetFile(data->path).c_str()), string().Format("/targetroot=%s", PathHelpers::GetDirectory(data->path)), string().Format("/sourceroot=%s", PathHelpers::GetDirectory(tempPath)) };
			//	m_pRC->CompileSingleFileNested(PathUtil::GetFile(tempPath), args);
			//	DeleteFile(tempPath);
			//}
			//else
			{
				const string tempPath(data->path + ".$ti"); // using ".$tif" is unsafe, by mistake the engine can treat it as the final tif file.
				const string finalPath(data->path + ".tif");
				const string filename = PathUtil::GetFile(finalPath);
				RCLog("Saving output from %s: %s", PathUtil::GetFile(data->texturePreset).c_str(), filename.c_str());

				if (!SaveByUsingTIFFSaver(tempPath, commandline, result->getTexture()))
				{
					RCLogError("Failed to save temporary image file: %s", tempPath.c_str());
				}

				// Force remove of the read only flag.
				SetFileAttributes(finalPath, FILE_ATTRIBUTE_ARCHIVE);
				MoveFileEx(tempPath, finalPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
				m_pRC->AddInputOutputFilePair(data->path, finalPath);
			}
		}
	}

	virtual void FillVirtualOutputRenderData(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) override
	{
		renderData.resize(1);
		SSubstanceRenderData& outRenderData = renderData[0];
		outRenderData.format = Substance_PF_RGBA | Substance_PF_16I;
		outRenderData.name = output.name;
		outRenderData.useMips = false;
		outRenderData.skipAlpha = false;
		outRenderData.swapRG = false;
		outRenderData.output = output;
		AttachOutputUserData(preset, output, outRenderData);
	}

	void AttachOutputUserData(const ISubstancePreset* preset, const SSubstanceOutput& output, SSubstanceRenderData& renderData)
	{
		if (!m_PresetOutputDataMap.count(preset->GetInstanceID()) || !m_PresetOutputDataMap[preset->GetInstanceID()].count(renderData.name))
		{
			std::shared_ptr<SGeneratedOutputData> generatedData = std::make_shared<SGeneratedOutputData>();
			generatedData->source = PathUtil::GetFile(preset->GetFileName());
			generatedData->path = preset->GetOutputPath(&output);
			m_PresetOutputDataMap[preset->GetInstanceID()][renderData.name] = generatedData;
		}
		SGeneratedOutputData* generatedData = m_PresetOutputDataMap[preset->GetInstanceID()][renderData.name].get();
		generatedData->texturePreset = output.preset;
		renderData.customData = (size_t)generatedData;
	}

	virtual SubstanceAir::InputImage::SPtr GetInputImage(const ISubstancePreset* preset, const string& path) override
	{
		return m_coverter->GetInputImage(preset, path);
	}

private:
	IResourceCompiler* m_pRC;
	CSubstanceConverter* m_coverter;
	PresetOutputsDataMap m_PresetOutputDataMap;
};


CSubstanceConverter::CSubstanceConverter(IResourceCompiler* pRC)
	: m_pRC(pRC)
{

}

CSubstanceConverter::~CSubstanceConverter()
{
}

void CSubstanceConverter::Release()
{
	delete this;
}

void CSubstanceConverter::Init(const ConverterInitContext& context)
{
	m_pConfig = context.config;
	m_gameRootPath = context.config->GetAsString("sourceroot", "", "");
	CFileReader* fileReader = new CFileReader(m_gameRootPath, m_pRC->GetPakSystem());
	m_pRenderer = new CSubstanceRenderer(m_pRC, this);
	InitCrySubstanceLib(fileReader); // Life-time of fileReader will be managed by substance lib.
	ISubstanceManager::Instance()->RegisterInstanceRenderer(m_pRenderer);

}

ICompiler* CSubstanceConverter::CreateCompiler()
{
	return new CSubstanceCompiler(this);
}

bool CSubstanceConverter::SupportsMultithreading() const
{
	return true;
}

const char* CSubstanceConverter::GetExt(int index) const
{
	if (index == 0)
	{
		return "crysub";
	}

	return NULL;
}

const SubstanceAir::InputImage::SPtr& CSubstanceConverter::GetInputImage(const ISubstancePreset* preset, const string& path)
{
	const uint32 crc = CCrc32::ComputeLowercase(path);

	if (m_loadedImages.count(crc) == 0)
	{
		// TODO this is pretty temp solution, but should work for basic implementation
		// although path is dds, we try to search for tif with the same name, if tif not found, we will try to find different image extensions and if not
		// try to convert the dds
		string nameWithoutExtension = PathUtil::Make(m_gameRootPath, PathUtil::RemoveExtension(path));
		nameWithoutExtension.Replace("/", "\\");
		string tiffPath = nameWithoutExtension + ".tif";
		std::vector<string> filesToRemove;
		if (!FileUtil::FileExists(tiffPath))
		{
			std::vector<string> supported{ ".bmp", ".jpg", ".png", ".psd", ".tga", ".dds" };
			for (const string& format : supported)
			{
				string filePath = nameWithoutExtension + format;
				PakSystemFile* textureFile = m_pRC->GetPakSystem()->Open(filePath, "r");
				string srcPath, targetPath;
				string tmpPath = PathUtil::Make(m_pRC->GetTmpPath(), PathHelpers::GetShortestRelativePath(m_gameRootPath, filePath)).Replace("/", "\\");
				targetPath = PathUtil::ReplaceExtension(tmpPath, "tif");
				if (textureFile)
				{
					if (textureFile->type == PakSystemFileType_PakFile)
					{
						if (FileUtil::FileExists(tmpPath.c_str()))
						{
							DeleteFile(tmpPath.c_str());
						}
						else {
							FileUtil::EnsureDirectoryExists(PathUtil::GetParentDirectory(tmpPath));
						}
						FILE* fFileOnDisk = fopen(tmpPath, "wb");
						if (!fFileOnDisk)
						{
							m_pRC->GetPakSystem()->Close(textureFile);
							textureFile = 0;
							break;
						}

						fwrite(textureFile->data, textureFile->fileEntry->desc.lSizeUncompressed, 1, fFileOnDisk);
						fclose(fFileOnDisk);
						filesToRemove.push_back(tmpPath);
						srcPath = tmpPath;
					}
					else
					{
						FileUtil::EnsureDirectoryExists(PathUtil::GetParentDirectory(tmpPath));
						srcPath = filePath;
					}

					m_pRC->GetPakSystem()->Close(textureFile);
					if (ConvertToTiff(srcPath, targetPath) && FileUtil::FileExists(targetPath))
					{
						filesToRemove.push_back(targetPath);
						tiffPath = targetPath;
						break;
					}



				}
			}
		}

		int imgW, imgH;
		uint32* buffer;
		bool dataLoaded = false;
		if (FileUtil::FileExists(tiffPath))
		{
			wstring widePath;
			Unicode::Convert(widePath, tiffPath);

			TIFF* tif = TIFFOpenW(widePath.c_str(), "r");
			if (tif) {
				size_t npixels;

				TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imgW);
				TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imgH);
				npixels = imgW * imgH;
				buffer = (uint32*)_TIFFmalloc(npixels * sizeof(uint32));
				if (TIFFReadRGBAImageOriented(tif, imgW, imgH, buffer, ORIENTATION_TOPLEFT, 0))
				{
					dataLoaded = true;
				}
				TIFFClose(tif);
			}

		}

		if (!dataLoaded)
		{
			imgW = imgH = 16;
			int imgSize = imgW * imgH * sizeof(uint32);
			// still not found, need to provide replaceme.
			buffer = (uint32*)_TIFFmalloc(imgSize);
			memset(buffer, 0xFFFFFFFFu, imgSize);
		}

		SubstanceTexture texture = {
			buffer, // No buffer (content will be set later for demo purposes)
			imgW,  // Width;
			imgH,  // Height
			Substance_PF_RGBA,
			Substance_ChanOrder_RGBA,
			1 };

		m_loadedImages[crc] = SubstanceAir::InputImage::create(texture);


		if (buffer)
		{
			_TIFFfree(buffer);
		}

		for (string& file : filesToRemove)
		{
			DeleteFile(file.c_str());
		}
		
	}

	SubstanceAir::InputImage::SPtr& result = m_loadedImages[crc];
	return result;
}

bool CSubstanceConverter::ConvertToTiff(const string& path, const string& target)
{

	if (!stricmp(PathUtil::GetExt(path), "dds"))
	{
		std::vector<string> args = { "/refresh", "/decompress", string().Format("/targetroot=%s", PathHelpers::GetDirectory(target)) };
		return m_pRC->CompileSingleFileNested(path, args);
	}


	string imageMagickFolder = PathUtil::Make(PathHelpers::GetDirectory(PathHelpers::GetDirectory(m_pRC->GetExePath())), "thirdparty\\ImageMagick");
	imageMagickFolder.Replace("/", "\\");
	PROCESS_INFORMATION pi;

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwX = 100;
	si.dwY = 100;
	si.dwFlags = STARTF_USEPOSITION;


	ZeroMemory(&pi, sizeof(pi));

	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 3> wRemoteCmdLine;
	wRemoteCmdLine.appendAscii("\"");
	wRemoteCmdLine.appendAscii(PathUtil::Make(imageMagickFolder, "convert.exe").replace("/", "\\"));
	wRemoteCmdLine.appendAscii("\" ");

	// Make sure to *not* write alpha channel.
	// "Normals" preset of RC does not work well with alpha channel.
	wRemoteCmdLine.appendAscii("-type TrueColor ");

	wRemoteCmdLine.appendAscii("\"");
	wRemoteCmdLine.appendAscii(path.c_str());
	wRemoteCmdLine.appendAscii("\" ");

	wRemoteCmdLine.appendAscii("\"");
	wRemoteCmdLine.appendAscii(target.c_str());
	wRemoteCmdLine.appendAscii("\"");

	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 3> wEnv;
	wEnv.appendAscii("PATH=");
	wEnv.appendAscii(PathUtil::ToDosPath(imageMagickFolder));
	wEnv.appendAscii("\0", 1);
	wEnv.appendAscii("MAGICK_CODER_MODULE_PATH=");
	wEnv.appendAscii(PathUtil::ToDosPath(PathUtil::Make(imageMagickFolder, "modules\\coders")));
	wEnv.appendAscii("\0", 1);


	if (!CreateProcessW(
		NULL,                                          // No module name (use command line).
		const_cast<wchar_t*>(wRemoteCmdLine.c_str()),  // Command line.
		NULL,                                          // Process handle not inheritable.
		NULL,                                          // Thread handle not inheritable.
		TRUE,                                          // Set handle inheritance to TRUE.
		CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, // creation flags.
													   // NULL,                                         // Use parent's environment block.
		(void*)wEnv.c_str(),
		NULL, // Set starting directory.
							 // nullptr,                          // Set starting directory.
		&si,                 // Pointer to STARTUPINFO structure.
		&pi))                // Pointer to PROCESS_INFORMATION structure.
	{
		// The following  code block is commented out instead of being deleted
		// because it's good to have at hand for a debugging session.
#if 0
		const size_t charsInMessageBuffer = 32768;   // msdn about FormatMessage(): "The output buffer cannot be larger than 64K bytes."
		wchar_t szMessageBuffer[charsInMessageBuffer] = L"";
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, szMessageBuffer, charsInMessageBuffer, NULL);
		GetCurrentDirectoryW(charsInMessageBuffer, szMessageBuffer);
#endif

		// not found
		return false;
	}


	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles.
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return true;
}
