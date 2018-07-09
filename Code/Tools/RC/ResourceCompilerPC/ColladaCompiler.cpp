// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   ColladaCompiler.cpp
//  Version:     v1.00
//  Created:     3/4/2006 by Michael Smith
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColladaCompiler.h"
#include "ColladaLoader.h"
#include "CharacterCompiler.h"
#include <CrySystem/CryVersion.h>
#include "CGF/CGFSaver.h"
#include "CGF/ANMSaver.h"
#include "CGF/CAFSaver.h"
#include "CGF/CgfUtil.h"
#include "MathHelpers.h"
#include "StaticObjectCompiler.h"
#include "StatCGFPhysicalize.h"
#include "StringHelpers.h"
#include "CGF/CGFNodeMerger.h"
#include "IConfig.h"
#include "IAssetManager.h"
#include "UpToDateFileHelpers.h"
#include "Export/ExportHelpers.h"

#include <CrySystem/XML/IXml.h>
#include "../../CryXML/IXMLSerializer.h"
#include "../../CryXML/Xml/xml.h"

#include <iterator>

ColladaCompiler::ColladaCompiler(ICryXML* pCryXML, IPakSystem* pPakSystem)
:	pCryXML(pCryXML),
	pPakSystem(pPakSystem)
{
	this->pCryXML->AddRef();
	this->pPhysicsInterface = 0;
}

ColladaCompiler::~ColladaCompiler()
{
	delete this->pPhysicsInterface;
	this->pCryXML->Release();
}

class ColladaLoaderListener : public IColladaLoaderListener
{
public:
	virtual void OnColladaLoaderMessage(MessageType type, const char* szMessage)
	{
		switch (type)
		{
			case MESSAGE_INFO:
				RCLog("%s", szMessage);
				break;
			case MESSAGE_WARNING:
				RCLogWarning("%s", szMessage);
				break;
			case MESSAGE_ERROR:
				RCLogError("%s", szMessage);
				break;
		}
	}
};

void ColladaCompiler::SetExportInfo(CContentCGF* const pCompiledCGF, ExportFile &exportfile )
{
	if (!pCompiledCGF)
		return;

	CExportInfoCGF* info = pCompiledCGF->GetExportInfo();
	// Set export data.
	{
		const SFileVersion &version = m_CC.pRC->GetFileVersion();
		info->rc_version[0] = version.v[0];
		info->rc_version[1] = version.v[1];
		info->rc_version[2] = version.v[2];
		info->rc_version[3] = version.v[3];

		StringHelpers::SafeCopyPadZeros(
			info->rc_version_string,
			sizeof(info->rc_version_string),
			StringHelpers::Format(" RCVer:%d.%d ", version.v[2], version.v[1]).c_str());
	}

	info->bFromColladaXSI = info->bFromColladaMAX = info->bFromColladaMAYA = false;
	switch (exportfile.metaData.GetAuthorTool())
	{
	case TOOL_SOFTIMAGE_XSI:
		info->bFromColladaXSI = true;
		break;
	case TOOL_AUTODESK_MAX:
		info->bFromColladaMAX = true;
		break;
	case TOOL_MAYA:
		info->bFromColladaMAYA = true;
		break;
	default:
		break;
	}

	info->authorToolVersion = exportfile.metaData.GetAuthorToolVersion();
}

