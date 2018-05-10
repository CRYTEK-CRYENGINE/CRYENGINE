// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// FBX SDK Help: http://download.autodesk.com/global/docs/fbxsdk2012/en_us/index.html

#include "FBXExporter.h"
#include "FBXSettingsDlg.h"

namespace {

std::string GetFilePath(const std::string& filename)
{
	int pos = filename.find_last_of("/\\");
	if (pos > 0)
		return filename.substr(0, pos + 1);

	return filename;
}

std::string GetFileName(const std::string& fullFilename)
{
	int pos = fullFilename.find_last_of("/\\");
	if (pos > 0)
		return fullFilename.substr(pos + 1);

	return fullFilename;
}

} // namespace

// CFBXExporter
CFBXExporter::CFBXExporter() :
	m_pFBXManager(0), m_pFBXScene(0)
{
	m_settings.bCopyTextures = true;
	m_settings.bEmbedded = false;
	m_settings.bAsciiFormat = false;
	m_settings.axis = eAxis_Z;
}

const char* CFBXExporter::GetExtension() const
{
	return "fbx";
}

const char* CFBXExporter::GetShortDescription() const
{
	return "FBX format";
}

bool CFBXExporter::ExportToFile(const char* filename, const SExportData* pData)
{
	if (!m_pFBXManager)
		m_pFBXManager = FbxManager::Create();

	bool bAnimationExport = false;

	for (int objectID = 0; objectID < pData->GetObjectCount(); ++objectID)
	{
		SExportObject* pObject = pData->GetExportObject(objectID);

		if (pObject->GetEntityAnimationDataCount() > 0)
		{
			bAnimationExport = true;
			break;
		}
	}

	if (!bAnimationExport && !OpenFBXSettingsDlg(m_settings))
	{
		return false;
	}

	m_path = GetFilePath(filename);
	std::string name = GetFileName(filename);

	// create an IOSettings object
	FbxIOSettings* pSettings = FbxIOSettings::Create(m_pFBXManager, IOSROOT);
	m_pFBXManager->SetIOSettings(pSettings);

	// Create a scene object
	FbxScene* pFBXScene = FbxScene::Create(m_pFBXManager, "Test");
	if (m_settings.axis == eAxis_Z)
	{
		pFBXScene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::Max);
		pFBXScene->GetGlobalSettings().SetOriginalUpAxis(FbxAxisSystem::Max);
	}
	else
	{
		pFBXScene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::MayaYUp);
		pFBXScene->GetGlobalSettings().SetOriginalUpAxis(FbxAxisSystem::MayaYUp);
	}

	// Create document info
	FbxDocumentInfo* pDocInfo = FbxDocumentInfo::Create(m_pFBXManager, "DocInfo");
	pDocInfo->mTitle = name.c_str();
	pDocInfo->mSubject = "Exported geometry from Crytek Sandbox application.";
	pDocInfo->mAuthor = "Crytek Sandbox FBX exporter.";
	pDocInfo->mRevision = "rev. 2.0";
	pDocInfo->mKeywords = "Crytek Sandbox FBX export";
	pDocInfo->mComment = "";
	pFBXScene->SetDocumentInfo(pDocInfo);

	// Create nodes from objects and add them to the scene
	FbxNode* pRootNode = pFBXScene->GetRootNode();

	FbxAnimStack* pAnimStack = FbxAnimStack::Create(pFBXScene, "AnimStack");
	FbxAnimLayer* pAnimBaseLayer = FbxAnimLayer::Create(pFBXScene, "AnimBaseLayer");
	pAnimStack->AddMember(pAnimBaseLayer);

	int numObjects = pData->GetObjectCount();

	m_nodes.resize(0);
	m_nodes.reserve(numObjects);

	for (int i = 0; i < numObjects; ++i)
	{
		SExportObject* pObj = pData->GetExportObject(i);
		FbxNode* newNode = NULL;

		if (!bAnimationExport)
		{
			newNode = CreateFBXNode(pObj);
		}
		else
		{
			newNode = CreateFBXAnimNode(pFBXScene, pAnimBaseLayer, pObj);
		}

		m_nodes.push_back(newNode);
	}

	// solve hierarchy
	for (int i = 0; i < m_nodes.size() && i < numObjects; ++i)
	{
		const SExportObject* pObj = pData->GetExportObject(i);
		FbxNode* pNode = m_nodes[i];
		FbxNode* pParentNode = 0;
		if (pObj->nParent >= 0 && pObj->nParent < m_nodes.size())
		{
			pParentNode = m_nodes[pObj->nParent];
		}
		if (pParentNode)
			pParentNode->AddChild(pNode);
		else
			pRootNode->AddChild(pNode);
	}

	int nFileFormat = -1;

	if (m_settings.bAsciiFormat)
	{
		if (nFileFormat < 0 || nFileFormat >= m_pFBXManager->GetIOPluginRegistry()->GetWriterFormatCount())
		{
			nFileFormat = m_pFBXManager->GetIOPluginRegistry()->GetNativeWriterFormat();
			int lFormatIndex, lFormatCount = m_pFBXManager->GetIOPluginRegistry()->GetWriterFormatCount();
			for (lFormatIndex = 0; lFormatIndex < lFormatCount; lFormatIndex++)
			{
				if (m_pFBXManager->GetIOPluginRegistry()->WriterIsFBX(lFormatIndex))
				{
					FbxString lDesc = m_pFBXManager->GetIOPluginRegistry()->GetWriterFormatDescription(lFormatIndex);
					char* lASCII = "ascii";
					if (lDesc.Find(lASCII) >= 0)
					{
						nFileFormat = lFormatIndex;
						break;
					}
				}
			}
		}
	}

	pSettings->SetBoolProp(EXP_FBX_EMBEDDED, m_settings.bEmbedded);

	//Save a scene
	bool bRet = false;
	FbxExporter* pFBXExporter = FbxExporter::Create(m_pFBXManager, name.c_str());
	if (pFBXExporter->Initialize(filename, nFileFormat, pSettings))
		bRet = pFBXExporter->Export(pFBXScene);

	m_materials.clear();
	m_meshMaterialIndices.clear();

	pFBXExporter->Destroy();

	pFBXScene->Destroy();

	m_pFBXManager->Destroy();
	m_pFBXManager = 0;

	if (bRet)
		GetIEditor()->GetSystem()->GetILog()->Log("\nFBX Exporter: Exported successfully to %s.", name.c_str());
	else
		GetIEditor()->GetSystem()->GetILog()->LogError("\nFBX Exporter: Failed.");

	return bRet;
}

