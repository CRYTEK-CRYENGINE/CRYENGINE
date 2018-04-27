// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MeshType.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "FilePathUtil.h"
#include <ThreadingUtils.h>

#include <AssetSystem/AssetEditor.h>
#include <Cry3DEngine/I3DEngine.h>
#include <ProxyModels/ItemModelAttribute.h>

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

#include "AssetSystem/AssetResourceSelector.h"

REGISTER_ASSET_TYPE(CMeshType);

// Detail attributes.
CItemModelAttribute CMeshType::s_triangleCountAttribute("Triangle count", eAttributeType_Int, CItemModelAttribute::StartHidden);
CItemModelAttribute CMeshType::s_vertexCountAttribute("Vertex count", eAttributeType_Int, CItemModelAttribute::StartHidden);
CItemModelAttribute CMeshType::s_materialCountAttribute("Material count", eAttributeType_Int, CItemModelAttribute::StartHidden);

CAssetEditor* CMeshType::Edit(CAsset* asset) const
{
	return CAssetEditor::OpenAssetForEdit("Mesh", asset);
}

CryIcon CMeshType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_mesh.ico");
}

std::vector<CItemModelAttribute*> CMeshType::GetDetails() const
{
	return
	{
		&s_triangleCountAttribute,
		&s_vertexCountAttribute,
		&s_materialCountAttribute
	};
}

QVariant CMeshType::GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const
{
	if (pDetail == &s_triangleCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("triangleCount"), pDetail);
	}
	else if (pDetail == &s_vertexCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("vertexCount"), pDetail);
	}
	else if (pDetail == &s_materialCountAttribute)
	{
		return GetVariantFromDetail(pAsset->GetDetailValue("materialCount"), pDetail);
	}
	return QVariant();
}

bool CMeshType::MoveAsset(CAsset* pAsset, const char* szNewPath, bool bMoveSourcefile) const
{
	if (!CAssetType::MoveAsset(pAsset, szNewPath, bMoveSourcefile))
	{
		return false;
	}

	// A workarond to refresh mesh dependencies in the cryasset file.
	// cgf may refers material without path, so this dependency may be broken if the cgf is moved without the material. 
	// TODO: Move all dependency generation stuff to a virtual CAssetType::OnSave
	const string filePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetFile(0));
	ThreadingUtils::AsyncQueue([filePath]()
	{
		CResourceCompilerHelper::CallResourceCompiler(
			filePath.c_str(),
			"/overwriteextension=cryasset",
			nullptr,
			false, // may show window?
			CResourceCompilerHelper::eRcExePath_editor,
			true,  // silent?
			true); // no user dialog?
	});

	return true;
}

void CMeshType::GenerateThumbnail(const CAsset* pAsset) const
{
	ThreadingUtils::PostOnMainThread([=]()
	{
		CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
	if (!pManager)
		return;

	CRY_ASSERT(0 < pAsset->GetFilesCount());
	if (1 > pAsset->GetFilesCount())
		return;

	const char* meshFileName = pAsset->GetFile(0);
	if (!meshFileName || !*meshFileName)
		return;

	const string thumbnailFileName = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetThumbnailPath());

	static QPreviewWidget* pPreviewWidget = new QPreviewWidget();
	pPreviewWidget->SetGrid(false);
	pPreviewWidget->SetAxis(false);
	pPreviewWidget->EnableMaterialPrecaching(true);
	pPreviewWidget->LoadFile(meshFileName);
	pPreviewWidget->setGeometry(0, 0, ASSET_THUMBNAIL_SIZE, ASSET_THUMBNAIL_SIZE);
	pPreviewWidget->FitToScreen();

		pPreviewWidget->SavePreview(thumbnailFileName.c_str());
	}).get();
}

//Group all mesh-derived types into one practical "AnyMesh" selector
REGISTER_RESOURCE_SELECTOR_ASSET_MULTIPLETYPES("AnyMesh", std::vector<string>({ "Mesh", "SkinnedMesh", "AnimatedMesh" }));

//Legacy support for all "models" which actually includes things that are not meshes such as skeletons or characters
REGISTER_RESOURCE_SELECTOR_ASSET_MULTIPLETYPES("Model", std::vector<string>({ "Mesh", "Skeleton", "SkinnedMesh", "Character", "AnimatedMesh" }));