// -----------------------------------------------------------------------------------------------------------
// Compile collada file to Crytek Geometry File (CGF) --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
bool ColladaCompiler::CompileToCGF(
	ExportFile &exportFile,
	const string& exportFileName)
{
	// _EM_INVALID is used to avoid Floating Point Exception inside CryPhysics
	MathHelpers::AutoFloatingPointExceptions autoFpe(~(_EM_INEXACT | _EM_UNDERFLOW | _EM_INVALID));

	CContentCGF* const pCGF = exportFile.pCGF;
	if (!pCGF)
	{
		return false;
	}

	CInternalSkinningInfo* const pSkinninginfo =
		(exportFile.type == EXPORT_CGA)
		? exportFile.pCtrlSkinningInfo
		: 0;

	PrepareCGF(pCGF);

	// Flip texture coordinate V in all meshes
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const node = pCGF->GetNode(i);
		if (!node || !node->pMesh)
		{
			continue;
		}

		CMesh* const mesh = node->pMesh;
		if (!mesh->m_pTexCoord)
		{
			continue;
		}

		for (int v = 0, n = mesh->GetTexCoordCount(); v < n; ++v)
		{
			Vec2 uv = mesh->m_pTexCoord[v].GetUV();
			uv.y = 1.0f - uv.y;
			mesh->m_pTexCoord[v] = SMeshTexCoord(uv);
		}
	}

	// Make a compiler object to compile the nodes.
	if (!this->pPhysicsInterface)
	{
		this->pPhysicsInterface = new CPhysicsInterface;
	}
	if (!this->pPhysicsInterface->GetGeomManager())
	{
		return false;
	}
	CStaticObjectCompiler compiler(this->pPhysicsInterface, false);

	// Compile the cgf.
	CContentCGF* const pCompiledCGF = compiler.MakeCompiledCGF(pCGF, false, false);

	if (!pCompiledCGF)
	{
		return false;
	}
	
	// Set export data.
	SetExportInfo(pCompiledCGF, exportFile);

	// Save the processed data to a chunk file.
	CChunkFile chunkFile;
	CSaverCGF cgfSaver(chunkFile);

	cgfSaver.SetContent(pCompiledCGF);

	// Only store 
	const bool bNeedEndianSwap = false;
	const bool bStorePositionsAsF16 = false;
	const bool bUseQuaternions = m_CC.config->GetAsBool("qtangents", false, true);
	const bool bStoreIndicesAsU16 = false;

	cgfSaver.SaveExportFlags(bNeedEndianSwap);
	if (pSkinninginfo)
	{
		cgfSaver.SaveTiming(pSkinninginfo);
	}
	cgfSaver.SaveMaterials(bNeedEndianSwap);
	cgfSaver.SaveNodes(bNeedEndianSwap, bStorePositionsAsF16, bUseQuaternions, bStoreIndicesAsU16, pSkinninginfo );
	cgfSaver.SaveBreakablePhysics(bNeedEndianSwap);
	cgfSaver.SaveFoliage();

	// Following 'delete' was commented out because pCompiledCGF is a copy of a pointer from
	// the compiler.m_pLods[] and will be deleted when the destructor of 'compiler' is invoked.
	//delete pCompiledCGF;

	return CgfUtil::WriteTempRename(chunkFile, exportFileName);
}