FbxMesh* CFBXExporter::CreateFBXMesh(const SExportObject* pObj)
{
	if (!m_pFBXManager)
		return 0;

	int numVertices = pObj->GetVertexCount();
	const Export::Vector3D* pVerts = pObj->GetVertexBuffer();

	int numMeshes = pObj->GetMeshCount();

	int numAllFaces = 0;
	for (int j = 0; j < numMeshes; ++j)
	{
		const SExportMesh* pMesh = pObj->GetMesh(j);
		int numFaces = pMesh->GetFaceCount();
		numAllFaces += numFaces;
	}

	if (numVertices == 0 || numAllFaces == 0)
		return 0;

	FbxMesh* pFBXMesh = FbxMesh::Create(m_pFBXManager, pObj->name);

	pFBXMesh->InitControlPoints(numVertices);
	FbxVector4* pControlPoints = pFBXMesh->GetControlPoints();

	for (int i = 0; i < numVertices; ++i)
	{
		const Export::Vector3D& vertex = pVerts[i];
		if (m_settings.axis == eAxis_Z)
			pControlPoints[i] = FbxVector4(vertex.x, vertex.y, vertex.z);
		else
			pControlPoints[i] = FbxVector4(vertex.x, vertex.z, -vertex.y);
	}

	int numNormals = pObj->GetNormalCount();
	const Export::Vector3D* pNormals = pObj->GetNormalBuffer();
	if (numNormals)
	{
		FbxGeometryElementNormal* pElementNormal = pFBXMesh->CreateElementNormal();
		FBX_ASSERT(pElementNormal != NULL);

		pElementNormal->SetMappingMode(FbxGeometryElement::eByControlPoint);
		pElementNormal->SetReferenceMode(FbxGeometryElement::eDirect);

		for (int i = 0; i < numNormals; ++i)
		{
			const Export::Vector3D& normal = pNormals[i];
			pElementNormal->GetDirectArray().Add(FbxVector4(normal.x, normal.y, normal.z));
		}
	}

	int numTexCoords = pObj->GetTexCoordCount();
	const Export::UV* pTextCoords = pObj->GetTexCoordBuffer();
	if (numTexCoords)
	{
		// Create UV for Diffuse channel.
		FbxGeometryElementUV* pFBXDiffuseUV = pFBXMesh->CreateElementUV("DiffuseUV");
		FBX_ASSERT(pFBXDiffuseUV != NULL);

		pFBXDiffuseUV->SetMappingMode(FbxGeometryElement::eByControlPoint);
		pFBXDiffuseUV->SetReferenceMode(FbxGeometryElement::eDirect);

		for (int i = 0; i < numTexCoords; ++i)
		{
			const Export::UV& textCoord = pTextCoords[i];
			pFBXDiffuseUV->GetDirectArray().Add(FbxVector2(textCoord.u, textCoord.v));
		}
	}

	int numColors = pObj->GetColorCount();
	const Export::Color* pColors = pObj->GetColorBuffer();
	if (numColors)
	{
		FbxGeometryElementVertexColor* pElementVertexColor = pFBXMesh->CreateElementVertexColor();
		pElementVertexColor->SetMappingMode(FbxGeometryElement::eByControlPoint);
		pElementVertexColor->SetReferenceMode(FbxGeometryElement::eDirect);
		pElementVertexColor->GetDirectArray().SetCount(numColors);

		for (int i = 0; i < numColors; ++i)
			pElementVertexColor->GetDirectArray().SetAt(i, FbxColor(pColors[i].r, pColors[i].g, pColors[i].b, pColors[i].a));
	}

	pFBXMesh->ReservePolygonCount(numAllFaces);
	pFBXMesh->ReservePolygonVertexCount(numAllFaces * 3);

	// Set material mapping.
	FbxGeometryElementMaterial* pMaterialElement = pFBXMesh->CreateElementMaterial();
	pMaterialElement->SetMappingMode(FbxGeometryElement::eByPolygon);
	pMaterialElement->SetReferenceMode(FbxGeometryElement::eIndexToDirect);

	for (int j = 0; j < numMeshes; ++j)
	{
		const SExportMesh* pMesh = pObj->GetMesh(j);

		// Write all faces, convert the indices to one based indices
		int numFaces = pMesh->GetFaceCount();

		int polygonCount = pFBXMesh->GetPolygonCount();
		pMaterialElement->GetIndexArray().SetCount(polygonCount + numFaces);
		int materialIndex = m_meshMaterialIndices[pMesh];

		const Export::Face* pFaces = pMesh->GetFaceBuffer();
		for (int i = 0; i < numFaces; ++i)
		{
			const Export::Face& face = pFaces[i];
			pFBXMesh->BeginPolygon(-1, -1, -1, false);

			pFBXMesh->AddPolygon(face.idx[0], face.idx[0]);
			pFBXMesh->AddPolygon(face.idx[1], face.idx[1]);
			pFBXMesh->AddPolygon(face.idx[2], face.idx[2]);

			//pFBXDiffuseUV->GetIndexArray().SetAt(face.idx[0], face.idx[0]);

			pFBXMesh->EndPolygon();
			//pPolygonGroupElement->GetIndexArray().Add(j);

			pMaterialElement->GetIndexArray().SetAt(polygonCount + i, materialIndex);
		}
	}

	return pFBXMesh;
}

