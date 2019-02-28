// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   StatCGFCompiler.cpp
//  Version:     v1.00
//  Created:     5/11/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryMath/Cry_Color.h>
#include "ConvertContext.h"
#include "CharacterCompiler.h"
#include "IConfig.h"
#include "IAssetManager.h"
#include "../CryEngine/Cry3DEngine/CGF/CGFLoader.h"
#include "CGF/CGFSaver.h"
#include "CGF/CgfUtil.h"
#include "StatCGFPhysicalize.h"
#include "MathHelpers.h"
#include <Cry3DEngine/CGF/CryHeaders.h>  // MAX_STATOBJ_LODS_NUM
#include <CrySystem/CryVersion.h>
#include "../../CryXML/ICryXML.h"
#include "../../CryXML/IXMLSerializer.h"
#include "StaticObjectCompiler.h"
#include "StringHelpers.h"
#include "FileUtil.h"
#include "UpToDateFileHelpers.h"
#include <CryAnimation/IAttachment.h>
#include "CGF\CGFNodeMerger.h"
#include "RcFile.h"
#include "../CryEngine/CryAnimation/AttachmentVClothPreProcess.h"

#include <iterator>


//////////////////////////////////////////////////////////////////////////
CharacterCompiler::CharacterCompiler(ICryXML * pXml)
{
	m_pXML = pXml;
	m_pPhysicsInterface = NULL;
}

//////////////////////////////////////////////////////////////////////////
CharacterCompiler::~CharacterCompiler()
{
	if (m_pPhysicsInterface)
		delete m_pPhysicsInterface;
}

////////////////////////////////////////////////////////////
string CharacterCompiler::GetOutputFileNameOnly() const
{
	string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
	if (StringHelpers::EndsWith(sourceFileFinal, GetExt(eFileType_cdf)))
		sourceFileFinal = PathUtil::ReplaceExtension(sourceFileFinal, "skin");
	if (m_CC.config->GetAsBool("StripNonMesh", false, true))
	{
		sourceFileFinal += "m";
	}

	return sourceFileFinal;
}

////////////////////////////////////////////////////////////
string CharacterCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

//////////////////////////////////////////////////////////////////////////
struct IntMeshCollisionInfo
{
	int m_iBoneId;
	AABB m_aABB;
	std::vector<short int> m_arrIndexes;

	IntMeshCollisionInfo()
		: m_iBoneId(-1)
	{
		// This didn't help much.
		// The BBs are reset to opposite infinities, 
		// but never clamped/grown by any member points.
		m_aABB.min.zero();
		m_aABB.max.zero();
	}
};


template<class T>
void WriteData(std::vector<uint8>& dest, const T* data, size_t count, bool bSwapEndian)
{
	const size_t total = count * sizeof(T);
	dest.resize(dest.size() + total);
	T* newdata = (T*)(&dest.back() + 1 - total);
	memcpy(newdata, data, total);
	SwapEndian(newdata, count, bSwapEndian);
}

template<class A>
void WriteArray(std::vector<uint8>& dest, const A& array, bool bSwapEndian)
{
	WriteData(dest, array.begin(), array.size(), bSwapEndian);
}

//////////////////////////////////////////////////////////////////////////
bool CharacterCompiler::Process()
{
	const string sourceFile = StringHelpers::Replace(string(m_CC.GetSourcePath()), '/', '\\');
	bool cdfasset = StringHelpers::EndsWith(sourceFile, GetExt(eFileType_cdf));

	string outputFile = StringHelpers::Replace(GetOutputPath(), '/', '\\');

	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(GetOutputPath(), m_CC.GetSourcePath()))
	{
		// The file is up-to-date
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
		return true;
	}

	bool ok = true;

	try
	{
		CChunkFile* pChunkFile = NULL;
		CLoaderCGF cgfLoader;

		{
			const int maxWeightsPerVertex = m_CC.config->GetAsInt("maxWeightsPerVertex", -1, -1);
			if (maxWeightsPerVertex >= 0)
			{
				cgfLoader.SetMaxWeightsPerVertex(maxWeightsPerVertex);
			}
		}

		class Listener : public ILoaderCGFListener
		{
		public:
			Listener()
			{
			}
			virtual void Warning(const char* format)
			{
				RCLogWarning("%s", format);
			}
			virtual void Error(const char* format)
			{
				RCLogError("%s", format);
			}
		};
		Listener listener;

		std::vector<CContentCGF*> cgfArray;

		if (!cdfasset)
		{
			pChunkFile = new CChunkFile();
			CContentCGF* const pCGF = cgfLoader.LoadCGF(sourceFile, *pChunkFile, &listener);
			if (pCGF == NULL)
			{
				RCLogError("%s: Failed to load cgf file for file %s. %s", __FUNCTION__, sourceFile.c_str(), cgfLoader.GetLastError());
				return false;
			}
			pCGF->SetFilename(outputFile.c_str());
			cgfArray.push_back(pCGF);
		}
		else
		{
			if (!LoadCDFAssets(sourceFile, outputFile, &cgfLoader, &listener, &cgfArray))
			{
				RCLogError("%s: Failed to load cdf file for file %s", __FUNCTION__, sourceFile.c_str());
				return false;
			}
		}

		const int cgfcount = cgfArray.size();
		for (int idx = 0; idx < cgfcount; ++idx)
		{
			CContentCGF* const pCGF = cgfArray[idx];
			outputFile = pCGF->GetFilename();
			if (!ProcessInternal(&cgfLoader, pCGF, pChunkFile, cdfasset, sourceFile, outputFile))
			{
				ok = false;
			}
		}
	}
	catch (char*)
	{
		ok = false;
	}

	return ok;
}