// -----------------------------------------------------------------------------------------------------------
// Compile collada file to Crytek Character File (CHR) -------------------------------------------------------
// This is uncompiled format of CHR! Should run the resource compiler again for this chr file ----------------
// -----------------------------------------------------------------------------------------------------------
bool ColladaCompiler::CompileToCHR(ExportFile &exportFile, const string& exportFileName)
{
	CContentCGF* pCGF = exportFile.pCGF;
	if (!pCGF)
		return false;

	CSkinningInfo* pSkinningInfo = pCGF->GetSkinningInfo();

	if (pSkinningInfo->m_arrBonesDesc.size() == 0)
		return false;

	CChunkFile chunkFile;
	CSaverCGF cgfSaver(chunkFile);

	SetExportInfo(pCGF, exportFile );

	cgfSaver.SetContent(pCGF);
	
	const bool bSwapEndian = (m_CC.pRC->GetPlatformInfo(m_CC.platform)->bBigEndian != SYSTEM_IS_BIG_ENDIAN);

	// Only store 
	cgfSaver.SaveExportFlags(bSwapEndian);
	cgfSaver.SaveMaterials(bSwapEndian);
	cgfSaver.SaveUncompiledNodes();				// the nodes of character will be compile later
	cgfSaver.SaveUncompiledMorphTargets();
	
	const int numBones = pSkinningInfo->m_arrBonesDesc.size();
	if (numBones > 0)
	{
		Matrix33 rootRot(pSkinningInfo->m_arrBonesDesc[0].m_DefaultB2W);
		Quat q(rootRot);
		// Note: engine uses same comparison formula with epsilon 0.1f (~5.7 degrees),
		// but we use epsilon 0.09f (~5.2 degrees) here. It's because we want to
		// avoid rounding-off problems (export passes ok, but engine fails to
		// load the model).
		if (!Quat::IsEquivalent(q, Quat(IDENTITY), 0.09f))
		{
			const char* rootName = pSkinningInfo->m_arrBonesDesc[0].m_arrBoneName;
			char buf[128];
			cry_sprintf(buf,
				"\t% 3.2f, % 3.2f, % 3.2f\n"
				"\t% 3.2f, % 3.2f, % 3.2f\n"
				"\t% 3.2f, % 3.2f, % 3.2f",
				rootRot.m00, rootRot.m01, rootRot.m02,
				rootRot.m10, rootRot.m11, rootRot.m12,
				rootRot.m20, rootRot.m21, rootRot.m22);

			RCLogError("Skeleton-s root bone orientation is not identity (\"%s\"):\n"
				"%s\n"
				"The character will not be loaded by the engine: %s\n",
				rootName, buf, exportFile.name.c_str());
		}
	}

	// TODO : 1. Remove "% ..." substrings from m_arrBoneName. 2. Replace to CSkinSaver::SaveUncompiledSkinningInfo(cgfSaver, *pSkinningInfo, bSwapEndian, 100.0f);
	{
		//---------------------------------------------------------------------
		//---  write bone meshes to chunk
		//---------------------------------------------------------------------
		std::vector<int> boneMeshMap(pSkinningInfo->m_arrPhyBoneMeshes.size());
		for (int phys = 0, physCount = int(pSkinningInfo->m_arrPhyBoneMeshes.size()); phys < physCount; ++phys)
			boneMeshMap[phys] = cgfSaver.SaveBoneMesh(bSwapEndian, pSkinningInfo->m_arrPhyBoneMeshes[phys]);

		//---------------------------------------------------------------------
		//---  write bones to chunk
		//---------------------------------------------------------------------
		assert(pSkinningInfo->m_arrBoneEntities.size() == pSkinningInfo->m_arrBonesDesc.size());

		DynArray<BONE_ENTITY> tempBoneEntities = pSkinningInfo->m_arrBoneEntities;
		for (int i = 0, count = int(tempBoneEntities.size()); i < count; ++i)
		{
			tempBoneEntities[i].phys.nPhysGeom = (tempBoneEntities[i].phys.nPhysGeom >= 0 ? boneMeshMap[tempBoneEntities[i].phys.nPhysGeom] : -1);
			StringHelpers::SafeCopyPadZeros(tempBoneEntities[i].prop, sizeof(tempBoneEntities[i].prop), pSkinningInfo->m_arrBoneEntities[i].prop);
		}
		cgfSaver.SaveBones(bSwapEndian, &tempBoneEntities[0], numBones, numBones * sizeof(BONE_ENTITY));

		//---------------------------------------------------------------------
		//---  write bone names to chunk
		//---------------------------------------------------------------------
		{
			std::vector<char> boneNames;
			boneNames.reserve(32 * numBones);

			std::vector<SBoneInitPosMatrix> boneMatrices(numBones);
			for (int BoneID = 0; BoneID < numBones; BoneID++)
			{
				const char* boneName = pSkinningInfo->m_arrBonesDesc[BoneID].m_arrBoneName;
				size_t len = strlen(boneName);
				const char* pos = strstr(boneName, "%");
				if (pos != 0)
				{
					len = pos - boneName;
				}
				boneNames.insert(boneNames.end(), boneName, boneName + len);
				boneNames.push_back('\0');

				// ----------------------------
				// get bone init matrix -------
				for (int x = 0; x < 4; x++)
				{
					//TODO: is this OK? (get data from m_DefaultB2W) (and the last rows of matrices, contains 100x scale? - this maybe causes a problem)
					Matrix34 m = Matrix34::CreateScale(Vec3(100, 100, 100)) * pSkinningInfo->m_arrBonesDesc[BoneID].m_DefaultB2W;
					Vec3 column = m.GetColumn(x);
					for (int y = 0; y < 3; y++)
						boneMatrices[BoneID][x][y] = column[y];
				}
				// ----------------------------
			}
			boneNames.push_back('\0');

			//---------------------------------------------------------------------
			//---  write bone names and initial matrices to chunk (in BoneID order)
			//---------------------------------------------------------------------
			cgfSaver.SaveBoneNames(bSwapEndian, &boneNames[0], numBones, boneNames.size());
			cgfSaver.SaveBoneInitialMatrices(bSwapEndian, &boneMatrices[0], numBones, sizeof(SBoneInitPosMatrix)*numBones);
		}
	}

	return CgfUtil::WriteTempRename(chunkFile, exportFileName);
}