FbxFileTexture* CFBXExporter::CreateFBXTexture(const char* pTypeName, const char* pName)
{
	std::string filename = pName;

	if (m_settings.bCopyTextures)
	{
		// check if file exists
		DWORD dwAttrib = GetFileAttributes(pName);
		if (dwAttrib == INVALID_FILE_ATTRIBUTES || dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		{
			GetIEditor()->GetSystem()->GetILog()->LogError("\nFBX Exporter: Texture %s is not on the disk.", pName);
		}
		else
		{
			filename = m_path;
			filename += GetFileName(pName);
			if (TRUE == CopyFile(pName, filename.c_str(), FALSE))
			{
				GetIEditor()->GetSystem()->GetILog()->Log("\nFBX Exporter: Texture %s was copied.", pName);
			}
			filename = GetFileName(pName); // It works for Maya, but not good for MAX
		}
	}

	FbxFileTexture* pFBXTexture = FbxFileTexture::Create(m_pFBXManager, pTypeName);
	pFBXTexture->SetFileName(filename.c_str());
	pFBXTexture->SetTextureUse(FbxTexture::eStandard);
	pFBXTexture->SetMappingType(FbxTexture::eUV);
	pFBXTexture->SetMaterialUse(FbxFileTexture::eModelMaterial);
	pFBXTexture->SetSwapUV(false);
	pFBXTexture->SetTranslation(0.0, 0.0);
	pFBXTexture->SetScale(1.0, 1.0);
	pFBXTexture->SetRotation(0.0, 0.0);

	return pFBXTexture;
}

FbxSurfaceMaterial* CFBXExporter::CreateFBXMaterial(const std::string& name, const SExportMesh* pMesh)
{
	auto it = m_materials.find(name);
	if (it != m_materials.end())
		return it->second;

	FbxSurfacePhong* pMaterial = FbxSurfacePhong::Create(m_pFBXManager, name.c_str());

	pMaterial->Diffuse.Set(FbxDouble3(pMesh->material.diffuse.r, pMesh->material.diffuse.g, pMesh->material.diffuse.b));
	pMaterial->DiffuseFactor.Set(1.0);
	pMaterial->Specular.Set(FbxDouble3(pMesh->material.specular.r, pMesh->material.specular.g, pMesh->material.specular.b));
	pMaterial->SpecularFactor.Set(1.0);

	// Opacity
	pMaterial->TransparentColor.Set(FbxDouble3(1.0f, 1.0f, 1.0f)); // need explicitly setup
	pMaterial->TransparencyFactor.Set(1.0f - pMesh->material.opacity);

	// Glossiness
	//pMaterial->Shininess.Set(pow(2, pMesh->material.shininess * 10));

	if (pMesh->material.mapDiffuse[0] != '\0')
	{
		FbxFileTexture* pFBXTexture = CreateFBXTexture("Diffuse", pMesh->material.mapDiffuse);
		pMaterial->Diffuse.ConnectSrcObject(pFBXTexture);
	}

	if (pMesh->material.mapSpecular[0] != '\0')
	{
		FbxFileTexture* pFBXTexture = CreateFBXTexture("Specular", pMesh->material.mapSpecular);
		pMaterial->Specular.ConnectSrcObject(pFBXTexture);
	}

	if (pMesh->material.mapOpacity[0] != '\0')
	{
		FbxFileTexture* pFBXTexture = CreateFBXTexture("Opacity", pMesh->material.mapOpacity);
		pMaterial->TransparentColor.ConnectSrcObject(pFBXTexture);
	}

	/*
	   if (pMesh->material.mapBump[0] != '\0')
	   {
	   FbxFileTexture* pFBXTexture = CreateFBXTexture("Bump", pMesh->material.mapBump);
	   pMaterial->Bump.ConnectSrcObject(pFBXTexture);
	   }
	 */

	if (pMesh->material.mapDisplacement[0] != '\0')
	{
		FbxFileTexture* pFBXTexture = CreateFBXTexture("Displacement", pMesh->material.mapDisplacement);
		pMaterial->DisplacementColor.ConnectSrcObject(pFBXTexture);
	}

	m_materials[name] = pMaterial;

	return pMaterial;
}

FbxNode* CFBXExporter::CreateFBXAnimNode(FbxScene* pScene, FbxAnimLayer* pAnimBaseLayer, const SExportObject* pObj)
{
	FbxNode* pAnimNode = FbxNode::Create(pScene, pObj->name);
	pAnimNode->LclTranslation.GetCurveNode(pAnimBaseLayer, true);
	pAnimNode->LclRotation.GetCurveNode(pAnimBaseLayer, true);
	pAnimNode->LclScaling.GetCurveNode(pAnimBaseLayer, true);

	FbxCamera* pCamera = FbxCamera::Create(pScene, pObj->name);

	if (pObj->entityType == Export::eCamera)
	{
		pCamera->SetApertureMode(FbxCamera::eHorizontal);
		pCamera->SetFormat(FbxCamera::eCustomFormat);
		pCamera->SetAspect(FbxCamera::eFixedRatio, 1.777778f, 1.0);
		pAnimNode->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(-90.0f, 0.0f, 90.0f));
		pAnimNode->SetNodeAttribute(pCamera);

		if (pObj->cameraTargetNodeName != "")
		{
			for (int i = 0; i < m_nodes.size(); ++i)
			{
				FbxNode* pCameraTargetNode = m_nodes[i];
				const char* nodeName = pCameraTargetNode->GetName();
				if (strcmp(nodeName, pObj->cameraTargetNodeName) == 0)
				{
					FbxMarker* pMarker = FbxMarker::Create(pScene, "");
					pCameraTargetNode->SetNodeAttribute(pMarker);
					pAnimNode->SetTarget(pCameraTargetNode);
				}
			}
		}
	}

	if (pObj->GetEntityAnimationDataCount() > 0)
	{
		int animDataCount = pObj->GetEntityAnimationDataCount();

		for (int animDataIndex = 0; animDataIndex < animDataCount; ++animDataIndex)
		{
			const Export::EntityAnimData* pAnimData = pObj->GetEntityAnimationData(animDataIndex);

			FbxTime time = 0;
			time.SetSecondDouble(pAnimData->keyTime);

			FbxAnimCurve* pCurve = NULL;

			switch (pAnimData->dataType)
			{
			case eAnimParamType_PositionX:
				pCurve = pAnimNode->LclTranslation.GetCurve(pAnimBaseLayer, "X", true);
				break;
			case eAnimParamType_PositionY:
				pCurve = pAnimNode->LclTranslation.GetCurve(pAnimBaseLayer, "Y", true);
				break;
			case eAnimParamType_PositionZ:
				pCurve = pAnimNode->LclTranslation.GetCurve(pAnimBaseLayer, "Z", true);
				break;
			case eAnimParamType_RotationX:
				pCurve = pAnimNode->LclRotation.GetCurve(pAnimBaseLayer, "X", true);
				break;
			case eAnimParamType_RotationY:
				pCurve = pAnimNode->LclRotation.GetCurve(pAnimBaseLayer, "Y", true);
				break;
			case eAnimParamType_RotationZ:
				pCurve = pAnimNode->LclRotation.GetCurve(pAnimBaseLayer, "Z", true);
				break;
			case eAnimParamType_FOV:
				{
					pCurve = pCamera->FieldOfView.GetCurve(pAnimBaseLayer, "FieldOfView", true);
				}
				break;

			default:
				break;
			}

			if (pCurve)
			{
				int keyIndex = 0;

				pCurve->KeyModifyBegin();
				keyIndex = pCurve->KeyInsert(time);
				pCurve->KeySet(keyIndex, time, pAnimData->keyValue, FbxAnimCurveDef::eInterpolationCubic, FbxAnimCurveDef::eTangentBreak, pAnimData->rightTangent, pAnimData->leftTangent, FbxAnimCurveDef::eWeightedAll, pAnimData->rightTangentWeight, pAnimData->leftTangentWeight);

				pCurve->KeySetLeftDerivative(keyIndex, pAnimData->leftTangent);
				pCurve->KeySetRightDerivative(keyIndex, pAnimData->rightTangent);

				pCurve->KeySetLeftTangentWeight(keyIndex, pAnimData->leftTangentWeight);
				pCurve->KeySetRightTangentWeight(keyIndex, pAnimData->rightTangentWeight);

				FbxAnimCurveTangentInfo keyLeftInfo;
				keyLeftInfo.mAuto = 0;
				keyLeftInfo.mDerivative = pAnimData->leftTangent;
				keyLeftInfo.mWeight = pAnimData->leftTangentWeight;
				keyLeftInfo.mWeighted = true;
				keyLeftInfo.mVelocity = 0.0f;
				keyLeftInfo.mHasVelocity = false;

				FbxAnimCurveTangentInfo keyRightInfo;
				keyRightInfo.mAuto = 0;
				keyRightInfo.mDerivative = pAnimData->rightTangent;
				keyRightInfo.mWeight = pAnimData->rightTangentWeight;
				keyRightInfo.mWeighted = true;
				keyRightInfo.mVelocity = 0.0f;
				keyRightInfo.mHasVelocity = false;

				pCurve->KeySetLeftDerivativeInfo(keyIndex, keyLeftInfo);
				pCurve->KeySetRightDerivativeInfo(keyIndex, keyRightInfo);

				pCurve->KeyModifyEnd();
			}
		}
	}

	return pAnimNode;
}

