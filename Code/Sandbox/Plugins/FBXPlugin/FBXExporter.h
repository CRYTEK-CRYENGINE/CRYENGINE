// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <IExportManager.h>
#include "fbxsdk.h"

enum EAxis
{
	eAxis_Y,
	eAxis_Z
};

struct SFBXSettings
{
	bool  bCopyTextures;
	bool  bEmbedded;
	bool  bAsciiFormat;
	EAxis axis;
};

class CFBXExporter : public IExporter
{
public:
	CFBXExporter();

	virtual const char* GetExtension() const;
	virtual const char* GetShortDescription() const;
	virtual bool        ExportToFile(const char* filename, const SExportData* pData);
	virtual bool        ImportFromFile(const char* filename, SExportData* pData);
	virtual void        Release();

private:
	FbxMesh*            CreateFBXMesh(const SExportObject* pObj);
	FbxFileTexture*     CreateFBXTexture(const char* pTypeName, const char* pName);
	FbxSurfaceMaterial* CreateFBXMaterial(const std::string& name, const SExportMesh* pMesh);
	FbxNode*            CreateFBXNode(const SExportObject* pObj);
	FbxNode*            CreateFBXAnimNode(FbxScene* pScene, FbxAnimLayer* pCameraAnimBaseLayer, const SExportObject* pObj);
	void                FillAnimationData(SExportObject* pObject, FbxAnimLayer* pAnimLayer, FbxAnimCurve* pCurve, EAnimParamType paramType);

	FbxManager*                                m_pFBXManager;
	SFBXSettings                               m_settings;
	FbxScene*                                  m_pFBXScene;
	std::string                                m_path;
	std::vector<FbxNode*>                      m_nodes;
	std::map<std::string, FbxSurfaceMaterial*> m_materials;
	std::map<const SExportMesh*, int>          m_meshMaterialIndices;
};