// -----------------------------------------------------------------------------------------------------------
// Compile collada file to Crytek Animation File (ANM) -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
bool ColladaCompiler::CompileToANM(ExportFile &exportFile, const string& exportFileName)
{
	CContentCGF* pCGF = exportFile.pCGF;
	CInternalSkinningInfo* pSkinninginfo = exportFile.pCtrlSkinningInfo;
	if (!pCGF || !pSkinninginfo)
		return false;

	// Set export data.
	SetExportInfo(pCGF, exportFile );

	// Save the processed data to a chunkfile.
	CChunkFile chunkFile;	//TODO: set FileType, Version (if needs)
	CSaverANM anmSaver(exportFileName, chunkFile);

	anmSaver.Save(pCGF,pSkinninginfo);

	return CgfUtil::WriteTempRename(chunkFile, exportFileName);
}

// -----------------------------------------------------------------------------------------------------------
// Compile collada file to Crytek Bone Animation File (CAF) --------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
bool ColladaCompiler::CompileToCAF(ExportFile &exportFile, const string& exportFileName)
{
	CContentCGF* pCGF = exportFile.pCGF;
	CInternalSkinningInfo* pSkinninginfo = exportFile.pCtrlSkinningInfo;
	if (!pCGF || !pSkinninginfo)
		return false;

	// Set export data.
	SetExportInfo(pCGF, exportFile );

	// Save the processed data to a chunkfile.
	CChunkFile chunkFile;	//TODO: set FileType, Version (if needs)
	CSaverCAF cafSaver(exportFileName, chunkFile);

	cafSaver.Save(pCGF,pSkinninginfo);

	return CgfUtil::WriteTempRename(chunkFile, exportFileName);
}

void ColladaCompiler::WriteMaterialLibrary(sMaterialLibrary& materialLibrary, const string& matFileName)
{
	IXMLSerializer* pSerializer = pCryXML->GetXMLSerializer();

	XmlNodeRef rootNode = pSerializer->CreateNode("Material");
	XmlNodeRef submatNode = pSerializer->CreateNode("SubMaterials");

	rootNode->setAttr("MtlFlags", materialLibrary.flag);
	rootNode->addChild(submatNode);

	for (int i=0;i<materialLibrary.materials.size();i++)
	{
		sMaterial& material = materialLibrary.materials[i];

		Vec3 vecDiffuse(material.diffuse.r,material.diffuse.g,material.diffuse.b);
		Vec3 vecSpecular(material.specular.r,material.specular.g,material.specular.b);
		Vec3 vecEmissive(material.emissive.r,material.emissive.g,material.emissive.b);

		XmlNodeRef matNode = pSerializer->CreateNode("Material");	

		matNode->setAttr("Name", material.name.c_str());
		matNode->setAttr("MtlFlags", material.flag);
		//matNode->setAttr("Shader", material.shader.c_str());		//TODO: select proper shader
		matNode->setAttr("SurfaceType", material.surfacetype.c_str());
		matNode->setAttr("Diffuse", vecDiffuse);
		matNode->setAttr("Specular", vecSpecular);
		matNode->setAttr("Emissive", vecEmissive);
		matNode->setAttr("Shininess", material.shininess);
		matNode->setAttr("Opacity", material.opacity);

		XmlNodeRef texturesNode = pSerializer->CreateNode("Textures");
		matNode->addChild(texturesNode);

		for (int t=0;t<material.textures.size();t++)
		{
			sTexture& texture = material.textures[t];
			
			XmlNodeRef textureNode = pSerializer->CreateNode("Texture");
			textureNode->setAttr("Map", texture.type.c_str());
			textureNode->setAttr("File", texture.filename.c_str());
			
			texturesNode->addChild(textureNode);
		}
		
		submatNode->addChild(matNode);
	}

	pSerializer->Write(rootNode, matFileName.c_str());
}