FbxNode* CFBXExporter::CreateFBXNode(const SExportObject* pObj)
{
	if (!m_pFBXManager)
		return 0;

	// create a FbxNode
	FbxNode* pNode = FbxNode::Create(m_pFBXManager, pObj->name);
	if (m_settings.axis == eAxis_Z)
		pNode->LclTranslation.Set(FbxVector4(pObj->pos.x, pObj->pos.y, pObj->pos.z));
	else
		pNode->LclTranslation.Set(FbxVector4(pObj->pos.x, pObj->pos.z, -pObj->pos.y));

	// Rotation: Create Euler angels from quaternion throughout a matrix
	FbxAMatrix rotMat;
	if (m_settings.axis == eAxis_Z)
		rotMat.SetQ(FbxQuaternion(pObj->rot.v.x, pObj->rot.v.y, pObj->rot.v.z, pObj->rot.w));
	else
		rotMat.SetQ(FbxQuaternion(pObj->rot.v.x, pObj->rot.v.z, -pObj->rot.v.y, pObj->rot.w));

	pNode->LclRotation.Set(rotMat.GetR());

	if (m_settings.axis == eAxis_Z)
		pNode->LclScaling.Set(FbxVector4(pObj->scale.x, pObj->scale.y, pObj->scale.z));
	else
		pNode->LclScaling.Set(FbxVector4(pObj->scale.x, pObj->scale.z, -pObj->scale.y));

	// collect materials
	int materialIndex = 0;
	if (pObj->GetMeshCount() != 0 && pObj->materialName[0] != '\0')
	{
		for (int i = 0; i < pObj->GetMeshCount(); ++i)
		{
			const SExportMesh* pMesh = pObj->GetMesh(i);

			if (pMesh->material.name == '\0')
				continue;

			// find if material was created for this object
			int findIndex = -1;
			for (int j = 0; j < i; ++j)
			{
				const SExportMesh* pTestMesh = pObj->GetMesh(j);
				if (!strcmp(pTestMesh->material.name, pMesh->material.name))
				{
					findIndex = m_meshMaterialIndices[pTestMesh];
					break;
				}
			}

			if (findIndex != -1)
			{
				m_meshMaterialIndices[pMesh] = findIndex;
				continue;
			}

			std::string materialName = pObj->materialName;
			if (strcmp(pObj->materialName, pMesh->material.name))
			{
				materialName += ':';
				materialName += pMesh->material.name;
			}

			FbxSurfaceMaterial* pFBXMaterial = CreateFBXMaterial(materialName, pMesh);
			if (pFBXMaterial)
			{
				pNode->AddMaterial(pFBXMaterial);
				m_meshMaterialIndices[pMesh] = materialIndex++;
			}
		}
	}

	FbxMesh* pFBXMesh = CreateFBXMesh(pObj);
	if (pFBXMesh)
	{
		pNode->SetNodeAttribute(pFBXMesh);
		pNode->SetShadingMode(FbxNode::eTextureShading);
	}

	return pNode;
}

