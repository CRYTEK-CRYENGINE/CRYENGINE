// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/CryCharAnimationParams.h>
#include "Controller.h"
#include "AnimationInfoLoader.h"
#include "../../../CryXML/IXMLSerializer.h"
#include "../CryEngine/Cry3DEngine/CGF/CGFLoader.h"
#include "CGF/CGFSaver.h"
#include "CGF/LoaderCAF.h"
#include "ControllerPQLog.h"
#include "ControllerPQ.h"
#include "SkeletonInfo.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "ConvertContext.h"
#include <CrySystem/CryVersion.h>
#include "CompressionController.h"
#include "GlobalAnimationHeader.h"
#include "GlobalAnimationHeaderCAF.h"
#include "GlobalAnimationHeaderAIM.h"
#include "ResourceCompiler.h" // for CryCriticalSection

class CSkeletonInfo;
struct ConvertContext;
class CContentCGF;
class CTrackStorage;
struct SPlatformAnimationSetup;

class CAnimationCompressor
{
public:
	explicit CAnimationCompressor(const CSkeletonInfo& skeleton);

	bool LoadCAF(const char* name, const SPlatformAnimationSetup& platform, const SAnimationDesc& animDesc, ECAFLoadMode mode);
	bool ProcessAnimation(bool debugCompression);

	uint32 AddCompressedChunksToFile(const char* name, bool bigEndianOutput);
	uint32 SaveOnlyCompressedChunksInFile(const char* name, FILETIME timeStamp, GlobalAnimationHeaderAIM* gaim, bool saveCAFHeader, bool bigEndianOutput);

	bool ProcessAimPoses(bool isLook, GlobalAnimationHeaderAIM& rAIM);

	static bool AreKeyTimesAscending(KeyTimesInformationPtr& keyTimes);
	static bool CompareKeyTimes(KeyTimesInformationPtr& ptr1, KeyTimesInformationPtr& ptr2);

	void EvaluateStartLocation();
	void ReplaceRootByLocator(bool bCheckLocator, float minRootPosEpsilon, float minRootRotEpsilon, float minRootSclEpsilon);
	void SetIdentityLocator(GlobalAnimationHeaderAIM& gaim);
	void EvaluateSpeed();
	void DetectCycleAnimations();

	void EnableTrackAligning(bool bEnable);
	void EnableRootMotionExtraction(const char* szRootMotionJointName);

	uint32 SaveControllers(CSaverCGF& saver, bool bigEndianOutput);
	void SaveMotionParameters(CChunkFile* pChunkFile, bool bigEndianOutput);
	bool SaveController829(CSaverCGF& saver, CController* pController, bool bigEndianOutput);
	bool SaveController832(CSaverCGF& saver, CController* pController, bool bigEndianOutput);

private:

	void DeleteCompressedChunks();
	void ExtractKeys(std::vector<int>& times, std::vector<PQLogS>& logKeys, const CControllerPQLog* controller) const;
	int ConvertRemainingControllers(TControllersVector& controllers) const;
	static CController* CreateNewController(int controllerID, const std::vector<int>& arrFullTimes, const std::vector<PQLogS>& arrFullQTKeys, float positionTolerance, float rotationToleranceInDegrees, float scaleTolerance);
	static CController* CreateNewControllerAIM(int controllerID, const std::vector<int>& arrFullTimes, const std::vector<PQLogS>& arrFullQTKeys);

public:

	GlobalAnimationHeaderCAF m_GlobalAnimationHeader;
	CSkeletonInfo m_skeleton;
	SAnimationDesc m_AnimDesc;
	CChunkFile m_ChunkFile;

private:

	string m_filename; // filename which was used to LoadCAF
	SPlatformAnimationSetup m_platformSetup;
	string m_rootMotionJointName;
	bool m_bAlignTracks;
};