bool ColladaCompiler::Process()
{
	string const outputFolder = m_CC.GetOutputFolder();

	ColladaLoaderListener listener;	
	std::vector<ExportFile> exportFileList;
	std::vector<sMaterialLibrary> materialLibraryList;
	IPakSystem* pPakSystem = (m_CC.pRC ? m_CC.pRC->GetPakSystem() : 0);
	if (!LoadColladaFile(exportFileList, materialLibraryList, m_CC.GetSourcePath(), this->pCryXML, pPakSystem, &listener))
	{
		return false;
	}

	// Write material libraries ------------------------------------------------------
	if (m_CC.config->GetAsBool("createmtl", false, true))
	{
		for (int i=0;i<materialLibraryList.size();i++)
		{
			sMaterialLibrary& materialLibrary = materialLibraryList[i];
			string const matFileName = PathUtil::Make(outputFolder, materialLibrary.library.c_str()) + ".mtl";
			WriteMaterialLibrary(materialLibrary, matFileName);
		}
	}
	
	// Export model and animation track files (CGF,CGA,CHR,ANM,CAF) ------------------
	bool success = true;
	for (int i=0; i<exportFileList.size(); i++)
	{
		ExportFile& exportfile = exportFileList[i];

		uint32 numNodes = exportfile.pCGF->GetNodeCount();
		for (int j=0; j<numNodes; j++)
		{
			CNodeCGF *pNode = exportfile.pCGF->GetNode(j);
			string newNodeName;
			size_t pos = string(pNode->name).find('%');
			if (pos == string::npos || pos > 0)
			{
				newNodeName = string(pNode->name).substr(0,pos);
				cry_strcpy(pNode->name, newNodeName);
			}
		}

		string exportFolder = outputFolder;
		if (!exportfile.customExportPath.empty())
		{
			exportFolder = exportfile.customExportPath;
			if (PathHelpers::IsRelative(exportfile.customExportPath))
			{
				exportFolder = PathUtil::Make(outputFolder,exportFolder);
			}
		}
		const string exportFileName = PathUtil::Make(exportFolder,exportfile.name);

		switch (exportfile.type)
		{
			// Export to CGF
			case EXPORT_CGF:
				{
					const string exportFileNameExt = PathUtil::ReplaceExtension(exportFileName,"cgf");
					if (!CompileToCGF(exportfile, exportFileNameExt)
						|| !m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { exportFileNameExt }))
					{
						success = false;
					}
					m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), exportFileNameExt);
				}
				break;
			
			// Export to CGA
			case EXPORT_CGA:
				{
					const string exportFileNameExt = PathUtil::ReplaceExtension(exportFileName,"cga");
					if (!CompileToCGF(exportfile,exportFileNameExt)
						|| !m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { exportFileNameExt }))
					{
						success = false;
					}
					m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), exportFileNameExt);
				}
				break;

			// Export to CHR
			case EXPORT_CHR:
			case EXPORT_SKIN:
				{
					const string exportFileNameExt = PathUtil::ReplaceExtension(exportFileName,exportfile.type == EXPORT_SKIN ? "skin" : "chr");
					if (!CompileToCHR(exportfile,exportFileNameExt)
						|| !m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { exportFileNameExt }))
					{
						success = false;
					}
					m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), exportFileNameExt);
				}
				break;
			
			// Export to ANM
			case EXPORT_ANM:
				{
					const string exportFileNameExt = PathUtil::ReplaceExtension(exportFileName,"anm");
					if (!CompileToANM(exportfile,exportFileNameExt)
						|| !m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { exportFileNameExt }))
					{
						success = false;
					}
					m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), exportFileNameExt);
				}
				break;

			// Export to CAF
			case EXPORT_CAF:
				{
					const string exportFileNameExt = PathUtil::ReplaceExtension(exportFileName,"caf");
					if (!CompileToCAF(exportfile,exportFileNameExt)
						|| !m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { exportFileNameExt }))
					{
						success = false;
					}
					m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), exportFileNameExt);
				}
				break;
			
			case EXPORT_INTERMEDIATE_CAF:
				{
					const string exportFileNameExt = PathUtil::ReplaceExtension(exportFileName,"i_caf");
					if (!CompileToCAF(exportfile,exportFileNameExt)
						|| !m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, m_CC.GetSourcePath(), { exportFileNameExt }))
					{
						success = false;
					}
					m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), exportFileNameExt);
				}
				break;
		}
	}

	return success;
}