void CFBXExporter::Release()
{
	delete this;
}

inline f32 Maya2SandboxFOVDeg(const f32 fov, const f32 ratio) { return RAD2DEG(2.0f * atanf(tan(DEG2RAD(fov) / 2.0f) / ratio)); };

void       CFBXExporter::FillAnimationData(SExportObject* pObject, FbxAnimLayer* pAnimLayer, FbxAnimCurve* pCurve, EAnimParamType paramType)
{
	if (pCurve)
	{
		for (int keyID = 0; keyID < pCurve->KeyGetCount(); ++keyID)
		{
			Export::EntityAnimData entityData;

			entityData.dataType = paramType;

			FbxAnimCurveKey key = pCurve->KeyGet(keyID);
			entityData.keyValue = key.GetValue();

			FbxTime time = key.GetTime();
			entityData.keyTime = (float)time.GetSecondDouble();

			entityData.leftTangent = pCurve->KeyGetLeftDerivative(keyID);
			entityData.rightTangent = pCurve->KeyGetRightDerivative(keyID);
			entityData.leftTangentWeight = pCurve->KeyGetLeftTangentWeight(keyID);
			entityData.rightTangentWeight = pCurve->KeyGetRightTangentWeight(keyID);

			// If the Exporter is Sandbox or Maya, convert fov, positions
			string exporterName = m_pFBXScene->GetDocumentInfo()->Original_ApplicationName.Get();

			if (exporterName.MakeLower().Find("3ds") == -1)
			{

				if (paramType == eAnimParamType_PositionX || paramType == eAnimParamType_PositionY || paramType == eAnimParamType_PositionZ)
				{
					entityData.rightTangent /= 100.0f;
					entityData.leftTangent /= 100.0f;
					entityData.keyValue /= 100.0f;
				}
				else if (paramType == eAnimParamType_FOV)
				{
					const float kAspectRatio = 1.777778f;
					entityData.keyValue = Maya2SandboxFOVDeg(entityData.keyValue, kAspectRatio);
				}
			}

			pObject->AddEntityAnimationData(entityData);
		}
	}
}