void CharacterCompiler::ProcessVCloth(
	SVClothInfoCGF* pVClothInfo,
	std::vector<Vec3> const& vtx,
	std::vector<int> const& idx,
	std::vector<bool> const& attached)
{

	AttachmentVClothPreProcess pre;
	pre.PreProcess(vtx, idx, attached);

	// get preprocess data

	std::vector<AttachmentVClothPreProcessNndc> const& preNndc = pre.GetNndc();
	for (int i = 0; i < preNndc.size(); i++)
	{
		SVClothVertex vertex;
		vertex.attributes.nndcIdx = preNndc[i].nndcIdx;
		vertex.attributes.nndcNextParent = preNndc[i].nndcNextParent;
		vertex.attributes.nndcDist = preNndc[i].nndcDist;

		pVClothInfo->m_vertices.push_back(vertex);
	}

	std::vector<SBendTriangle> const& preBt = pre.GetListBendTriangle();
	for (int i = 0; i < preBt.size(); i++)
	{
		SVClothBendTriangle bt;
		bt.p0 = preBt[i].p0;
		bt.p1 = preBt[i].p1;
		bt.p2 = preBt[i].p2;

		pVClothInfo->m_triangles.push_back(bt);
	}

	std::vector<SBendTrianglePair> const& preBtp = pre.GetListBendTrianglePair();
	for (int i = 0; i < preBtp.size(); i++)
	{
		SVClothBendTrianglePair btp;
		btp.angle = preBtp[i].phi0;
		btp.p0 = preBtp[i].p0;
		btp.p1 = preBtp[i].p1;
		btp.p2 = preBtp[i].p2;
		btp.p3 = preBtp[i].p3;
		btp.idxNormal0 = preBtp[i].idxNormal0;
		btp.idxNormal1 = preBtp[i].idxNormal1;

		pVClothInfo->m_trianglePairs.push_back(btp);
	}

	std::vector<int> const& preNndcNotAttachedOrderedIdx = pre.GetNndcNotAttachedOrderedIdx();
	for (int i = 0; i < preNndcNotAttachedOrderedIdx.size(); i++)
	{
		SVClothNndcNotAttachedOrderedIdx nndc;
		nndc.nndcNotAttachedOrderedIdx = preNndcNotAttachedOrderedIdx[i];

		pVClothInfo->m_nndcNotAttachedOrderedIdx.push_back(nndc);
	}

	for (int e = 0; e < eVClothLink_COUNT; e++)
	{
		std::vector<SLink> const& preLinks = pre.GetLinks(e);
		for (int i = 0; i < preLinks.size(); i++)
		{
			SVClothLink l;
			l.i1 = preLinks[i].i1;
			l.i2 = preLinks[i].i2;
			l.lenSqr = preLinks[i].lenSqr;
			pVClothInfo->m_links[e].push_back(l);
		}
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
bool CharacterCompiler::ProcessInternal(CLoaderCGF* cgfLoader, CContentCGF* pCGF, CChunkFile* chunkFile, bool cdfasset, const string& sourceFile, const string& outputFile)
{
	if (cdfasset)
	{
		chunkFile = new CChunkFile();
	}

	int nStripMeshData = m_CC.config->GetAsInt("StripMesh", 0, 1);
	const bool bStripNonMeshData = m_CC.config->GetAsBool("StripNonMesh", false, true);
	const bool bCompactVertexStreams = m_CC.config->GetAsBool("CompactVertexStreams", false, true);
	const bool bComputeSubsetTexelDensity = m_CC.config->GetAsBool("ComputeSubsetTexelDensity", false, true);
	const bool bMakeVCloth = m_CC.config->GetAsBool("forceVCloth", false, true) || pCGF->GetExportInfo()->bMakeVCloth;

	bool bStorePositionsAsF16;
	{
		const char* const optionName = "vertexPositionFormat";
		const string s = m_CC.config->GetAsString(optionName, "exporter", "f32");

		if (StringHelpers::EqualsIgnoreCase(s, "f32"))
		{
			bStorePositionsAsF16 = false;
		}
		else if (StringHelpers::EqualsIgnoreCase(s, "f16"))
		{
			bStorePositionsAsF16 = true;
		}
		else if (StringHelpers::EqualsIgnoreCase(s, "exporter"))
		{
			bStorePositionsAsF16 = !pCGF->GetExportInfo()->bWantF32Vertices;
		}
		else
		{
			RCLogError("Unknown value of '%s': '%s'. Valid values are: 'f32', 'f16', 'exporter'.", optionName, s.c_str());
			return false;
		}
	}

	if (m_CC.config->GetAsBool("debugvalidate", false, true))
	{
		bool debugValidateCGF(CContentCGF* pCGF, const char* a_filename);
		debugValidateCGF(pCGF, sourceFile.c_str());
		return true;
	}

	const bool bNeedEndianSwap = (m_CC.pRC->GetPlatformInfo(m_CC.platform)->bBigEndian != SYSTEM_IS_BIG_ENDIAN);

	const bool bUseQuaternions = m_CC.config->GetAsBool("qtangents", false, true);


	// Delete Node and Mesh chunks from CGF chunk file.
	DeleteOldChunks(pCGF, *chunkFile);
	CSkinningInfo* pSkinningInfo = pCGF->GetSkinningInfo();

	const string name = StringHelpers::MakeLowerCase(sourceFile);

	string name_woLOD(name);
	int pos = name_woLOD.find("_lod");
	if (pos != string::npos) {
		name_woLOD = name_woLOD.erase(pos, name_woLOD.length() - pos) + ".chr";
	}

	const uint32 numPhysicalProxies = pSkinningInfo->m_arrPhyBoneMeshes.size();
	const uint32 numMorphtargets = pSkinningInfo->m_arrMorphTargets.size();

	// Mesh data is needed by morph target load
	const bool bNeedsFullMesh = numMorphtargets > 0;

	if (bNeedsFullMesh)
	{
		nStripMeshData = 0;
	}

	const bool bStripForSkin = nStripMeshData == 3;
	const bool bStripForModel = nStripMeshData == 4;

	// Save modified content to the same chunk file.
	CSaverCGF cgfSaver(*chunkFile);
	{
		cgfSaver.SetMeshDataSaving(nStripMeshData == 0);
		cgfSaver.SetNonMeshDataSaving(!bStripNonMeshData);
		cgfSaver.SetSavePhysicsMeshes(!bStripForSkin && !bStripNonMeshData);
		cgfSaver.SetVertexStreamCompacting(!bNeedsFullMesh && bCompactVertexStreams);
		cgfSaver.SetSubsetTexelDensityComputing(bComputeSubsetTexelDensity);
	}

	if (cdfasset)
	{
		cgfSaver.SaveMaterial(pCGF->GetCommonMaterial(), bNeedEndianSwap);
	}

	if (bStripNonMeshData || bStripForSkin || bStripForModel)
	{
		// Start from a blank slate for stripped skins and mesh files
		chunkFile->Clear();
	}

	std::vector<IntMeshCollisionInfo> arrCollisions;
	std::vector<CryBoneDescData_Comp> bones;
	std::vector<BONE_ENTITY> physBones;
	std::vector<uint8> physProxyChunkData;
	std::vector<uint8> morphTargetsChunkData;
	std::vector<IntSkinVertex> intSkinVertices;
	std::vector<TFace> intFaces;
	std::vector<uint16> ext2IntMap;

	//---------------------------------------------------------------------
	//---  write compiled bones to chunk
	//---------------------------------------------------------------------
	if (!bStripNonMeshData)
	{
		uint32 numAnimBones = pSkinningInfo->m_arrBonesDesc.size();
		if (numAnimBones)
		{
			bones.reserve(numAnimBones);
			for (uint32 i = 0; i < numAnimBones; ++i)
			{
				CryBoneDescData_Comp t;
				CopyBoneDescData(t, pSkinningInfo->m_arrBonesDesc[i]);
				bones.push_back(t);
			}
			SwapEndian(&bones[0], numAnimBones, bNeedEndianSwap);
		}
	}

	if (!bStripNonMeshData && !bStripForSkin)
	{
		//---------------------------------------------------------------------
		//---  write compiled physical bones to chunk
		//---------------------------------------------------------------------
		uint32 numPhyBones = pSkinningInfo->m_arrBonesDesc.size();
		physBones.reserve(numPhyBones);

		if (numPhyBones > 0)
		{
			std::copy(&pSkinningInfo->m_arrBoneEntities[0], &pSkinningInfo->m_arrBoneEntities[0] + numPhyBones, std::back_inserter(physBones));
			SwapEndian(&physBones[0], physBones.size(), bNeedEndianSwap);
		}

		//---------------------------------------------------------------------
		//--- create compiled physical proxi mesh and write it to chunk     ---
		//---------------------------------------------------------------------

		for (uint32 i = 0; i<numPhysicalProxies; i++)
		{
			SMeshPhysicalProxyHeader header;
			header.ChunkID = pSkinningInfo->m_arrPhyBoneMeshes[i].ChunkID;
			header.numPoints = pSkinningInfo->m_arrPhyBoneMeshes[i].m_arrPoints.size();
			header.numIndices = pSkinningInfo->m_arrPhyBoneMeshes[i].m_arrIndices.size();
			header.numMaterials = pSkinningInfo->m_arrPhyBoneMeshes[i].m_arrMaterials.size();

			WriteData(physProxyChunkData, &header, 1, bNeedEndianSwap);
			WriteArray(physProxyChunkData, pSkinningInfo->m_arrPhyBoneMeshes[i].m_arrPoints, bNeedEndianSwap);
			WriteArray(physProxyChunkData, pSkinningInfo->m_arrPhyBoneMeshes[i].m_arrIndices, bNeedEndianSwap);
			WriteArray(physProxyChunkData, pSkinningInfo->m_arrPhyBoneMeshes[i].m_arrMaterials, bNeedEndianSwap);
		}

		//---------------------------------------------------------------------
		//--- create compiled internal morph-targets and write them to chnk ---
		//---------------------------------------------------------------------

		for (uint32 i = 0; i<numMorphtargets; i++)
		{
			SMeshMorphTargetHeader header;
			//store the mesh ID
			header.MeshID = pSkinningInfo->m_arrMorphTargets[i]->MeshID;
			//store the name of morph-target
			header.NameLength = pSkinningInfo->m_arrMorphTargets[i]->m_strName.size() + 1;
			//store the vertices&indices of morph-target
			header.numIntVertices = pSkinningInfo->m_arrMorphTargets[i]->m_arrIntMorph.size();
			//store the vertices&indices of morph-target
			header.numExtVertices = pSkinningInfo->m_arrMorphTargets[i]->m_arrExtMorph.size();


			WriteData(morphTargetsChunkData, &header, 1, bNeedEndianSwap);

			WriteArray(morphTargetsChunkData, pSkinningInfo->m_arrMorphTargets[i]->m_strName, bNeedEndianSwap);
			// Add ending null to name.
			morphTargetsChunkData.push_back(0);

			WriteArray(morphTargetsChunkData, pSkinningInfo->m_arrMorphTargets[i]->m_arrIntMorph, bNeedEndianSwap);

			WriteArray(morphTargetsChunkData, pSkinningInfo->m_arrMorphTargets[i]->m_arrExtMorph, bNeedEndianSwap);
		}


		std::set<int> usedBoneslist;
		std::vector<int> useRemap(pSkinningInfo->m_arrBonesDesc.size());

		for (uint32 e = 0; e<pSkinningInfo->m_arrIntVertices.size()/*m_arrExt2IntMap.size()*/; e++)
		{
			//uint32 i=pSkinningInfo->m_arrExt2IntMap[e];

			const int idx0 = (int)pSkinningInfo->m_arrIntVertices[e].boneIDs[0];
			const int idx1 = (int)pSkinningInfo->m_arrIntVertices[e].boneIDs[1];
			const int idx2 = (int)pSkinningInfo->m_arrIntVertices[e].boneIDs[2];
			const int idx3 = (int)pSkinningInfo->m_arrIntVertices[e].boneIDs[3];

			usedBoneslist.insert(idx0);
			usedBoneslist.insert(idx1);
			usedBoneslist.insert(idx2);
			usedBoneslist.insert(idx3);
		}

		int remap = 0;
		for (std::set<int>::iterator it = usedBoneslist.begin(), end = usedBoneslist.end(); it != end; ++it, ++remap)
		{
			IntMeshCollisionInfo meshCollision;
			meshCollision.m_aABB = AABB(Vec3(VMAX), Vec3(VMIN));
			meshCollision.m_iBoneId = *it;
			arrCollisions.push_back(meshCollision);
			useRemap[*it] = remap;
		}

		for (uint32 e = 0; e<pSkinningInfo->m_arrExt2IntMap.size(); e++)
		{
			uint32 i = pSkinningInfo->m_arrExt2IntMap[e];

			//uint32 i= std::distance(pSkinningInfo->m_arrExt2IntMap.begin(), (std::find(pSkinningInfo->m_arrExt2IntMap.begin(), pSkinningInfo->m_arrExt2IntMap.end(), e)));

			const int idx0 = (int)pSkinningInfo->m_arrIntVertices[i].boneIDs[0];
			const int idx1 = (int)pSkinningInfo->m_arrIntVertices[i].boneIDs[1];
			const int idx2 = (int)pSkinningInfo->m_arrIntVertices[i].boneIDs[2];
			const int idx3 = (int)pSkinningInfo->m_arrIntVertices[i].boneIDs[3];

			arrCollisions[useRemap[idx0]].m_aABB.Add(pSkinningInfo->m_arrIntVertices[i].pos);
			arrCollisions[useRemap[idx1]].m_aABB.Add(pSkinningInfo->m_arrIntVertices[i].pos);
			arrCollisions[useRemap[idx2]].m_aABB.Add(pSkinningInfo->m_arrIntVertices[i].pos);
			arrCollisions[useRemap[idx3]].m_aABB.Add(pSkinningInfo->m_arrIntVertices[i].pos);

			arrCollisions[useRemap[idx0]].m_arrIndexes.push_back(e);
			arrCollisions[useRemap[idx1]].m_arrIndexes.push_back(e);
			arrCollisions[useRemap[idx2]].m_arrIndexes.push_back(e);
			arrCollisions[useRemap[idx3]].m_arrIndexes.push_back(e);

			/*
			Vec3 binormal	= Vec3( tPackB2F(pMesh->m_pTangents[e].Binormal.x),tPackB2F(pMesh->m_pTangents[e].Binormal.y),tPackB2F(pMesh->m_pTangents[e].Binormal.z) );
			Vec3 b = z180*binormal;
			pMesh->m_pTangents[e].Binormal = Vec4sf(tPackF2B(b.x),tPackF2B(b.y),tPackF2B(b.z), pMesh->m_pTangents[e].Binormal.w);

			Vec3 tangent	= Vec3( tPackB2F(pMesh->m_pTangents[e].Tangent.x),tPackB2F(pMesh->m_pTangents[e].Tangent.y),tPackB2F(pMesh->m_pTangents[e].Tangent.z) );
			Vec3 t = z180*tangent;
			pMesh->m_pTangents[e].Tangent = Vec4sf(tPackF2B(t.x),tPackF2B(t.y),tPackF2B(t.z), pMesh->m_pTangents[e].Tangent.w);
			*/
		}

		if (!bStripForModel)
		{
			//---------------------------------------------------------------------
			//---  write internal skinning vertices to chunk
			//---------------------------------------------------------------------
			uint32 numIntVertices = pSkinningInfo->m_arrIntVertices.size();
			intSkinVertices.reserve(numIntVertices);

			if (numIntVertices>0)
			{
				std::copy(&pSkinningInfo->m_arrIntVertices[0], &pSkinningInfo->m_arrIntVertices[0] + numIntVertices, std::back_inserter(intSkinVertices));
				SwapEndian(&intSkinVertices[0], numIntVertices, bNeedEndianSwap);
			}

			//---------------------------------------------------------------------
			//---  write internal faces to chunk
			//---------------------------------------------------------------------
			uint32 numIntFaces = pSkinningInfo->m_arrIntFaces.size();
			intFaces.reserve(numIntFaces);
			if (numIntFaces > 0)
			{
				std::copy(&pSkinningInfo->m_arrIntFaces[0], &pSkinningInfo->m_arrIntFaces[0] + numIntFaces, std::back_inserter(intFaces));
				SwapEndian(&intFaces[0], numIntFaces, bNeedEndianSwap);
			}

			//---------------------------------------------------------------------
			//---  write Ext2IntMap to chunk
			//---------------------------------------------------------------------
			uint32 numExt2Int = pSkinningInfo->m_arrExt2IntMap.size();
			ext2IntMap.reserve(numExt2Int);

			if (numExt2Int > 0)
			{
				std::copy(&pSkinningInfo->m_arrExt2IntMap[0], &pSkinningInfo->m_arrExt2IntMap[0] + numExt2Int, std::back_inserter(ext2IntMap));
				SwapEndian(&ext2IntMap[0], numExt2Int, bNeedEndianSwap);
			}
		}
	}

	//-----------------------------------------------------------------------------------------------

	const SFileVersion& fv = m_CC.pRC->GetFileVersion();
	CContentCGF* pCContentCGF = cgfLoader->GetCContentCGF();

	if (pCContentCGF == 0)
	{
		pCContentCGF = MakeCompiledCGF(pCGF);
		if (!pCContentCGF)
		{
			return false;
		}
	}


	if (pCContentCGF)
	{
		pCContentCGF->GetExportInfo()->rc_version[0] = fv.v[0];
		pCContentCGF->GetExportInfo()->rc_version[1] = fv.v[1];
		pCContentCGF->GetExportInfo()->rc_version[2] = fv.v[2];
		pCContentCGF->GetExportInfo()->rc_version[3] = fv.v[3];

		StringHelpers::SafeCopyPadZeros(
			pCContentCGF->GetExportInfo()->rc_version_string,
			sizeof(pCContentCGF->GetExportInfo()->rc_version_string),
			StringHelpers::Format(" RCVer:%d.%d ", fv.v[2], fv.v[1]).c_str());

		cgfSaver.SetContent(pCContentCGF);
	}

	if (bStripNonMeshData || bStripForSkin || bStripForModel)
	{
		// All existing chunks were cleared - read the materials
		cgfSaver.SaveMaterials(bNeedEndianSwap);
	}

	if (!bones.empty())
	{
		cgfSaver.SaveCompiledBones(bNeedEndianSwap, &bones[0], bones.size() * sizeof(CryBoneDescData_Comp), COMPILED_BONE_CHUNK_DESC_0800::VERSION);
	}
	if (!physBones.empty())
	{
		cgfSaver.SaveCompiledPhysicalBones(bNeedEndianSwap, &physBones[0], physBones.size()*sizeof(BONE_ENTITY), COMPILED_PHYSICALBONE_CHUNK_DESC_0800::VERSION);
	}
	if (!physProxyChunkData.empty())
	{
		cgfSaver.SaveCompiledPhysicalProxis(bNeedEndianSwap, &physProxyChunkData[0], physProxyChunkData.size(), numPhysicalProxies, COMPILED_PHYSICALPROXY_CHUNK_DESC_0800::VERSION);
	}
	if (!morphTargetsChunkData.empty())
	{
		cgfSaver.SaveCompiledMorphTargets(bNeedEndianSwap, &morphTargetsChunkData[0], morphTargetsChunkData.size(), numMorphtargets, COMPILED_MORPHTARGETS_CHUNK_DESC_0800::VERSION1);
	}
	if (!intSkinVertices.empty())
	{
		cgfSaver.SaveCompiledIntSkinVertices(bNeedEndianSwap, &intSkinVertices[0], intSkinVertices.size()*sizeof(IntSkinVertex), COMPILED_INTSKINVERTICES_CHUNK_DESC_0800::VERSION);
	}
	if (!intFaces.empty())
	{
		cgfSaver.SaveCompiledIntFaces(bNeedEndianSwap, &intFaces[0], intFaces.size()*sizeof(TFace), COMPILED_INTFACES_CHUNK_DESC_0800::VERSION);
	}
	if (!ext2IntMap.empty())
	{
		cgfSaver.SaveCompiledExt2IntMap(bNeedEndianSwap, &ext2IntMap[0], ext2IntMap.size()*sizeof(uint16), COMPILED_EXT2INTMAP_CHUNK_DESC_0800::VERSION);
	}

	if (!bStripNonMeshData && !bStripForSkin)
	{
		for (uint32 c = 0, endc = arrCollisions.size(); c < endc; ++c)
		{
			std::vector<char> data;

			data.resize(sizeof(int)+sizeof(AABB)+sizeof(int)+arrCollisions[c].m_arrIndexes.size() * sizeof(short int));
			char * pData = &data[0];
			int32 size = (int32)arrCollisions[c].m_arrIndexes.size();
			if (bNeedEndianSwap)
			{
				SwapEndian(arrCollisions[c].m_iBoneId, true);
				SwapEndian(size, true);
				SwapEndian(arrCollisions[c].m_aABB, true);
			}
			memcpy(pData, &arrCollisions[c].m_iBoneId, sizeof(int));
			pData += sizeof(int);
			memcpy(pData, &arrCollisions[c].m_aABB, sizeof(AABB));
			pData += sizeof(AABB);

			memcpy(pData, &size, sizeof(size));
			pData += sizeof(int);
			if (size)
			{
				memcpy(pData, &arrCollisions[c].m_arrIndexes[0], arrCollisions[c].m_arrIndexes.size() * sizeof(short int));
			}

			cgfSaver.SaveCompiledBoneBox(bNeedEndianSwap, &data[0], data.size(), COMPILED_BONEBOXES_CHUNK_DESC_0800::VERSION1);
		}
	}


	bool bHasMeshWithColors = false;
	for (int i = 0; i < pCContentCGF->GetNodeCount(); i++)
	{
		if (pCContentCGF->GetNode(i)->pMesh == nullptr) continue;
		CMesh& mesh = *pCContentCGF->GetNode(i)->pMesh;
		if ((mesh.GetVertexCount()>0) && (mesh.m_pColor0 != nullptr))
		{
			bHasMeshWithColors = true;
			break;
		}
	}

	if (bMakeVCloth && bHasMeshWithColors && !intSkinVertices.empty() && !intFaces.empty())
	{
		SVClothInfoCGF* pVClothInfo = pCContentCGF->GetVClothInfo();

		// search for mesh
		CMesh* mesh = nullptr;
		for (int i = 0; i <= pCContentCGF->GetNodeCount(); i++)
		{
			if (pCContentCGF->GetNode(i)->pMesh == nullptr) continue;
			mesh = pCContentCGF->GetNode(i)->pMesh;
			break;
		}

		bool ok = true;
		int nVtx = mesh->GetVertexCount();
		std::vector<Vec3> vtx;
		vtx.resize(nVtx);
		if (vtx.size() == 0) ok = false;
		std::copy(&mesh->m_pPositions[0], &mesh->m_pPositions[0] + vtx.size(), vtx.begin());

		std::vector<int> idx;
		idx.resize(mesh->GetIndexCount());
		if (idx.size() == 0) ok = false;
		std::copy(&mesh->m_pIndices[0], &mesh->m_pIndices[0] + idx.size(), idx.begin());

		std::vector<bool> attached;
		attached.resize(nVtx);
		for (int i = 0; i < nVtx; i++)
		{
			ColorB c = mesh->m_pColor0[i].GetRGBA();
			attached[i] = AttachmentVClothPreProcess::IsAttached(1.f - (float)c.r / 255.f);
		}

		AttachmentVClothPreProcess::PruneWeldedVertices(vtx, idx, attached);
		AttachmentVClothPreProcess::RemoveDegeneratedTriangles(vtx, idx, attached);

		ProcessVCloth(pVClothInfo, vtx, idx, attached);

		cgfSaver.SaveVCloth(bNeedEndianSwap);
	}

	if (pCContentCGF)
	{
		// Only store 
		cgfSaver.SaveExportFlags(bNeedEndianSwap);
	}

	const bool bStoreIndicesAsU16 = true;

	cgfSaver.SaveNodes(bNeedEndianSwap, !bNeedsFullMesh && bStorePositionsAsF16, bUseQuaternions, bStoreIndicesAsU16, NULL);

	bool ok = CgfUtil::WriteTempRename(*chunkFile, outputFile);

	if (cdfasset)
	{
		delete chunkFile;
	}

	if (ok)
	{
		if (!UpToDateFileHelpers::SetMatchingFileTime(GetOutputPath(), m_CC.GetSourcePath()))
		{
			return false;
		}
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
		ok = m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { GetOutputPath() });
	}

	return ok;
}

//////////////////////////////////////////////////////////////////////////
void CharacterCompiler::DeleteOldChunks(CContentCGF *pCGF, CChunkFile &chunkFile)
{
	for (int i = 0; i < pCGF->GetNodeCount(); i++)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		if ((pNode->type == CNodeCGF::NODE_MESH || pNode->type == CNodeCGF::NODE_HELPER) &&
			pNode->nChunkId)
		{
			chunkFile.DeleteChunkById(pNode->nChunkId); // Delete chunk of node.
			if (pNode->nObjectChunkId)
			{
				chunkFile.DeleteChunkById(pNode->nObjectChunkId); // Delete chunk of mesh.
			}
		}
	}

	chunkFile.DeleteChunksByType(ChunkType_Mesh);
	chunkFile.DeleteChunksByType(ChunkType_MeshSubsets);
	chunkFile.DeleteChunksByType(ChunkType_DataStream);
	chunkFile.DeleteChunksByType(ChunkType_MeshPhysicsData);
	chunkFile.DeleteChunksByType(ChunkType_ExportFlags);
	chunkFile.DeleteChunksByType(ChunkType_MeshMorphTarget);
	chunkFile.DeleteChunksByType(ChunkType_BoneNameList);
	chunkFile.DeleteChunksByType(ChunkType_BoneInitialPos);
	chunkFile.DeleteChunksByType(ChunkType_BoneAnim);
	chunkFile.DeleteChunksByType(ChunkType_BoneMesh);
	chunkFile.DeleteChunksByType(ChunkType_CompiledBones);
	chunkFile.DeleteChunksByType(ChunkType_CompiledPhysicalBones);
	chunkFile.DeleteChunksByType(ChunkType_CompiledPhysicalProxies);
	chunkFile.DeleteChunksByType(ChunkType_CompiledMorphTargets);
	chunkFile.DeleteChunksByType(ChunkType_CompiledIntFaces);
	chunkFile.DeleteChunksByType(ChunkType_CompiledIntSkinVertices);
	chunkFile.DeleteChunksByType(ChunkType_CompiledExt2IntMap);
	chunkFile.DeleteChunksByType(ChunkType_BonesBoxes);
	chunkFile.DeleteChunksByType(ChunkType_VertAnim);
	chunkFile.DeleteChunksByType(ChunkType_SceneProps);
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CharacterCompiler::MakeCompiledCGF(CContentCGF *pCGF)
{
	CContentCGF *pCompiledCGF = new CContentCGF(pCGF->GetFilename());
	*pCompiledCGF->GetExportInfo() = *pCGF->GetExportInfo(); // Copy export info.

	{
		// Compile meshes in nodes.
		for (int i = 0, num = pCGF->GetNodeCount(); i < num; ++i)
		{
			CNodeCGF *pNodeCGF = pCGF->GetNode(i);
			if (!pNodeCGF->pMesh || pNodeCGF->bPhysicsProxy)
				continue;

			mesh_compiler::CMeshCompiler meshCompiler;
			const int compileFlags =
				mesh_compiler::MESH_COMPILE_TANGENTS |
				mesh_compiler::MESH_COMPILE_OPTIMIZE |
				mesh_compiler::MESH_COMPILE_VALIDATE;
			if (!meshCompiler.Compile(*pNodeCGF->pMesh, compileFlags))
			{
				RCLogError("Failed to compile geometry file %s - %s", pCGF->GetFilename(), meshCompiler.GetLastError());
				delete pCompiledCGF;
				return 0;
			}
		}

		for (int i = 0, num = pCGF->GetNodeCount(); i < num; ++i)
		{
			CNodeCGF *pNodeCGF = pCGF->GetNode(i);
			if (pNodeCGF->pMesh && (pNodeCGF->type == CNodeCGF::NODE_MESH))
			{
				pCompiledCGF->AddNode(pNodeCGF);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Compile physics proxy nodes.
	//////////////////////////////////////////////////////////////////////////
	if (pCGF->GetExportInfo()->bHavePhysicsProxy)
	{
		for (int i = 0, num = pCGF->GetNodeCount(); i < num; ++i)
		{
			CNodeCGF *pNodeCGF = pCGF->GetNode(i);
			if (pNodeCGF->pMesh && pNodeCGF->bPhysicsProxy)
			{
				// Compile physics proxy mesh.
				mesh_compiler::CMeshCompiler meshCompiler;
				if (!meshCompiler.Compile(*pNodeCGF->pMesh, mesh_compiler::MESH_COMPILE_OPTIMIZE))
				{
					RCLogError("Failed to compile geometry in node %s in file %s - %s", pNodeCGF->name, pCGF->GetFilename(), meshCompiler.GetLastError());
					delete pCompiledCGF;
					return 0;
				}
			}
			pCompiledCGF->AddNode(pNodeCGF);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	const bool ok = Physicalize(pCompiledCGF);
	if (!ok)
	{
		RCLogError("Failed to physicalize geometry in file %s (bad vertex coordinates in source mesh, probably)", pCGF->GetFilename());
		delete pCompiledCGF;
		return 0;
	}

	return pCompiledCGF;
}

//////////////////////////////////////////////////////////////////////////
bool CharacterCompiler::Physicalize(CContentCGF *pCGF)
{
	// _EM_INVALID is used to avoid Floating Point Exception inside CryPhysics
	MathHelpers::AutoFloatingPointExceptions autoFpe(~(_EM_INEXACT | _EM_UNDERFLOW | _EM_INVALID));

	if (!m_pPhysicsInterface)
	{
		m_pPhysicsInterface = new CPhysicsInterface;
	}

	if (!m_pPhysicsInterface->GetGeomManager())
	{
		return false;
	}

	for (int i = 0; i < pCGF->GetNodeCount(); i++)
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);
		if (pNode->pMesh &&
			m_pPhysicsInterface->Physicalize(pNode, pCGF) == CPhysicsInterface::ePR_Fail)
		{
			return false;
		}
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
/// CDF MERGING CODE ///
bool CharacterCompiler::LoadCDFAssets(const char * sourceFile, const char * finalfilename, CLoaderCGF * cgfLoader, ILoaderCGFListener * pListener, std::vector<CContentCGF*> * cgfArray)
{
	XmlNodeRef config = m_CC.pRC->LoadXml("objects/characters/character_merging_config.xml");
	if (!config)
	{
		RCLog("[chrmerge] CDF Asset - failed to load attachment merging configuration file!");
		return false;
	}

	//map of skin and material xml
	LodAttachmentMap attachments;
	const ECdfLoadResult result = FindAllAttachmentModels(sourceFile, cgfLoader, pListener, &attachments, config);
	if (result != eCdfLoadResult_Success)
	{
		if (result == eCdfLoadResult_Failed)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	CMaterialCGF * pMaterial = NULL;
	XmlNodeRef basematerial = NULL;
	for (LodAttachmentMap::iterator item = attachments.begin(), end = attachments.end(); item != end; ++item)
	{
		CContentCGF * pContent = NULL;
		string newlodpath(finalfilename);
		if (item->first != 0)
		{
			if (attachments[0].size() != attachments[item->first].size())
			{
				RCLog("[chrmerge] CDF Asset - failed to load the lod attachments for merging lod %d!", item->first);
				continue;
			}

			string fullpath(finalfilename);
			string newext;
			newext.Format("_lod%d.skin", item->first);
			PathUtil::RemoveExtension(fullpath);
			newlodpath.Format("%s%s", fullpath, newext);
		}

		pContent = MakeMergedCGF(newlodpath.c_str(), pMaterial, item->second, config, item->first != 0, basematerial);
		if (!pContent)
		{
			return false;
		}

		if (pMaterial == NULL)
		{
			pMaterial = pContent->GetCommonMaterial();
			string materialpath = PathUtil::ReplaceExtension(pContent->GetFilename(), "mtl");
			basematerial = m_CC.pRC->LoadXml(materialpath.c_str());
		}

		pContent->SetFilename(newlodpath.c_str());
		cgfArray->push_back(pContent);
	}

	return true;
}

bool CharacterCompiler::MaterialValidForMerge(XmlNodeRef mtlcontents, XmlNodeRef setupxml)
{
	const int batchcount = setupxml->getChildCount();

	XmlNodeRef children = mtlcontents->findChild("SubMaterials");
	const int childcount = children->getChildCount();
	for (int idx = 0; idx < childcount; ++idx)
	{
		XmlNodeRef child = children->getChild(idx);
		string materialName(child->getAttr("Name"));
		bool found = false;
		for (int batchidx = 0; batchidx < batchcount; ++batchidx)
		{
			XmlNodeRef batch = setupxml->getChild(batchidx);
			string batchName(batch->getAttr("wildcard"));
			if (materialName.find(batchName) != string::npos)
			{
				found = true;
				break;
			}
		}

		if (found == false)
		{
			RCLogError("[chrmerge] CDF Asset Material - '%s' is not in the list of valid naming conventions.", materialName.c_str());
			return false;
		}
	}
	return true;
}

CharacterCompiler::ECdfLoadResult CharacterCompiler::FindAllAttachmentModels(const char * sourceFile, CLoaderCGF * cgfLoader, ILoaderCGFListener * pListener, LodAttachmentMap * lods, XmlNodeRef setupxml)
{
	XmlNodeRef root = m_CC.pRC->LoadXml(sourceFile);
	if (!root)
	{
		RCLogError("[chrmerge] CDF Asset - failed to load cdf combined character setup file '%s'", sourceFile);
		return eCdfLoadResult_Failed;
	}

	XmlNodeRef attachmentList = root->findChild("AttachmentList");
	if (!attachmentList)
	{
		return eCdfLoadResult_NotMergeCdf;
	}

	const int childcount = attachmentList->getChildCount();
	for (int idx = 0; idx < childcount; ++idx)
	{

		XmlNodeRef attachmentNode = attachmentList->getChild(idx);
		if (!attachmentNode)
			continue;

		int flags = 0;
		if (!attachmentNode->getAttr("Flags", flags))
			continue;

		if (!(flags & FLAGS_ATTACH_COMBINEATTACHMENT))
			continue;

		//once we know were an attachment if anything else fails bail from the function and the job
		const char * skinFilename = attachmentNode->getAttr("Binding");
		if (!skinFilename)
		{
			RCLogError("[chrmerge] CDF Asset - failed to read Binding path for attachment '%s' in cdf!", attachmentNode->getAttr("AName"));
			return eCdfLoadResult_Failed;
		}

		if (skinFilename[0] == '\0')
		{
			RCLog("[chrmerge] CDF Attachment - has empty binding string '%s'", attachmentNode->getAttr("AName"));
			continue;
		}

		if (!FileUtil::FileExists(skinFilename))
		{
			RCLogError("[chrmerge] CDF Asset - '%s' attachment binding file does not exist for node '%s'", skinFilename, attachmentNode->getAttr("AName"));
			return eCdfLoadResult_Failed;
		}

		string mtlname;
		const char * mtlfilename = attachmentNode->getAttr("Material");
		if (!mtlfilename || mtlfilename[0] == '\0')
		{
			mtlname = PathUtil::ReplaceExtension(skinFilename, ".mtl");
		}
		else
		{
			mtlname.Format("%s.mtl", mtlfilename);
		}

		if (!FileUtil::FileExists(mtlname.c_str()))
		{
			RCLogError("[chrmerge] CDF Asset - '%s' material file for attachment does not exist!", mtlname.c_str());
			return eCdfLoadResult_Failed;
		}

		CChunkFile chunkfile;
		CContentCGF * cgfcontents = cgfLoader->LoadCGF(skinFilename, chunkfile, pListener);
		XmlNodeRef mtlcontents = m_CC.pRC->LoadXml(mtlname);

		if (!(cgfcontents && mtlcontents))
		{
			RCLogError("[chrmerge] CDF Asset - either a model or its matching material file could not be loaded");
			return eCdfLoadResult_Failed;
		}

		if (!MaterialValidForMerge(mtlcontents, setupxml))
		{
			RCLogError("[chrmerge] CDF Asset Material - '%s' has an invalid naming convention for its submaterials", mtlname.c_str());
			return eCdfLoadResult_Failed;
		}


		SAttachmentMap attachment;
		attachment.pCGF = cgfcontents;
		attachment.xmlNode = mtlcontents;
		if (lods->find(0) == lods->end())
		{
			std::vector<SAttachmentMap> cgfVector;
			cgfVector.push_back(attachment);
			lods->insert(std::make_pair(0, cgfVector));
		}
		else
		{
			(*lods)[0].push_back(attachment);
		}

		FindAttachmentLods(cgfLoader, pListener, skinFilename, mtlcontents, lods);

	}

	if (lods->size() > 0 && !ValidateSkeletons(*lods))
	{
		RCLogError("[chrmerge] CDF Skeleton mismatch");
		return eCdfLoadResult_Failed;
	}

	return eCdfLoadResult_Success;
}

void CharacterCompiler::FindAttachmentLods(CLoaderCGF * cgfLoader, ILoaderCGFListener * pListener, const char * lod0model, XmlNodeRef material, LodAttachmentMap * lods)
{
	for (int lod = 1; lod < MAX_STATOBJ_LODS_NUM; lod++)
	{
		string lodext;
		lodext.Format("_lod%d.skin", lod);
		string filename(lod0model);
		PathUtil::RemoveExtension(filename);
		string lodfilename;
		lodfilename.Format("%s%s", filename.c_str(), lodext.c_str());

		if (FileUtil::FileExists(lodfilename.c_str()))
		{
			CChunkFile chunkfile;
			CContentCGF * cgfcontents = cgfLoader->LoadCGF(lodfilename.c_str(), chunkfile, pListener);
			if (!cgfcontents)
				break;

			SAttachmentMap attachment;
			attachment.pCGF = cgfcontents;
			attachment.xmlNode = material;
			if (lods->find(lod) == lods->end())
			{
				std::vector<SAttachmentMap> cgfVector;
				cgfVector.push_back(attachment);
				lods->insert(std::make_pair(lod, cgfVector));
			}
			else
			{
				(*lods)[lod].push_back(attachment);
			}
		}
	}
}

bool CharacterCompiler::ValidateSkeletons(const LodAttachmentMap& lods) const
{
	bool valid = true;

	LodAttachmentMap::const_iterator lod0 = lods.cbegin();

	for (std::vector<SAttachmentMap>::const_iterator model1item = (*lod0).second.cbegin(), modelend = (*lod0).second.cend(); model1item != modelend; ++model1item)
	{
		for (std::vector<SAttachmentMap>::const_iterator model2item = model1item + 1; model2item != modelend; ++model2item)
		{
			const int model1bonearraysize = (*model1item).pCGF->GetSkinningInfo()->m_arrBonesDesc.size();
			const int model2bonearraysize = (*model2item).pCGF->GetSkinningInfo()->m_arrBonesDesc.size();

			if (model1bonearraysize != model2bonearraysize)
			{
				valid = false;
				RCLogError("[chrmerge] CDF Skeleton mismatch - models '%s' and '%s' have different skeletons bone count.", (*model1item).pCGF->GetFilename(), (*model2item).pCGF->GetFilename());
			}

			for (int idx = 0; idx < model1bonearraysize; ++idx)
			{
				const char * name1 = (*model1item).pCGF->GetSkinningInfo()->m_arrBonesDesc[idx].m_arrBoneName;
				const char * name2 = (*model2item).pCGF->GetSkinningInfo()->m_arrBonesDesc[idx].m_arrBoneName;

				if (strncmp(name1, name2, CryBoneDescData::kBoneNameMaxSize) != 0)
				{
					valid = false;
					RCLogError("[chrmerge] CDF Skeleton mismatch - models '%s' and '%s' have different skeleton bone names '%s' : '%s' for bone id '%d'.", (*model1item).pCGF->GetFilename(), (*model2item).pCGF->GetFilename(), name1, name2, idx);
				}
			}
		}
	}
	return valid;
}

CContentCGF * CharacterCompiler::MakeMergedCGF(const char * filename, CMaterialCGF * pMaterial, const std::vector<SAttachmentMap>& attachments, XmlNodeRef setupXml, bool lod, XmlNodeRef basematerial)
{
	CContentCGF * pNewContentCGF = new CContentCGF(filename);
	CNodeCGF * pNode = SetupNewMergeContents(pNewContentCGF, pMaterial);

	std::vector<CNodeCGF*> merge_nodes;
	std::vector<SMeshSubset> subsets;
	std::vector<CSkinningInfo*> skinning;
	SubsetMatMap unoptimised;
	CombineMap combinemap;


	for (std::vector<SAttachmentMap>::const_iterator item = attachments.cbegin(), end = attachments.cend(); item != end; ++item)
	{
		CollectNodesForMerging(pNode, (*item).pCGF, (*item).xmlNode, &subsets, &merge_nodes, &unoptimised, lod, basematerial, &skinning);
		MergeOtherNodes(pNewContentCGF, (*item).pCGF);
	}

	if (!DetermineMergedSubsets(&merge_nodes, &subsets, &unoptimised, setupXml, &combinemap, lod))
	{
		RCLogError("[chrmerge] CDF Asset - failed to determine optimum batch combinations");
		return NULL;
	}

	if (!MergeSubsets(combinemap, &subsets, &merge_nodes, pNewContentCGF->GetSkinningInfo(), skinning))
	{
		RCLogError("[chrmerge] CDF Asset - failed to optimise batches");
		return NULL;
	}

	if (!MergeMeshNodes(pNode, subsets, &merge_nodes))
	{
		RCLogError("[chrmerge] CDF Asset - failed to merge mesh data");
		return NULL;
	}

	if (!lod)
	{
		if (!CompileOmptimisedMaterials(pNewContentCGF, pNode, unoptimised, combinemap, setupXml))
		{
			RCLogError("[chrmerge] CDF Asset - failed to optimise materials");
			return NULL;
		}
	}

	return pNewContentCGF;
}

CNodeCGF * CharacterCompiler::SetupNewMergeContents(CContentCGF * pCGF, CMaterialCGF * pMaterial)
{
	CNodeCGF * pNode = new CNodeCGF;
	pNode->type = CNodeCGF::NODE_MESH;
	cry_strcpy(pNode->name, "Merged");
	pNode->bIdentityMatrix = true;
	pNode->pMesh = new CMesh();

	if (!pMaterial)
	{
		pMaterial = new CMaterialCGF();
		cry_strcpy(pMaterial->name, PathUtil::GetFile(pCGF->GetFilename()));
	}
	pNode->pMaterial = pMaterial;

	pCGF->AddNode(pNode);
	pCGF->SetCommonMaterial(pMaterial);
	return pNode;
}

void CharacterCompiler::CollectNodesForMerging(CNodeCGF * pTargetNode, CContentCGF * sourceCgf, XmlNodeRef sourceMtl, std::vector<SMeshSubset> * subsets, std::vector<CNodeCGF*> * merge_nodes, SubsetMatMap * unoptimised, bool lod, XmlNodeRef basematerial, std::vector<CSkinningInfo*> * skinning)
{
	for (int i = 0; i < sourceCgf->GetNodeCount(); i++)
	{
		CNodeCGF *pNode = sourceCgf->GetNode(i);
		if (pNode->pMesh && !pNode->bPhysicsProxy && pNode->type == CNodeCGF::NODE_MESH)
		{
			int subsetCount = pNode->pMesh->GetSubSetCount();
			for (int subsidx = 0; subsidx < subsetCount; ++subsidx)
			{
				const SMeshSubset & pMeshSubset = pNode->pMesh->m_subsets[subsidx];
				int newSubSetId = subsets->size();

				XmlNodeRef children = sourceMtl->findChild("SubMaterials");
				const int childcount = children->getChildCount();
				if (pMeshSubset.nMatID >= 0 && pMeshSubset.nMatID < childcount)
				{
					SSubsetMatMap newMatMap;
					newMatMap.oldSubsetId = pMeshSubset.nMatID;
					newMatMap.originalMtlXml = children->getChild(pMeshSubset.nMatID);
					newMatMap.newSubsetId = newSubSetId;
					newMatMap.merged = false;
					newMatMap.mergeTextures = false;
					unoptimised->push_back(newMatMap);

					int submatid = newSubSetId;
					if (!lod)
					{
						pTargetNode->pMaterial->subMaterials.push_back(pNode->pMaterial->subMaterials[pMeshSubset.nMatID]);
					}
					else
					{
						submatid = FindExistingSubmatId(newMatMap.originalMtlXml, basematerial);
					}

					SMeshSubset subset;
					subset.nFirstVertId = 0;
					subset.nFirstIndexId = 0;
					subset.nNumIndices = pMeshSubset.nNumIndices;
					subset.nNumVerts = pMeshSubset.nNumVerts;
					subset.fRadius = pMeshSubset.fRadius;
					subset.vCenter = pMeshSubset.vCenter;
					subset.nMatID = submatid;

					CNodeCGF * pSubSetNode = new CNodeCGF(*pNode);
					pSubSetNode->pMesh = new CMesh(*pNode->pMesh);

					pSubSetNode->pMesh->FreeStreams();

					pSubSetNode->pMesh->AppendStreamsFrom(
						*pNode->pMesh,
						pMeshSubset.nFirstVertId,
						pMeshSubset.nNumVerts,
						pMeshSubset.nFirstIndexId / 3,
						pMeshSubset.nNumIndices / 3);

					skinning->push_back(CopySkinningInfo(sourceCgf, pMeshSubset));

					pSubSetNode->pMesh->m_subsets.push_back(subset);

					subsets->push_back(subset);
					merge_nodes->push_back(pSubSetNode);
				}
			}
		}
	}
}

int CharacterCompiler::FindExistingSubmatId(XmlNodeRef sourceMtl, XmlNodeRef baseMtl)
{
	XmlNodeRef baseSubMats = baseMtl->findChild("SubMaterials");
	const int submatCount = baseSubMats->getChildCount();
	for (int idx = 0; idx < submatCount; ++idx)
	{
		XmlNodeRef child = baseSubMats->getChild(idx);
		if (CompareXmlNodesAttributes(sourceMtl, child, false))
			return idx;
	}
	return 0;
}

CSkinningInfo * CharacterCompiler::CopySkinningInfo(CContentCGF * pSourceCgf, const SMeshSubset & pMeshSubset)
{
	CSkinningInfo * pSrcSkinning = pSourceCgf->GetSkinningInfo();
	CSkinningInfo * pDstSkinning = new CSkinningInfo();

	const int startidx = pMeshSubset.nFirstVertId;
	const int length = pMeshSubset.nNumVerts;

	std::map<int, int> internalMovement;
	pDstSkinning->m_arrExt2IntMap.resize(length);

	for (int idx = 0; idx < length; ++idx)
	{
		const int internalVert = pSrcSkinning->m_arrExt2IntMap[startidx + idx];
		pDstSkinning->m_arrIntVertices.push_back(pSrcSkinning->m_arrIntVertices[internalVert]);
		pDstSkinning->m_arrExt2IntMap[idx] = idx;
		internalMovement.insert(std::make_pair(internalVert, idx));
	}

	const int nDstFaces = pMeshSubset.nNumIndices / 3;
	pDstSkinning->m_arrIntFaces.reserve(nDstFaces);

	int nCountDstFaces = 0;

	const int numIntFaces = pSrcSkinning->m_arrIntFaces.size();
	for (int idx = 0; idx < numIntFaces && nCountDstFaces < nDstFaces; ++idx)
	{
		TFace face = pSrcSkinning->m_arrIntFaces[idx];
		int idx0 = face.i0;
		int idx1 = face.i1;
		int idx2 = face.i2;

		std::map<int, int>::iterator iterIdx0 = internalMovement.find(idx0);
		std::map<int, int>::iterator iterIdx1 = internalMovement.find(idx1);
		std::map<int, int>::iterator iterIdx2 = internalMovement.find(idx2);

		if (iterIdx0 != internalMovement.end() && iterIdx1 != internalMovement.end() && iterIdx2 != internalMovement.end())
		{
			face.i0 = (*iterIdx0).second;
			face.i1 = (*iterIdx1).second;
			face.i2 = (*iterIdx2).second;
			pDstSkinning->m_arrIntFaces.push_back(face);
			nCountDstFaces++;
		}
	}

	pDstSkinning->m_arrBoneEntities = pSrcSkinning->m_arrBoneEntities;
	pDstSkinning->m_arrBonesDesc = pSrcSkinning->m_arrBonesDesc;

	pDstSkinning->m_arrCollisions.push_back(pSrcSkinning->m_arrCollisions);
	pDstSkinning->m_arrMorphTargets.push_back(pSrcSkinning->m_arrMorphTargets);

	return pDstSkinning;
}

void CharacterCompiler::MergeOtherNodes(CContentCGF * targetCgf, CContentCGF * sourceCgf)
{
	//export info
	*targetCgf->GetExportInfo() = *sourceCgf->GetExportInfo();

	const string lodNamePrefix(CGF_NODE_NAME_LOD_PREFIX);
	// Add rest of the helper nodes.
	int numNodes = sourceCgf->GetNodeCount();
	for (int i = 0; i<numNodes; i++)
	{
		CNodeCGF *pNode = sourceCgf->GetNode(i);

		if (pNode->type != CNodeCGF::NODE_MESH)
		{
			// Do not add LOD nodes.
			if (!StringHelpers::StartsWithIgnoreCase(string(pNode->name), lodNamePrefix))
			{
				if (pNode->pParent && pNode->pParent->type == CNodeCGF::NODE_MESH)
				{
					pNode->pParent = pNode;
				}
				targetCgf->AddNode(pNode);
			}
		}
	}
}

bool CharacterCompiler::DetermineMergedSubsets(std::vector<CNodeCGF*> * nodes, std::vector<SMeshSubset> * subsets, SubsetMatMap * unoptimised, XmlNodeRef setupXml, CombineMap * combinemap, bool lod)
{
	const int length = unoptimised->size();
	//determine which subsets to merge based on material entry
	for (int idx = 0; idx < length; ++idx)
	{
		if ((*unoptimised)[idx].merged)
			continue;

		std::vector<int> mergesubsets;
		combinemap->insert(std::make_pair(idx, mergesubsets));
		(*unoptimised)[idx].merged = true;
		if (!lod)
			(*subsets)[(*unoptimised)[idx].newSubsetId].nMatID = combinemap->size() - 1;

		XmlNodeRef firstXmlNode = (*unoptimised)[idx].originalMtlXml;

		//determine which subsets to merge based on material entry
		for (int comp_idx = idx + 1; comp_idx < length; ++comp_idx)
		{
			if ((*unoptimised)[comp_idx].merged == true)
				continue;

			XmlNodeRef secondXmlNode = (*unoptimised)[comp_idx].originalMtlXml;
			if (CompareXmlNodesAttributes(firstXmlNode, secondXmlNode, !lod))
			{
				(*combinemap)[idx].push_back(comp_idx);
				(*unoptimised)[comp_idx].merged = true;
				OffsetUVs((*nodes)[comp_idx], secondXmlNode, setupXml);

				if (!lod)
				{
					(*subsets)[(*unoptimised)[comp_idx].newSubsetId].nMatID = combinemap->size() - 1;
					(*unoptimised)[comp_idx].mergeTextures = true;
					(*unoptimised)[idx].mergeTextures = true;
				}
			}
		}

		int sizecmb = (*combinemap)[idx].size();
		if (sizecmb > 0)
			OffsetUVs((*nodes)[idx], (*unoptimised)[idx].originalMtlXml, setupXml);
	}
	return true;
}

bool CharacterCompiler::CompareXmlNodesAttributes(XmlNodeRef a, XmlNodeRef b, bool verbose)
{
	int attribACount = a->getNumAttributes();
	int attribBCount = b->getNumAttributes();

	if (!(attribACount > 1 && attribBCount > 1))
	{
		return false;
	}

	const char *nameA = "";
	const char *valNameA = "";
	const char *nameB = "";
	const char *valNameB = "";

	a->getAttributeByIndex(0, &nameA, &valNameA);
	b->getAttributeByIndex(0, &nameB, &valNameB);

	int similarCount = 0;
	bool ret = true;

	const char *keyA = "";
	const char *valueA = "";
	const char *keyB = "";
	const char *valueB = "";
	for (int idx = 1; idx < attribACount; ++idx, ++similarCount)
	{
		if (!(a->getAttributeByIndex(idx, &keyA, &valueA) && b->getAttributeByIndex(idx, &keyB, &valueB)))
			ret = false;

		if (stricmp(keyA, "GenMask") == 0)
			continue;

		if (stricmp(keyA, keyB) != 0)
			ret = false;

		if (stricmp(valueA, valueB) != 0)
			ret = false;

		if (ret == false)
			break;
	}

	if (verbose && ret == false && similarCount >= 2)
	{
		RCLog("[chrmerge] Potentially unoptimised materials that were suitable for merging '%s'- '%s'", valNameA, valNameB);
		RCLog("[chrmerge] material did not match because '%s' is different '%s' - '%s'", keyA, valueA, valueB);
	}

	if (verbose && ret)
		RCLog("[chrmerge] Merging materials: '%s'- '%s'", valNameA, valNameB);

	return ret;
}

float CharacterCompiler::NormalizeTexCoord(float uv, bool & normalised)
{
	while (uv > 1.0f)
	{
		uv -= 1.0f;
		normalised = true;
	}

	while (uv < 0.0f)
	{
		uv += 1.0f;
		normalised = true;
	}

	return uv;
}

void CharacterCompiler::OffsetUVs(CNodeCGF * pSubSetNode, XmlNodeRef material, XmlNodeRef setupXml)
{
	//find the offset entry in the setupxml based on the material name
	const char * materialName = material->getAttr("Name");
	if (!materialName)
	{
		RCLogError("[chrmerge] CDF Asset - material file has unexpected format");
		return;
	}

	const char * shaderType = material->getAttr("Shader");
	if (!shaderType)
	{
		RCLogError("[chrmerge] CDF Asset - material file has unexpected format");
		return;
	}

	const int uvCount = pSubSetNode->pMesh->GetTexCoordCount();
	if (uvCount == 0)
	{
		RCLogError("[chrmerge] CDF Asset - mesh does not have any uvs");
		return;
	}

	float destSizeX = 0, destSizeY = 0;
	if (!(setupXml->getAttr("destSizeX", destSizeX) && setupXml->getAttr("destSizeY", destSizeY)))
	{
		RCLogError("[chrmerge] CDF Asset - character merge setup xml has unexpected format");
		return;
	}

	bool normalisedUVs = false;
	const int childCount = setupXml->getChildCount();
	for (int idx = 0; idx < childCount; ++idx)
	{
		XmlNodeRef batchEntry = setupXml->getChild(idx);
		const char * wildCard = batchEntry->getAttr("wildcard");
		if (strstr(materialName, wildCard) && strnicmp(batchEntry->getTag(), "ignore", sizeof("ignore")) && strnicmp(shaderType, "Humanskin", sizeof("Humanskin")) && strnicmp(batchEntry->getTag(), "templates", sizeof("templates")))
		{
			float offsetX = 0, offsetY = 0, destAreaX = 0, destAreaY = 0;
			if (!(batchEntry->getAttr("offsetX", offsetX) && batchEntry->getAttr("offsetY", offsetY) && batchEntry->getAttr("destAreaX", destAreaX) && batchEntry->getAttr("destAreaY", destAreaY)))
			{
				RCLogError("[chrmerge] CDF Asset - character merge setup xml has unexpected format");
				return;
			}

			float uScale = (destAreaX / destSizeX);
			float vScale = (destAreaY / destSizeY);
			float uPos = (offsetX / destSizeX);
			float vPos = (offsetY / destSizeY);

			SMeshTexCoord * pTexCoords = pSubSetNode->pMesh->m_pTexCoord;
			for (int uvidx = 0; uvidx < uvCount; ++uvidx, ++pTexCoords)
			{
				Vec2 uv = pTexCoords->GetUV();

				float tempu = NormalizeTexCoord(uv.x, normalisedUVs);
				float tempv = NormalizeTexCoord(uv.y, normalisedUVs);

				uv.x = (tempu * uScale) + uPos;
				uv.y = (tempv * vScale) + vPos;

				*pTexCoords = SMeshTexCoord(uv);
			}
		}
	}

	if (normalisedUVs)
		RCLog("[chrmerge] UVs for '%s' have been normalized back into 0 to 1 range.", materialName);
}

bool CharacterCompiler::MergeSubsets(const CombineMap& combinemap, std::vector<SMeshSubset> * subsets, std::vector<CNodeCGF*> * merge_nodes, CSkinningInfo * pDstSkinningInfo, const std::vector<CSkinningInfo*> & skinning)
{
	//remove merged subsets and reorganise the mergenode list 
	std::vector<CNodeCGF*> newNodeOrder;
	std::vector<SMeshSubset> newSubSets;

	int vertextotal = 0;
	int indextotal = 0;

	for (CombineMap::const_iterator item = combinemap.cbegin(), end = combinemap.cend(); item != end; ++item)
	{
		CNodeCGF * pNode = (*merge_nodes)[(*item).first];
		const SMeshSubset& subsetSource = (*subsets)[(*item).first];

		SMeshSubset subset;
		subset.nMatID = subsetSource.nMatID;

		subset.nFirstVertId = vertextotal;
		subset.nFirstIndexId = indextotal;
		subset.nNumIndices += subsetSource.nNumIndices;
		subset.nNumVerts += subsetSource.nNumVerts;
		MergeSkinning(skinning[(*item).first], pDstSkinningInfo, vertextotal, subsetSource.nNumVerts, true);
		newNodeOrder.push_back(pNode);

		for (std::vector<int>::const_iterator mergeitem = (*item).second.cbegin(), mergeend = (*item).second.cend(); mergeitem != mergeend; ++mergeitem)
		{
			CNodeCGF * pMergeNode = (*merge_nodes)[(*mergeitem)];
			const SMeshSubset& subsetMergeSource = (*subsets)[(*mergeitem)];
			subset.nNumIndices += subsetMergeSource.nNumIndices;
			subset.nNumVerts += subsetMergeSource.nNumVerts;
			MergeSkinning(skinning[(*mergeitem)], pDstSkinningInfo, vertextotal, subsetMergeSource.nNumVerts, false);
			newNodeOrder.push_back(pMergeNode);
		}

		//these need fixing up somehow
		subset.fRadius = subsetSource.fRadius;
		subset.vCenter = subsetSource.vCenter;
		//----------------------

		vertextotal += subset.nNumVerts;
		indextotal += subset.nNumIndices;
		newSubSets.push_back(subset);
	}

	subsets->clear();
	merge_nodes->clear();
	subsets->insert(subsets->begin(), newSubSets.begin(), newSubSets.end());
	merge_nodes->insert(merge_nodes->begin(), newNodeOrder.begin(), newNodeOrder.end());

	return true;
}

void CharacterCompiler::MergeSkinning(CSkinningInfo * pSrc, CSkinningInfo * pDst, int nOffset, int nCount, bool master)
{
	if (master)
	{
		pDst->m_arrBoneEntities = pSrc->m_arrBoneEntities;
		pDst->m_arrBonesDesc = pSrc->m_arrBonesDesc;
	}

	pDst->m_arrCollisions.push_back(pSrc->m_arrCollisions);
	pDst->m_arrMorphTargets.push_back(pSrc->m_arrMorphTargets);
	pDst->m_arrPhyBoneMeshes.push_back(pSrc->m_arrPhyBoneMeshes);

	//pDst->m_AimDirBlends.push_back(pSrc->m_AimDirBlends);
	//pDst->m_AimIK_Pos.push_back(pSrc->m_AimIK_Pos);
	//pDst->m_AimIK_Rot.push_back(pSrc->m_AimIK_Rot);
	//pDst->m_LookDirBlends.push_back(pSrc->m_LookDirBlends);
	//pDst->m_LookIK_Pos.push_back(pSrc->m_LookIK_Pos);
	//pDst->m_LookIK_Rot.push_back(pSrc->m_LookIK_Rot);

	const int internalOffset = pDst->m_arrIntVertices.size();
	pDst->m_arrIntVertices.push_back(pSrc->m_arrIntVertices);

	const int srcIntFacesCount = pSrc->m_arrIntFaces.size();
	for (int idx = 0; idx < srcIntFacesCount; ++idx)
	{
		TFace face = pSrc->m_arrIntFaces[idx];
		face.i0 += internalOffset;
		face.i1 += internalOffset;
		face.i2 += internalOffset;
		pDst->m_arrIntFaces.push_back(face);
	}

	pDst->m_arrExt2IntMap.resize(pDst->m_arrExt2IntMap.size() + nCount, 0);

	const int srcExt2IntMapCount = pSrc->m_arrExt2IntMap.size();
	for (int idx = 0; idx < srcExt2IntMapCount; ++idx)
	{
		const int index = pSrc->m_arrExt2IntMap[idx];
		pDst->m_arrExt2IntMap[idx + nOffset] = (index + internalOffset);
	}
}

bool CharacterCompiler::MergeMeshNodes(CNodeCGF *pNode, const std::vector<SMeshSubset> & subsets, std::vector<CNodeCGF*> * merge_nodes)
{
	if (merge_nodes->empty())
	{
		RCLogError("[chrmerge] Error merging nodes, No mergeable nodes found in any attachment");
		return false;
	}

	//merges streams
	string errorMessage;
	if (!CGFNodeMerger::MergeNodes(NULL, *merge_nodes, errorMessage, pNode->pMesh))
	{
		RCLogError("[chrmerge] Error merging nodes: %s", errorMessage.c_str());
		return false;
	}

	//adds subsets
	pNode->pMesh->m_subsets.clear();
	for (std::vector<SMeshSubset>::const_iterator item = subsets.cbegin(), end = subsets.cend(); item != end; ++item)
	{
		pNode->pMesh->m_subsets.push_back((*item));
	}

	return true;
}

bool CharacterCompiler::CompileOmptimisedMaterials(CContentCGF * pMergedCgf, CNodeCGF * pNode, const SubsetMatMap& unoptimised, const CombineMap& combinedMap, XmlNodeRef setupXml)
{
	const char * filename = pMergedCgf->GetFilename();
	CMaterialCGF * pMergedMaterial = pMergedCgf->GetCommonMaterial();
	pMergedMaterial->subMaterials.clear();

	std::vector<string> ignoreList;
	const int childcount = setupXml->getChildCount();
	for (int childidx = 0; childidx < childcount; ++childidx)
	{
		XmlNodeRef child = setupXml->getChild(childidx);
		if (child && stricmp(child->getTag(), "ignore") == 0)
		{
			ignoreList.push_back(string(child->getAttr("wildcard")));
		}
	}

	XmlNodeRef mergeSetup = CreateTextureSetupFile(filename, setupXml);
	if (!mergeSetup)
	{
		return false;
	}

	std::vector<XmlNodeRef> newSubMats;
	for (CombineMap::const_iterator item = combinedMap.cbegin(), end = combinedMap.cend(); item != end; ++item)
	{
		XmlNodeRef originalXml = unoptimised[(*item).first].originalMtlXml;
		CMaterialCGF * pMaterial = new CMaterialCGF();
		const char * submatname = originalXml->getAttr("Name");
		if (ShouldCombineTextures(submatname, ignoreList) && unoptimised[(*item).first].mergeTextures == true)
		{
			CombineTextureFiles(mergeSetup, (*item), setupXml, originalXml, unoptimised);
			XmlNodeRef combinedSubMat = WriteOriginalXml(originalXml, mergeSetup);
			newSubMats.push_back(combinedSubMat);
			cry_strcpy(pMaterial->name, "merged");
		}
		else
		{
			newSubMats.push_back(originalXml);
			cry_strcpy(pMaterial->name, submatname);
		}
		pMergedMaterial->subMaterials.push_back(pMaterial);
	}

	SaveTextureMergeSetup(filename, mergeSetup);

	if (!CreateOptimisedMaterial(filename, newSubMats))
	{
		RCLog("[chrmerge] Failed to write optimized material file.");
		return false;
	}

	return true;
}

bool CharacterCompiler::ShouldCombineTextures(const char * submat, const std::vector<string> & ignorelist)
{
	for (std::vector<string>::const_iterator item = ignorelist.cbegin(), end = ignorelist.cend(); item != end; ++item)
	{
		if (strstr(submat, (*item).c_str()))
			return false;
	}
	return true;
}

XmlNodeRef CharacterCompiler::CreateTextureSetupFile(const char * filename, XmlNodeRef setupXml)
{
	int resx, resy;
	setupXml->getAttr("destSizeX", resx);
	setupXml->getAttr("destSizeY", resy);

	XmlNodeRef templates = setupXml->findChild("templates");
	if (!templates)
	{
		RCLogError("[chrmerge] character merge setup file does not contain the correct texture preset information.");
		return NULL;
	}

	XmlNodeRef diffuseTemplate = templates->findChild("diffuse");
	if (!diffuseTemplate)
	{
		RCLogError("[chrmerge] character merge setup file does not contain the correct Diffuse texture preset information.");
		return NULL;
	}

	XmlNodeRef specularTemplate = templates->findChild("specular");
	if (!specularTemplate)
	{
		RCLogError("[chrmerge] character merge setup file does not contain the correct Specular texture preset information.");
		return NULL;
	}

	XmlNodeRef bumpTemplate = templates->findChild("normal");
	if (!bumpTemplate)
	{
		RCLogError("[chrmerge] character merge setup file does not contain the correct NormalMap texture preset information.");
		return NULL;
	}

	const char * diffusePreset = diffuseTemplate->getAttr("preset");
	const char * specularPreset = specularTemplate->getAttr("preset");
	const char * bumpPreset = bumpTemplate->getAttr("preset");

	char workingDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, workingDir);
	string workDir(workingDir);
	string relativePath = PathUtil::GetPathWithoutFilename(filename);
	if (relativePath.MakeLower().find(workDir.MakeLower()) != string::npos)
	{
		relativePath = relativePath.substr(workDir.size()).TrimLeft('\\');
	}

	string partname;
	partname.Format("%s%s", relativePath.c_str(), PathUtil::GetFile(filename));
	string diffuse(partname);
	string specular(partname);
	string bumpmap(partname);
	diffuse.append("_diff.tif");
	specular.append("_spec.tif");
	bumpmap.append("_ddn.tif");

	XmlNodeRef config = m_CC.pRC->CreateXml("texturemerge");

	XmlNodeRef diffuseTextures = m_CC.pRC->CreateXml("diffuse");
	diffuseTextures->setAttr("filename", diffuse.c_str());
	diffuseTextures->setAttr("preset", diffusePreset);
	diffuseTextures->setAttr("xres", resx);
	diffuseTextures->setAttr("yres", resy);

	XmlNodeRef specularTextures = m_CC.pRC->CreateXml("specular");
	specularTextures->setAttr("filename", specular.c_str());
	specularTextures->setAttr("preset", specularPreset);
	specularTextures->setAttr("xres", resx);
	specularTextures->setAttr("yres", resy);

	XmlNodeRef bumpmapTextures = m_CC.pRC->CreateXml("bumpmap");
	bumpmapTextures->setAttr("filename", bumpmap.c_str());
	bumpmapTextures->setAttr("preset", bumpPreset);
	bumpmapTextures->setAttr("xres", resx);
	bumpmapTextures->setAttr("yres", resy);

	config->addChild(diffuseTextures);
	config->addChild(specularTextures);
	config->addChild(bumpmapTextures);

	return config;
}

void CharacterCompiler::CombineTextureFiles(XmlNodeRef texMergeSetup, const std::pair<int, std::vector<int>>& map, XmlNodeRef setupXml, XmlNodeRef originalXml, const SubsetMatMap& unoptimised)
{
	XmlNodeRef attachment = unoptimised[map.first].originalMtlXml;
	XmlNodeRef diffuseTextures = texMergeSetup->findChild("diffuse");
	XmlNodeRef specularTextures = texMergeSetup->findChild("specular");
	XmlNodeRef bumpmapTextures = texMergeSetup->findChild("bumpmap");

	WriteConfigEntry(setupXml, attachment, "diffuse", diffuseTextures);
	WriteConfigEntry(setupXml, attachment, "specular", specularTextures);
	WriteConfigEntry(setupXml, attachment, "bumpmap", bumpmapTextures);
	for (std::vector<int>::const_iterator item = map.second.cbegin(), end = map.second.cend(); item != end; ++item)
	{
		attachment = unoptimised[(*item)].originalMtlXml;
		WriteConfigEntry(setupXml, attachment, "diffuse", diffuseTextures);
		WriteConfigEntry(setupXml, attachment, "specular", specularTextures);
		WriteConfigEntry(setupXml, attachment, "bumpmap", bumpmapTextures);
	}
}

XmlNodeRef CharacterCompiler::WriteOriginalXml(XmlNodeRef originalXml, XmlNodeRef config)
{
	XmlNodeRef diffuse = config->findChild("diffuse");
	XmlNodeRef specular = config->findChild("specular");
	XmlNodeRef bumpmap = config->findChild("bumpmap");

	const char * diffuseFile = diffuse->getAttr("filename");
	const char * specularFile = specular->getAttr("filename");
	const char * bumpFile = bumpmap->getAttr("filename");

	const char * diffusePreset = diffuse->getAttr("preset");
	const char * specularPreset = specular->getAttr("preset");
	const char * bumpPreset = bumpmap->getAttr("preset");

	XmlNodeRef textures = originalXml->findChild("Textures");
	if (textures)
	{
		int childCount = textures->getChildCount();
		for (int texid = 0; texid < childCount; ++texid)
		{
			XmlNodeRef texture = textures->getChild(texid);
			const char * maptype = texture->getAttr("Map");
			if (maptype)
			{
				if (strnicmp(maptype, "diffuse", sizeof("diffuse")) == 0)
				{
					texture->setAttr("File", diffuseFile);
					texture->setAttr("Preset", diffusePreset);
				}
				else if (strnicmp(maptype, "specular", sizeof("specular")) == 0)
				{
					texture->setAttr("File", specularFile);
					texture->setAttr("Preset", specularPreset);
				}
				else if (strnicmp(maptype, "bumpmap", sizeof("bumpmap")) == 0)
				{
					texture->setAttr("File", bumpFile);
					texture->setAttr("Preset", bumpPreset);
				}
			}
		}
	}
	return originalXml;
}

void CharacterCompiler::SaveTextureMergeSetup(const char * filename, XmlNodeRef texMergeSetup)
{
	char workingDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, workingDir);
	string workDir(workingDir);
	string relativePath = PathUtil::GetPathWithoutFilename(filename);
	if (relativePath.MakeLower().find(workDir.MakeLower()) != string::npos)
	{
		relativePath = relativePath.substr(workDir.size()).TrimLeft('\\');
	}

	string partname;
	partname.Format("%s%s", relativePath.c_str(), PathUtil::GetFile(filename));
	string setupfile = partname.append("_setup.tmxs");
	texMergeSetup->saveToFile(setupfile.c_str());

	m_CC.pRC->CompileSingleFileBySingleProcess(setupfile.c_str());
}

void CharacterCompiler::WriteConfigEntry(XmlNodeRef setup, XmlNodeRef attachment, const char * slot, XmlNodeRef destination)
{
	const char * attachmentName = attachment->getAttr("Name");
	bool foundAttachment = false;
	int childcount = setup->getChildCount();
	for (int batchidx = 0; batchidx < childcount; ++batchidx)
	{
		XmlNodeRef setupChild = setup->getChild(batchidx);
		const char * wildcard = setupChild->getAttr("wildcard");

		if (!wildcard)
			continue;
		if (wildcard[0] == '\0')
			continue;

		if (strstr(attachmentName, wildcard) != NULL)
		{
			foundAttachment = true;

			int offsetx, offsety, sx, sy, dszx, dszy;
			setupChild->getAttr("offsetX", offsetx);
			setupChild->getAttr("offsetY", offsety);
			setupChild->getAttr("destAreaX", sx);
			setupChild->getAttr("destAreaY", sy);

			if (setupChild->haveAttr("dstSubAreaX"))
			{
				setupChild->getAttr("dstSubAreaX", dszx);
			}

			if (setupChild->haveAttr("dstSubAreaY"))
			{
				setupChild->getAttr("dstSubAreaY", dszy);
			}

			XmlNodeRef textures = attachment->findChild("Textures");
			int textureChildCount = textures->getChildCount();
			for (int textureidx = 0; textureidx < textureChildCount; ++textureidx)
			{
				XmlNodeRef texture = textures->getChild(textureidx);
				const char * maptype = texture->getAttr("Map");
				if (maptype && strnicmp(maptype, slot, strlen(slot)) == 0)
				{
					XmlNodeRef textureEntry = m_CC.pRC->CreateXml("texture");
					textureEntry->setAttr("file", texture->getAttr("File"));
					textureEntry->setAttr("x", offsetx);
					textureEntry->setAttr("y", offsety);
					textureEntry->setAttr("dx", sx);
					textureEntry->setAttr("dy", sy);

					if (setupChild->haveAttr("dstSubAreaX"))
					{
						textureEntry->setAttr("sdx", dszx);
					}

					if (setupChild->haveAttr("dstSubAreaY"))
					{
						textureEntry->setAttr("sdy", dszy);
					}

					destination->addChild(textureEntry);
					return;
				}
			}
		}
	}

	if (!foundAttachment)
	{
		RCLogError("[chrmerge] Failed to find material name '%s' in the character merge setup file, please check the material slot naming conventions.", attachmentName);
	}
}

bool CharacterCompiler::CreateOptimisedMaterial(const char * filename, const std::vector<XmlNodeRef> & materials)
{
	string path(PathUtil::ReplaceExtension(filename, ".mtl"));

	XmlNodeRef newMtlFile = m_CC.pRC->CreateXml("Material");
	if (!newMtlFile)
		return false;

	newMtlFile->setAttr("MtlFlags", 524544);
	XmlNodeRef submats = m_CC.pRC->CreateXml("SubMaterials");
	if (!submats)
		return false;

	//go through each material in matrial list 
	for (std::vector<XmlNodeRef>::const_iterator item = materials.cbegin(), end = materials.cend(); item != end; ++item)
	{
		submats->addChild((*item));
	}

	//add the submat list to the material file
	newMtlFile->addChild(submats);

	//write the material file
	newMtlFile->saveToFile(path.c_str());
	return true;
}

int CharacterCompiler::FindSubsetId(CNodeCGF * pNode, int vertid)
{
	const int nSubsets = pNode->pMesh->m_subsets.size();
	for (int idx = 0; idx < nSubsets; ++idx)
	{
		const SMeshSubset & subset = pNode->pMesh->m_subsets[idx];
		if (vertid >= subset.nFirstVertId && vertid < subset.nFirstVertId + subset.nNumVerts)
		{
			return idx;
		}
	}
	return -1;
}

///~ CDF MERGING CODE ~///

