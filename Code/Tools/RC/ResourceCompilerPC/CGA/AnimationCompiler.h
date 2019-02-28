// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IConverter.h"
#include "../CryEngine/Cry3DEngine/CGF/CGFLoader.h"

#include "AnimationInfoLoader.h"
#include "ResourceCompiler.h"
#include "GlobalAnimationHeaderCAF.h"

#include "SkeletonLoader.h"
#include "SkeletonManager.h"

struct ConvertContext;
class CSkeletonInfo;
class CAnimationManager;
class DBATableEnumerator;
struct SCompressionPresetTable;
struct SAnimationDesc;
class CTrackStorage;

namespace ThreadUtils
{
	class StealingThreadPool;
}
class CAnimationCompiler;
class SkeletonLoader;

struct AnimationJob
{
	CAnimationCompiler* m_compiler;

	// Path to animation database
	string m_dbaPath;

	// Unified animation path (path from Game folder)
	string m_animationPath;

	// Actual source path. Can have a different extension if single file (/file) processed
	string m_sourcePath;

	// Name of temporary file. This file is always stored in little-endian (PC)
	// format. It is used when final animation stored in .DBA or converted to
	// big-endian (console format). We can't load big-endian CAF's on PC
	// at the moment.
	string m_intermediatePath;
	
	// Output file
	string m_destinationPath;

	SAnimationDesc m_animDesc;
	const CSkeletonInfo* m_skeleton;

	bool m_debugCompression;
	bool m_bigEndianOutput;
	bool m_fileChanged;
	bool m_alignTracks;
	
	bool m_writeIntermediateFile; // e.g. .$caf
	bool m_writeDestinationFile;
	size_t m_resSize;

	AnimationJob()
		: m_compiler(0)
		, m_fileChanged(false)
		, m_debugCompression(false)
		, m_bigEndianOutput(false)
		, m_alignTracks(false)
		, m_writeIntermediateFile(false)
		, m_writeDestinationFile(false)
		, m_resSize(0)
		, m_skeleton(0)
	{
	}
};


class ICryXML;

class CAnimationConverter : public IConverter
{
public:

	CAnimationConverter(ICryXML* pXMLParser, IPakSystem* pPakSystem, IResourceCompiler* pRC);

	// interface IConverter ------------------------------------------------------------
	virtual void Init(const ConverterInitContext& context);

	virtual void DeInit();
	
	virtual void Release();

	virtual ICompiler* CreateCompiler();
	virtual bool SupportsMultithreading() const;

	virtual const char* GetExt(int index) const
	{
		switch (index)
		{
		case 0:
			return "i_caf";
		default:
			return 0;
		}
	}

	// ---------------------------------------------------------------------------------

	// thread-safe methods
	void SetPlatform(int platform);
	const string& GetSourceGameFolderPath() const{ return m_sourceGameFolderPath; }
	const SPlatformAnimationSetup& GetPlatformSetup() const{ return m_platformAnimationSetup; }

	void IncrementChangedAnimationCount();
	int IncrementFancyAnimationIndex();

	ICryXML* GetXMLParser() { return m_pXMLParser; }
	IPakSystem* GetPakSystem() { return m_pPakSystem; }

	bool InLocalUpdateMode() const;

	std::auto_ptr<SkeletonManager> m_skeletonManager;
	std::auto_ptr<DBATableEnumerator> m_dbaTableEnumerator;
	std::auto_ptr<SCompressionPresetTable> m_compressionPresetTable;
private:
	bool RebuildDatabases();

	void InitDbaTableEnumerator();
	void InitSkeletonManager(const std::set<string>& usedSkeletons);


	int m_platform;
	SPlatformAnimationSetup m_platformAnimationSetup;

	// Folder that contains SkeletonList.xml and DBATable.json, relative to game data folder.
	// "Animations", if /animConfigFolder is not specified.
	string m_configSubfolder;

	// Source game data folder path is a location relative to which animation paths are specified.
	string m_sourceGameFolderPath;

	IResourceCompiler* m_rc;
	IPakSystem* m_pPakSystem;
	ICryXML* m_pXMLParser;
	const IConfig* m_config;
	volatile LONG m_fancyAnimationIndex;
	volatile LONG m_changedAnimationCount;
};

class CAnimationCompiler : public ICompiler
{
public:
	explicit CAnimationCompiler(CAnimationConverter* converter);
	virtual ~CAnimationCompiler();

	// interface ICompiler -------------------------------------------------------------
	virtual void Release();

	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }

	virtual IConvertContext* GetConvertContext() { return &m_CC; }
	virtual bool Process();
	// ---------------------------------------------------------------------------

	CAnimationConverter* Converter() const{ return m_converter; }

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

	void GetFilenamesForUpToDateCheck(std::vector<string>& upToDateSrcFilenames, std::vector<string>& upToDateCheckFilenames);
	bool CheckIfAllFilesAreUpToDate(const std::vector<string>& upToDateSrcFilenames, const std::vector<string>& upToDateCheckFilenames);
	void HandleUpToDateCheckFilesOnReturn(const std::vector<string>& upToDateSrcFilenames, const std::vector<string>& upToDateCheckFilenames);

	CAnimationConverter* m_converter;
	ICryXML* m_pXMLParser;
	ConvertContext m_CC;
};


// a little helper to replace SWAP macro
struct SEndiannessSwapper
{
	explicit SEndiannessSwapper(bool bigEndianOutput)
		: m_bigEndianOutput(bigEndianOutput)
	{
	}

	template<class T>
	void operator()(T& value) const
	{
		if (m_bigEndianOutput)
		{
			SwapEndianness<T>(value);
		}
	}

	template<class T>
	void operator()(T* value, size_t count) const
	{
		if (m_bigEndianOutput)
		{
			SwapEndianness<T>(value, count);
		}
	}

private:
	bool m_bigEndianOutput;
};