bool CFBXExporter::ImportFromFile(const char* filename, SExportData* pData)
{
	FbxManager* pFBXManager = FbxManager::Create();

	FbxScene* pFBXScene = FbxScene::Create(pFBXManager, "Test");
	m_pFBXScene = pFBXScene;
	pFBXScene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::Max);
	pFBXScene->GetGlobalSettings().SetOriginalUpAxis(FbxAxisSystem::Max);

	FbxImporter* pImporter = FbxImporter::Create(pFBXManager, "");

	FbxIOSettings* pSettings = FbxIOSettings::Create(pFBXManager, IOSROOT);

	pFBXManager->SetIOSettings(pSettings);
	pSettings->SetBoolProp(IMP_FBX_ANIMATION, true);

	const bool lImportStatus = pImporter->Initialize(filename, -1, pFBXManager->GetIOSettings());
	if (!lImportStatus)
	{
		return false;
	}

	bool bLoadStatus = pImporter->Import(pFBXScene);

	if (!bLoadStatus)
	{
		return false;
	}

	FbxNode* pRootNode = pFBXScene->GetRootNode();

	if (pRootNode)
	{
		for (int animStackID = 0; animStackID < pFBXScene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId)); ++animStackID)
		{
			FbxAnimStack* pAnimStack = (FbxAnimStack*)pFBXScene->GetSrcObject(FbxCriteria::ObjectType(FbxAnimStack::ClassId), animStackID);
			const int animLayersCount = pAnimStack->GetMemberCount(FbxCriteria::ObjectType(FbxAnimLayer::ClassId));

			for (int layerID = 0; layerID < animLayersCount; ++layerID)
			{
				FbxAnimLayer* pAnimLayer = (FbxAnimLayer*)pAnimStack->GetMember(FbxCriteria::ObjectType(FbxAnimLayer::ClassId), layerID);

				const int nodeCount = pRootNode->GetChildCount();

				for (int nodeID = 0; nodeID < nodeCount; ++nodeID)
				{
					FbxNode* pNode = pRootNode->GetChild(nodeID);

					if (pNode)
					{
						FbxAnimCurve* pCurve = 0;
						SExportObject* pObject = pData->AddObject(pNode->GetName());

						if (pObject)
						{
							cry_strcpy(pObject->name, pNode->GetName());

							FbxCamera* pCamera = pNode->GetCamera();

							if (pCamera)
							{
								pNode->SetPivotState(FbxNode::eSourcePivot, FbxNode::ePivotActive);
								pNode->SetPivotState(FbxNode::eDestinationPivot, FbxNode::ePivotActive);
								pNode->SetPostRotation(FbxNode::eDestinationPivot, FbxVector4(0, -90, -90));
								pNode->ConvertPivotAnimationRecursive(pAnimStack, FbxNode::eDestinationPivot, 30.0);

								pCurve = pCamera->FieldOfView.GetCurve(pAnimLayer, "FieldOfView", true);
								FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_FOV);
							}

							pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, "X");
							FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_PositionX);
							pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, "Y");
							FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_PositionY);
							pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, "Z");
							FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_PositionZ);
							pCurve = pNode->LclRotation.GetCurve(pAnimLayer, "X");
							FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_RotationX);
							pCurve = pNode->LclRotation.GetCurve(pAnimLayer, "Y");
							FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_RotationY);
							pCurve = pNode->LclRotation.GetCurve(pAnimLayer, "Z");
							FillAnimationData(pObject, pAnimLayer, pCurve, eAnimParamType_RotationZ);
						}
					}
				}
			}
		}
	}

	return true;
}