string ColladaCompiler::GetOutputFileNameOnly() const
{
	const string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
	return sourceFileFinal;
}

const char* ColladaCompiler::GetExt(int index) const
{
	switch (index)
	{
	case 0:
		return "dae";
	case 1:
		return "dae.zip";
	default:
		return 0;
	}
}

void GenerateDefaultUVs(CMesh& mesh);

void ColladaCompiler::PrepareCGF(CContentCGF* pCGF)
{
	// Loop through all the meshes.
	for (int i = 0; i < pCGF->GetNodeCount(); i++) 
	{
		CNodeCGF* const pNodeCGF = pCGF->GetNode(i);
		CMesh* const pMesh = pNodeCGF->pMesh;
		if (pMesh)
		{
			// If the mesh has no UVs, generate them here.
			if (pMesh->GetTexCoordCount() == 0)
			{
				GenerateDefaultUVs(*pMesh);
			}
		}
	}
}

void ColladaCompiler::FindUsedMaterialIDs(std::vector<int>& usedMaterialIDs, CContentCGF* pCGF)
{
	std::set<int> materialIDSet;
	for (int nodeIndex = 0; nodeIndex < pCGF->GetNodeCount(); ++nodeIndex)
	{
		CNodeCGF* node = pCGF->GetNode(nodeIndex);
		CMesh* mesh = 0;
		if (node && node->pMesh && !node->bPhysicsProxy && node->type == CNodeCGF::NODE_MESH)
			mesh = node->pMesh;
		if (mesh)
		{
			for (int subsetIndex = 0; subsetIndex < mesh->m_subsets.size(); ++subsetIndex)
			{
				SMeshSubset& subset = mesh->m_subsets[subsetIndex];
				materialIDSet.insert(subset.nMatID);
			}
		}
	}

	usedMaterialIDs.clear();
	std::copy(materialIDSet.begin(), materialIDSet.end(), back_inserter(usedMaterialIDs));
}

static void GenerateDefaultUVs(CMesh& mesh)
{
	const int numVerts = mesh.GetVertexCount();
	mesh.ReallocStream(CMesh::TEXCOORDS, numVerts);

	// make up the cubic coordinates
	for (int nVert = 0; nVert < numVerts; ++nVert)
	{
		const Vec3& v = mesh.m_pPositions[nVert];
		Vec2 coord;

		ExportHelpers::GenerateTextureCoordinates(&coord.x, &coord.y, v.x, v.y, v.z);
		mesh.m_pTexCoord[nVert] = SMeshTexCoord(coord);
	}
}
