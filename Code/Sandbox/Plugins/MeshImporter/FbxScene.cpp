// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Tools for FBX file importing.

#include "StdAfx.h"
#include "FbxScene.h"
#include "FbxUtil.h"
#include <CryMath/Cry_Geo.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CrySystem/ITimer.h>

// To check for .mtl files in the current project root.
#include <CrySystem/ISystem.h>
#include <CrySystem/IProjectManager.h>
#include <QtUtil.h>
#include <QDirIterator> 


void LogPrintf(const char* szFormat, ...);

namespace FbxTool
{
// Percentages per phase of loading
static const float kInitializePercentage = 0.05f;
static const float kImportPercentage = 0.70f;
static const float kPostPercentage = 0.25f;

static const char* const kDefaultNodeName = "<anonymous>";
static const char* const kDefaultMaterialName = "Default";
static const char* const kDefaultMappingName = "<anonymous>";

const char* MaterialPhysicalizeSettingToString(EMaterialPhysicalizeSetting ps)
{
	switch(ps)
	{
	case eMaterialPhysicalizeSetting_None:
		return "none";
	case eMaterialPhysicalizeSetting_Default:
		return "default";
	case eMaterialPhysicalizeSetting_Obstruct:
		return "obstruct";
	case eMaterialPhysicalizeSetting_NoCollide:
		return "no collide";
	case eMaterialPhysicalizeSetting_ProxyOnly:
		return "proxy only (no draw)";
	default:
		CRY_ASSERT(0 && "unkown material physicalize setting");
		return nullptr;
	}
}

// Adjust I/O settings.
// This function should set the minimum required things to import from the file to speed up loading.
// TODO: Tweak this to optimize for import speed of FBX SDK.
static void AdjustIOSettings(FbxIOSettings* pIOSettings, const SFileImportDescriptor& desc)
{
	assert(pIOSettings);

	// Password
	if (!desc.password.empty())
	{
		pIOSettings->SetStringProp(IMP_FBX_PASSWORD, desc.password.c_str());
		pIOSettings->SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);
	}

	// General
	pIOSettings->SetBoolProp(IMP_PRESETS, true);
	pIOSettings->SetBoolProp(IMP_STATISTICS, true);
	pIOSettings->SetBoolProp(IMP_GEOMETRY, true);
	pIOSettings->SetBoolProp(IMP_ANIMATION, false);
	pIOSettings->SetBoolProp(IMP_LIGHT, false);
	pIOSettings->SetBoolProp(IMP_ENVIRONMENT, false);
	pIOSettings->SetBoolProp(IMP_CAMERA, false);
	pIOSettings->SetBoolProp(IMP_VIEW_CUBE, false);
	pIOSettings->SetBoolProp(IMP_ZOOMEXTENTS, false);
	pIOSettings->SetBoolProp(IMP_GLOBAL_AMBIENT_COLOR, false);
	pIOSettings->SetBoolProp(IMP_META_DATA, false);
	pIOSettings->SetBoolProp(IMP_REMOVEBADPOLYSFROMMESH, false);   // ?
	pIOSettings->SetStringProp(IMP_EXTRACT_FOLDER, ".");

	// FBX
	pIOSettings->SetBoolProp(IMP_FBX_TEMPLATE, true);
	pIOSettings->SetBoolProp(IMP_FBX_PIVOT, true);
	pIOSettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
	pIOSettings->SetBoolProp(IMP_FBX_CHARACTER, false);
	pIOSettings->SetBoolProp(IMP_FBX_CONSTRAINT, false);
	pIOSettings->SetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false);
	pIOSettings->SetBoolProp(IMP_FBX_GOBO, false);
	pIOSettings->SetBoolProp(IMP_FBX_SHAPE, false);
	pIOSettings->SetBoolProp(IMP_FBX_LINK, false);
	pIOSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
	pIOSettings->SetBoolProp(IMP_FBX_TEXTURE, true);
	pIOSettings->SetBoolProp(IMP_FBX_MODEL, true);
	pIOSettings->SetBoolProp(IMP_FBX_ANIMATION, true);
	pIOSettings->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true);   // textures etc only needed for materials

	const string extractFolder = PathUtil::GetPathWithoutFilename(desc.filePath);
	pIOSettings->SetStringProp(IMP_EXTRACT_FOLDER, extractFolder.c_str());

	// 3DS
	pIOSettings->SetBoolProp(IMP_3DS_REFERENCENODE, false);
	pIOSettings->SetBoolProp(IMP_3DS_TEXTURE, true);
	pIOSettings->SetBoolProp(IMP_3DS_MATERIAL, true);
	pIOSettings->SetBoolProp(IMP_3DS_ANIMATION, false);
	pIOSettings->SetBoolProp(IMP_3DS_MESH, true);
	pIOSettings->SetBoolProp(IMP_3DS_LIGHT, false);
	pIOSettings->SetBoolProp(IMP_3DS_CAMERA, false);
	pIOSettings->SetBoolProp(IMP_3DS_AMBIENT_LIGHT, false);
	pIOSettings->SetBoolProp(IMP_3DS_RESCALING, true);
	pIOSettings->SetBoolProp(IMP_3DS_FILTER, false);
	pIOSettings->SetBoolProp(IMP_3DS_SMOOTHGROUP, false);

	// OBJ
	pIOSettings->SetBoolProp(IMP_OBJ_REFERENCE_NODE, false);

	// DXF
	pIOSettings->SetBoolProp(IMP_DXF_WELD_VERTICES, false);
	pIOSettings->SetBoolProp(IMP_DXF_OBJECT_DERIVATION, false);
	pIOSettings->SetBoolProp(IMP_DXF_REFERENCE_NODE, false);
}

struct SProgressContext
{
	ICallbacks* pCallbacks;
	float       prevProgress;
};

static bool OnProgress(void* pArgs, float percentage, const char* pStatus)
{
	SProgressContext* pContext = static_cast<SProgressContext*>(pArgs);
	assert(pContext && pContext->pCallbacks);
	if (pStatus && *pStatus)
	{
		pContext->pCallbacks->OnProgressMessage(pStatus);
	}
	if (percentage > pContext->prevProgress)
	{
		pContext->pCallbacks->OnProgressPercentage(int(percentage * kImportPercentage + kInitializePercentage * 100.0f));
		pContext->prevProgress = percentage;
	}
	return true;
}

// Post-process the FBX scene.
// This is called after import to handle import options.
static void PostProcess(FbxScene* pScene, const SFileImportDescriptor& desc)
{
	FbxGeometryConverter converter(pScene->GetFbxManager());
	const float kDegenerateRatio = 0.2f;
	const float kTriangulateRatio = 0.4f;
	const float kNormalGenRatio = 0.4f;

	const float currentRatios = ((desc.bRemoveDegeneratePolygons ? kDegenerateRatio : 0.0f)
	                             + (desc.bTriangulate ? kTriangulateRatio : 0.0f)
	                             + (desc.generateNormals != EGenerateNormals::Never ? kNormalGenRatio : 0.0f)) / kPostPercentage;
	float currentProgress = kInitializePercentage + kImportPercentage;

	if (desc.bRemoveDegeneratePolygons)
	{
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnProgressMessage("Removing degenerate polygons...");
		}

		FbxArray<FbxNode*> arrNodes;
		converter.RemoveBadPolygonsFromMeshes(pScene, &arrNodes);
		if (desc.pCallbacks)
		{
			const int numNodes = arrNodes.Size();
			string message;
			for (int i = 0; i < numNodes; ++i)
			{
				const string nodeName = GetNodePath(arrNodes.GetAt(i));
				message = string() + "One or more meshes at node " + nodeName + " had degenerate polygons removed";
				desc.pCallbacks->OnProgressMessage(message.c_str());
			}
		}

		currentProgress += kPostPercentage * (kDegenerateRatio / currentRatios);
	}

	if (desc.bTriangulate)
	{
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnProgressMessage("Triangulating geometry...");
			desc.pCallbacks->OnProgressPercentage(int(currentProgress * 100.0f));
		}

		std::vector<FbxGeometry*> toTriangulate;
		auto findGeometryToTriangulate = [&toTriangulate](FbxGeometry* pGeometry, FbxNode* pNode)
		{
			assert(pGeometry);
			if (FbxMesh* pMesh = FbxCast<FbxMesh>(pGeometry))
			{
				if (pMesh->IsTriangleMesh())
				{
					return;
				}
			}
			else if (!pGeometry->Is<FbxNurbsCurve>() && !pGeometry->Is<FbxNurbsSurface>() && !pGeometry->Is<FbxPatch>())
			{
				return;   // This type of geometry cannot be converted
			}
			toTriangulate.push_back(pGeometry);
		};
		VisitNodeAttributesOfType<FbxGeometry>(pScene, findGeometryToTriangulate);
		if (!toTriangulate.empty())
		{
			const float perMeshProgress = kTriangulateRatio / currentRatios / (float)toTriangulate.size();

			for (FbxGeometry* pGeometry : toTriangulate)
			{
				const char* szGeometryName = pGeometry->GetName();
				if (!szGeometryName || !*szGeometryName)
				{
					FbxNode* const pNode = pGeometry->GetNode();
					szGeometryName = pNode ? pNode->GetName() : nullptr;
					if (!szGeometryName || !*szGeometryName)
					{
						szGeometryName = "<anonymous>";
					}
				}
				if (desc.pCallbacks)
				{
					const string message = string() + "Triangulating " + szGeometryName + "...";
					desc.pCallbacks->OnProgressMessage(message.c_str());
					desc.pCallbacks->OnProgressPercentage(int(currentProgress * 100.0f));
				}
				const string warningMessage = string() + "Failed to triangulate " + szGeometryName;   // Capture szGeometryName before conversion, it invalidates the pointer
				const bool bResult = converter.Triangulate(pGeometry, true) != nullptr;
				if (!bResult && desc.pCallbacks)
				{
					desc.pCallbacks->OnWarning(warningMessage.c_str());
				}
				currentProgress += perMeshProgress;
			}
		}
		else
		{
			currentProgress += kTriangulateRatio / currentRatios;
		}
	}

	if (desc.generateNormals != EGenerateNormals::Never)
	{
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnProgressMessage("Generating normals...");
			desc.pCallbacks->OnProgressPercentage(int(currentProgress * 100.0f));
		}

		std::vector<FbxMesh*> toGenerate;
		const bool bOverwrite = (desc.generateNormals == EGenerateNormals::OverwriteSmooth) || (desc.generateNormals == EGenerateNormals::OverwriteHard);

		auto generator = [&toGenerate, bOverwrite](FbxMesh* pMesh, FbxNode* pNode)
		{
			if (!bOverwrite)
			{
				const int numLayers = pMesh->GetLayerCount();
				for (int i = 0; i < numLayers; ++i)
				{
					FbxLayer* const pLayer = pMesh->GetLayer(i);
					if (pLayer && pLayer->GetNormals())
					{
						return;
					}
				}
			}
			toGenerate.push_back(pMesh);
		};
		VisitNodeAttributesOfType<FbxMesh>(pScene, generator);

		if (!toGenerate.empty())
		{
			const bool bSmooth = (desc.generateNormals == EGenerateNormals::Smooth) || (desc.generateNormals == EGenerateNormals::OverwriteSmooth);
			const bool bClockwise = false;   // TODO: Configurable? Is CCW/CW an import setting?
			const float perMeshProgress = kNormalGenRatio / currentRatios / (float)toGenerate.size();

			for (FbxMesh* pMesh : toGenerate)
			{
				const char* szMeshName = pMesh->GetName();
				if (!szMeshName || !*szMeshName)
				{
					FbxNode* const pNode = pMesh->GetNode();
					szMeshName = pNode ? pNode->GetName() : nullptr;
					if (!szMeshName || !*szMeshName)
					{
						szMeshName = "<anonymous>";
					}
				}
				if (desc.pCallbacks)
				{
					const string message = string() + "Generating " + (bSmooth ? "smooth" : "hard") + " normals for " + szMeshName + "... ";
					desc.pCallbacks->OnProgressMessage(message.c_str());
					desc.pCallbacks->OnProgressPercentage(int(currentProgress * 100.0f));
				}
				const string warningMessage = string() + "Failed to generate " + (bSmooth ? "smooth" : "hard") + " normals for " + szMeshName;
				const bool bResult = pMesh->GenerateNormals(true, bSmooth, bClockwise);
				if (!bResult && desc.pCallbacks)
				{

					desc.pCallbacks->OnWarning(warningMessage.c_str());
				}
				currentProgress += perMeshProgress;
			}
		}
	}

	if (desc.pCallbacks)
	{
		desc.pCallbacks->OnProgressMessage("Scene post-process completed");
		desc.pCallbacks->OnProgressPercentage(100);
	}
}

static string ToString(const FbxAxisSystem& axisSystem)
{
	int upSign, fwdSign;
	FbxAxisSystem::EUpVector up = axisSystem.GetUpVector(upSign);
	FbxAxisSystem::EFrontVector fwd = axisSystem.GetFrontVector(fwdSign);
	FbxAxisSystem::ECoordSystem cs = axisSystem.GetCoorSystem();

	string res;

	if (upSign < 0)
	{
		res += "-";
	}

	switch (up)
	{
	case FbxAxisSystem::eXAxis:
		res += "eXAxis, ";
		break;
	case FbxAxisSystem::eYAxis:
		res += "eYAxis, ";
		break;
	case FbxAxisSystem::eZAxis:
		res += "eZAxis, ";
		break;
	default:
		assert(0 && "unkown up vector");
		break;
	}

	if (fwdSign < 0) res += "-";
	switch (fwd)
	{
	case FbxAxisSystem::eParityEven:
		res += "ePartiyEven, ";
		break;
	case FbxAxisSystem::eParityOdd:
		res += "ePartiyOdd, ";
		break;
	default:
		assert(0 && "unkown front vector");
	}

	switch (cs)
	{
	case FbxAxisSystem::eRightHanded:
		res += "eRightHanded";
		break;
	case FbxAxisSystem::eLeftHanded:
		res += "eLeftHanded";
		break;
	default:
		assert(0 && "unknown coordinate system");
	}

	return res;
}

static const char* ToString(Axes::EAxis axis)
{
	switch (axis)
	{
	case Axes::EAxis::PosX:
		return "PosX";
	case Axes::EAxis::PosY:
		return "PosY";
	case Axes::EAxis::PosZ:
		return "PosZ";
	case Axes::EAxis::NegX:
		return "NegX";
	case Axes::EAxis::NegY:
		return "NegY";
	case Axes::EAxis::NegZ:
		return "NegZ";
	default:
		assert(0 && "unkown axis");
		return nullptr;
	}
}

static int GetSign(Axes::EAxis axis)
{
	assert(axis < Axes::EAxis::COUNT);
	switch (axis)
	{
	case Axes::EAxis::PosX:
	case Axes::EAxis::PosY:
	case Axes::EAxis::PosZ:
		return 1;
	case Axes::EAxis::NegX:
	case Axes::EAxis::NegY:
	case Axes::EAxis::NegZ:
		return -1;
	default:
		assert(0 && "unkown axis");
		return 0;
	}
}

namespace Axes
{

EAxis Abs(EAxis axis)
{
	assert(axis < EAxis::COUNT);
	switch (axis)
	{
	case EAxis::NegX:
		return EAxis::PosX;
	case EAxis::NegY:
		return EAxis::PosY;
	case EAxis::NegZ:
		return EAxis::PosZ;
	default:
		assert(EAxis::PosX <= axis && axis <= EAxis::PosZ);
		return axis;
	}
}

}   // namespace Axes

static Axes::EAxis Negate(Axes::EAxis axis)
{
	assert(axis < Axes::EAxis::COUNT);
	switch (axis)
	{
	case Axes::EAxis::PosX:
		return Axes::EAxis::NegX;
	case Axes::EAxis::PosY:
		return Axes::EAxis::NegY;
	case Axes::EAxis::PosZ:
		return Axes::EAxis::NegZ;
	case Axes::EAxis::NegX:
		return Axes::EAxis::PosX;
	case Axes::EAxis::NegY:
		return Axes::EAxis::PosY;
	case Axes::EAxis::NegZ:
		return Axes::EAxis::PosZ;
	default:
		assert(0 && "unkown axis");
		return axis;
	}
}

static void GetAxesFromFbxAxisSystem(
  const FbxAxisSystem& axisSystem,
  Axes::EAxis& fwd, Axes::EAxis& up)
{
	int upSign = 0;
	const FbxAxisSystem::EUpVector fbxUpAxis = axisSystem.GetUpVector(upSign);

	switch (fbxUpAxis)
	{
	case FbxAxisSystem::eXAxis:
		up = Axes::EAxis::PosX;
		break;
	case FbxAxisSystem::eYAxis:
		up = Axes::EAxis::PosY;
		break;
	case FbxAxisSystem::eZAxis:
		up = Axes::EAxis::PosZ;
		break;
	default:
		assert(0 && "unkown up vector");
		break;
	}

	int fwdSign = 0;
	const FbxAxisSystem::EFrontVector fbxFwdAxis = axisSystem.GetFrontVector(fwdSign);

	// Get integers from up-axis and forward-axis in range [0, 2] and [0, 1],
	// respectively, which can be used for lookup.
	const int upAxis = static_cast<int>(fbxUpAxis) - 1;
	const int parity = static_cast<int>(fbxFwdAxis) - 1;
	assert(0 <= upAxis && upAxis <= 2);
	assert(0 <= parity && parity <= 1);

	static const Axes::EAxis lutFwd[] =
	{
		Axes::EAxis::PosY, Axes::EAxis::PosZ,   // up = x
		Axes::EAxis::PosX, Axes::EAxis::PosZ,   // up = y
		Axes::EAxis::PosX, Axes::EAxis::PosY,   // up = z
	};

	fwd = lutFwd[upAxis * 2 + parity];

	if (upSign < 0)
	{
		up = Negate(up);
	}
	if (fwdSign < 0)
	{
		fwd = Negate(fwd);
	}
}

std::unique_ptr<CScene> CScene::ImportFile(const SFileImportDescriptor& desc)
{
	static CryCriticalSection m_fbxSdkMutex;
	AUTO_LOCK(m_fbxSdkMutex); // The FBX SDK is not multi-threading safe.
	return FbxTool::CScene::ImportFileInternal(desc); // This is supposed to be the only place where CScene::ImportFileInternal() is called.
}

class CNullCallbacks : public FbxTool::ICallbacks
{
public:
	virtual void OnProgressMessage(const char* szMessage) override {}
	virtual void OnProgressPercentage(int percentage) override {}
	virtual void OnWarning(const char* szMessage) override {}
	virtual void OnError(const char* szMessage) override {}

public:
	static CNullCallbacks theInstance;
};
CNullCallbacks CNullCallbacks::theInstance;

std::unique_ptr<CScene> CScene::ImportFileInternal(SFileImportDescriptor desc)
{
	if (!desc.pCallbacks)
	{
		desc.pCallbacks = &CNullCallbacks::theInstance;
	}

	if (desc.filePath.empty())
	{
		LogPrintf("CScene::ImportFile failed. Path is null.\n");
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnError("The import process failed");
		}
		return nullptr;
	}

	// The OBJ importer looks for the material template library (.mtl) in the process current directory.
	// Having different current directories for the RC and Sandbox we have an issue that RC and Sandbox may create a different scene for the same obj file.
	if (stricmp(PathUtil::GetExt(desc.filePath.c_str()), "obj") == 0)
	{
		QDirIterator iterator(QtUtil::ToQString(GetISystem()->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute()), { "*.mtl" }, QDir::Files);
		if (iterator.hasNext())
		{
			const string filePath = QtUtil::ToString(iterator.next());
			desc.pCallbacks->OnError(string().Format("Unable to import obj file if the project's root folder contains \"*.mtl\" files.\n"
				"Please move all the *.mtl files outside the project root.\nFor example: %s", filePath.c_str()));
			return nullptr;
		}
	}

	TFbxManagerPtr pManager(FbxManager::Create());
	if (!pManager)
	{
		LogPrintf("Creating FBX manager failed.\n");
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnError("The import process failed");
		}
		return nullptr;
	}

	typedef std::unique_ptr<FbxIOSettings, SFbxSdkDeleter> TIOSettingsPtr;
	TIOSettingsPtr pIOSettings(FbxIOSettings::Create(pManager.get(), "FBX I/O settings"));
	if (!pIOSettings)
	{
		LogPrintf("Creating FBX IO settings failed.\n");
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnError("The import process failed");
		}
		return nullptr;
	}

	AdjustIOSettings(pIOSettings.get(), desc);
	pManager->SetIOSettings(pIOSettings.get());
	typedef std::unique_ptr<FbxImporter, SFbxSdkDeleter> TImporterPtr;
	TImporterPtr pImporter(FbxImporter::Create(pManager.get(), "FBX importer"));
	if (!pImporter)
	{
		LogPrintf("Creating FBX importer object failed.\n");
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnError("The import process failed");
		}
		return nullptr;
	}

	SProgressContext context;
	context.pCallbacks = desc.pCallbacks;
	context.prevProgress = 0.0f;
	if (desc.pCallbacks)
	{
		pImporter->SetProgressCallback(OnProgress, &context);
	}

	if (!pImporter->Initialize(desc.filePath.c_str(), -1, pIOSettings.get()))
	{
		if (pImporter->GetStatus() == FbxStatus::ePasswordError && !pIOSettings->GetBoolProp(IMP_FBX_PASSWORD_ENABLE, false))
		{
			const char* szPassword = desc.pCallbacks->OnPasswordRequired();
			if (szPassword)
			{
				pIOSettings->SetStringProp(IMP_FBX_PASSWORD, szPassword);
				pIOSettings->SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);

				if (!pImporter->Initialize(desc.filePath.c_str(), -1, pIOSettings.get()))
				{
					LogPrintf("CScene::ImportFile failed. Initializing importer for password protected file failed.\n");
					if (desc.pCallbacks)
					{
						desc.pCallbacks->OnError("The import process failed");
					}
					return nullptr;
				}
			}
			else
			{
				LogPrintf("CScene::ImportFile failed. Need password.\n");
				if (desc.pCallbacks)
				{
					desc.pCallbacks->OnError("The import process failed");
				}
				return nullptr;
			}
		}

		const string message = string() + "Cannot initialize importer for " + desc.filePath + ": " + pImporter->GetStatus().GetErrorString();
		desc.pCallbacks->OnError(message.c_str());

		return nullptr;
	}

	if (desc.pCallbacks)   // FBX version information
	{
		const string message = string() + "Initialized importer for " + desc.filePath;
		desc.pCallbacks->OnProgressMessage(message.c_str());
		desc.pCallbacks->OnProgressPercentage(unsigned(kInitializePercentage * 100.0f));

		if (pImporter->GetFileFormat() == 0)
		{
			int major, minor, revision;
			pImporter->GetFileVersion(major, minor, revision);
			if (major || minor || revision)
			{
				char szVersion[100];
				std::sprintf(szVersion, "FBX file version: %d.%d.%d", major, minor, revision);
				desc.pCallbacks->OnProgressMessage(szVersion);
			}
		}
	}

	TFbxScenePtr pScene(FbxScene::Create(pManager.get(), desc.filePath.c_str()));
	if (!pScene)
	{
		LogPrintf("Creating FBX scene failed.\n");
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnError("The import process failed. Try re-exporting the file with the latest FBX plug-in for your DCC package.");
		}
		return nullptr;
	}

	if (!pImporter->Import(pScene.get()))
	{
		LogPrintf("Importing scene failed.\n");
		if (desc.pCallbacks)
		{
			desc.pCallbacks->OnError("The import process failed. Try re-exporting the file with the latest FBX plug-in for your DCC package.");
		}
		return nullptr;
	}

	pManager->SetIOSettings(nullptr);
	pIOSettings.reset();
	pImporter.reset();

	// Log scale.
	const FbxSystemUnit fileUnit = pScene->GetGlobalSettings().GetSystemUnit();
	if (fileUnit.GetMultiplier() != 1.0)
	{
		LogPrintf("\tignoring multiplier with value != 1\n");
	}

	PostProcess(pScene.get(), desc);

	// User-accessible wrappers
	return std::unique_ptr<CScene>(new CScene(std::move(pManager), std::move(pScene), desc));
}

// TODO: GetAxisSystem() prevents const attribute.
Axes::EAxis CScene::GetUpAxis()
{
	const FbxAxisSystem axisSystem = GetFbxScene()->GetGlobalSettings().GetAxisSystem();
	Axes::EAxis up, fwd;
	GetAxesFromFbxAxisSystem(axisSystem, fwd, up);
	return up;
}

// TODO: GetAxisSystem() prevents const attribute.
Axes::EAxis CScene::GetForwardAxis()
{
	const FbxAxisSystem axisSystem = GetFbxScene()->GetGlobalSettings().GetAxisSystem();
	Axes::EAxis up, fwd;
	GetAxesFromFbxAxisSystem(axisSystem, fwd, up);
	return fwd;
}

double CScene::GetFileUnitSizeInCm() const
{
	return m_fileScale;
}

void CScene::SetGlobalScale(double scale)
{
	m_globalScale = scale;
	UpdateRootTransformation();
}

void CScene::SetGlobalRotation(const Matrix33d& rotation)
{
	m_globalRotation = rotation;
	UpdateRootTransformation();
}

int CScene::GetMaterialCount() const
{
	const size_t numMats = GetMaterials().size();
	assert(numMats < INT_MAX);
	return static_cast<int>(numMats);
}

int CScene::GetMeshCount() const
{
	const size_t numMeshes = GetMeshes().size();
	assert(numMeshes < INT_MAX);
	return static_cast<int>(numMeshes);
}

int CScene::GetAnimationTakeCount() const
{
	return (int)m_animationTakes.size();
}

void CScene::UpdateRootTransformation()
{
	Matrix33d scale;
	scale.SetScale(Vec3d(m_globalScale, m_globalScale, m_globalScale));
	m_pRootNode->postTransform = m_globalRotation * scale;
}

int CScene::GetNodeCount() const
{
	return static_cast<int>(m_nodes.size());
}

const std::vector<std::unique_ptr<SMaterial>>& CScene::GetMaterials() const
{
	return m_materials;
}

const std::vector<std::unique_ptr<SMesh>>& CScene::GetMeshes() const
{
	return m_meshes;
}

static void CreateDummyMaterial(SMaterial* pResult)
{
	pResult->szName = SMaterial::DummyMaterialName();
}

static bool GetFbxPropertyNamesFromChannelType(EMaterialChannelType channelType, std::vector<string>& properties)
{
	properties.clear();

	switch (channelType)
	{
	case eMaterialChannelType_Diffuse:
		properties.push_back(FbxSurfaceMaterial::sDiffuse);
		return true;
	case eMaterialChannelType_Bump:
		properties.push_back(FbxSurfaceMaterial::sNormalMap);
		properties.push_back(FbxSurfaceMaterial::sBump);
		return true;
	case eMaterialChannelType_Specular:
		properties.push_back(FbxSurfaceMaterial::sSpecular);
		return true;
	default:
		CRY_ASSERT(0 && "unknown channel type");
		return false;
	}
}

SMaterialChannel::SMaterialChannel()
	: m_bHasTexture(false)
{}

static void CreateMaterial(SMaterial* pResult, FbxSurfaceMaterial* pFbxMaterial)
{
	// set name
	pResult->szName = pFbxMaterial->GetName();
	if (pResult->szName == nullptr || !*pResult->szName)
	{
		pResult->szName = kDefaultMaterialName;
	}

	// Initialize material channels.

	std::vector<string> propertyNames; // all the properties that map to this channel.

	for (int i = 0; i < eMaterialChannelType_COUNT; ++i)
	{
		const EMaterialChannelType channelType = EMaterialChannelType(i);
		GetFbxPropertyNamesFromChannelType(channelType, propertyNames);

		for (auto& propName : propertyNames)
		{
			FbxProperty prop = pFbxMaterial->FindProperty(propName.c_str(), true);
			if (!prop.IsValid())
			{
				continue;
			}

			if (prop.GetPropertyDataType().GetType() != eFbxDouble3)
			{
				LogPrintf("%s: Property '%s' of material '%s' has unexpected type '%s'\n",
					__FUNCTION__, prop.GetName(), pFbxMaterial->GetName(), prop.GetPropertyDataType().GetName());
				continue;
			}

			const FbxDouble3 color = prop.Get<FbxDouble3>();
			SMaterialChannel& channel = pResult->m_materialChannels[channelType];
			channel.m_color = ColorF((float)color[0], (float)color[1], (float)color[2]);

			// Select single texture for this channel.

			for (int j = 0; j < prop.GetSrcObjectCount(); ++j)
			{
				FbxFileTexture* const pFbxFileTexture = prop.GetSrcObject<FbxFileTexture>(j);
				if (!pFbxFileTexture)
				{
					continue;
				}

				if (channel.m_bHasTexture)
				{
					LogPrintf("%s: Ignore additional texture file '%s' for property '%s' of material '%s'. Texture is already '%s'\n",
						__FUNCTION__, pFbxFileTexture->GetName(), prop.GetName(), pFbxMaterial->GetName(), channel.m_textureFilename.c_str());
					continue;
				}

				channel.m_textureName = pFbxFileTexture->GetName();
				channel.m_textureFilename = pFbxFileTexture->GetFileName();
				channel.m_bHasTexture = true;
			}

			for (int j = 0; j < prop.GetSrcObjectCount(); ++j)
			{
				FbxLayeredTexture* const pFbxLayeredTexture = prop.GetSrcObject<FbxLayeredTexture>(j);
				if (!pFbxLayeredTexture)
				{
					continue;
				}

				for (int srcObj = 0; srcObj < pFbxLayeredTexture->GetSrcObjectCount(); ++srcObj)
				{
					FbxFileTexture* const pFbxFileTexture = pFbxLayeredTexture->GetSrcObject<FbxFileTexture>(srcObj);
					if (!pFbxFileTexture)
					{
						continue;
					}

					if (channel.m_bHasTexture)
					{
						LogPrintf("%s: Ignore additional texture layer '%s' for property '%s' of material '%s'. Texture is already '%s'\n",
							__FUNCTION__, pFbxFileTexture->GetName(), prop.GetName(), pFbxMaterial->GetName(), channel.m_textureFilename.c_str());
						continue;
					}

					channel.m_textureName = pFbxFileTexture->GetName();
					channel.m_textureFilename = pFbxFileTexture->GetFileName();
					channel.m_bHasTexture = true;
				}
			}
		}
	}
}

static void CreateAllMaterials(FbxScene* pScene, std::vector<std::unique_ptr<SMaterial>>& materials)
{
	materials.clear();

	int nextId = 0;

	// Dummy material.
	{
		std::unique_ptr<SMaterial> pMaterial(new SMaterial());
		pMaterial->id = nextId++;
		CreateDummyMaterial(pMaterial.get());
		materials.emplace_back(std::move(pMaterial));
	}

	// All materials included in the scene.

	FbxArray<FbxSurfaceMaterial*> sceneMaterials;
	pScene->FillMaterialArray(sceneMaterials);

	for (int i = 0; i < sceneMaterials.Size(); ++i)
	{
		FbxSurfaceMaterial* pFbxMaterial = sceneMaterials[i];
		const char* const szName = pFbxMaterial->GetName();
		const auto matIt = std::find_if(materials.begin(), materials.end(), [szName](const std::unique_ptr<SMaterial>& m)
			{
				return stricmp(m->szName, szName) == 0;
		  });
		if (matIt != materials.end())
		{
			// Ignore materials with duplicated name. A material should be uniquely identified
			// by its name.
			LogPrintf("ignoring material with duplicated name '%s'\n", szName);
			continue;
		}

		std::unique_ptr<SMaterial> pMaterial(new SMaterial());
		pMaterial->id = nextId++;
		CreateMaterial(pMaterial.get(), pFbxMaterial);
		materials.emplace_back(std::move(pMaterial));
	}
}

#define CASE_OF_ENUM(x) case FbxNodeAttribute:: ## x: \
  return # x

static const char* GetNameOfAttribute(const FbxNodeAttribute* pAtt)
{
	switch (pAtt->GetAttributeType())
	{
		CASE_OF_ENUM(eUnknown);
		CASE_OF_ENUM(eNull);
		CASE_OF_ENUM(eMarker);
		CASE_OF_ENUM(eSkeleton);
		CASE_OF_ENUM(eMesh);
		CASE_OF_ENUM(eNurbs);
		CASE_OF_ENUM(ePatch);
		CASE_OF_ENUM(eCamera);
		CASE_OF_ENUM(eCameraStereo);
		CASE_OF_ENUM(eCameraSwitcher);
		CASE_OF_ENUM(eLight);
		CASE_OF_ENUM(eOpticalReference);
		CASE_OF_ENUM(eOpticalMarker);
		CASE_OF_ENUM(eNurbsCurve);
		CASE_OF_ENUM(eTrimNurbsSurface);
		CASE_OF_ENUM(eBoundary);
		CASE_OF_ENUM(eNurbsSurface);
		CASE_OF_ENUM(eShape);
		CASE_OF_ENUM(eLODGroup);
		CASE_OF_ENUM(eSubDiv);
		CASE_OF_ENUM(eCachedEffect);
		CASE_OF_ENUM(eLine);
	default:
		return "<unkown attribute>";
	}
}

SNode* CScene::NewNode()
{
	std::unique_ptr<SNode> pNode(new SNode());
	pNode->id = m_nodes.size();
	m_nodes.push_back(std::move(pNode));
	return m_nodes.back().get();
}

SMesh* CScene::NewMesh()
{
	std::unique_ptr<SMesh> pMesh(new SMesh());
	pMesh->id = m_meshes.size();
	m_meshes.push_back(std::move(pMesh));
	return m_meshes.back().get();
}

void CScene::InitializeNode(SNode* pNode, FbxNode* pFbxNode)
{
	pNode->szName = pFbxNode->GetName();
	if (!pNode->szName || !*pNode->szName)
	{
		pNode->szName = kDefaultNodeName;
	}
	pNode->pScene = this;

	// Handle visibility (assumes parent nodes are processed before child nodes)
	const bool bVisible = (pFbxNode->Visibility.IsValid() ? pFbxNode->Visibility.Get() != 0.0 : true) && pNode->szName[0] != '_';
	const bool bInheritVisibility = pFbxNode->VisibilityInheritance.IsValid() ? pFbxNode->VisibilityInheritance.Get() : true;
	const bool bInheritedVisibility = pNode->pParent ? pNode->pParent->bVisible : true;
	pNode->bVisible = bInheritVisibility ? bInheritedVisibility && bVisible : bVisible;

	// Transform of the node, just store this immediately
	const FbxAMatrix& transform = pFbxNode->EvaluateLocalTransform();
	StoreTransform(pNode, transform);

	// Geometry transform is the transform that applies to a node attribute (such as a mesh in our case)
	pNode->geometryTransform.SetTRS(pFbxNode->GetGeometricTranslation(FbxNode::eSourcePivot), pFbxNode->GetGeometricRotation(FbxNode::eSourcePivot), pFbxNode->GetGeometricScaling(FbxNode::eSourcePivot));

	// Check attributes.
	for (int i = 0; i < pFbxNode->GetNodeAttributeCount(); ++i)
	{
		pNode->bAnyAttributes = true;
		const FbxNodeAttribute* const pAttribute = pFbxNode->GetNodeAttributeByIndex(i);
		pNode->namedAttributes.push_back(GetNameOfAttribute(pAttribute));
		switch (pAttribute->GetAttributeType())
		{
		case FbxNodeAttribute::eNull:
			pNode->bAttributeNull = true;
			break;
		case FbxNodeAttribute::eMesh:
			pNode->bAttributeMesh = true;
			break;
		default:
			break;
		}
	}
}

bool FileExists(const string& path);

void CScene::InitializeAnimations()
{
	FbxTimeSpan timeLineTimeSpan;
	m_pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeLineTimeSpan);

	FbxArray<FbxString*> animStackNameArray;
	m_pScene->FillAnimStackNameArray(animStackNameArray);

	m_animationTakes.reserve(animStackNameArray.Size() + 1);

	for (int i = 0; i < animStackNameArray.Size(); ++i)
	{
		FbxTime start = timeLineTimeSpan.GetStart();
		FbxTime stop = timeLineTimeSpan.GetStop();

		const FbxTakeInfo* const pTakeInfo = m_pScene->GetTakeInfo(*(animStackNameArray[i]));
		if (pTakeInfo)
		{
			start = pTakeInfo->mLocalTimeSpan.GetStart();
			stop = pTakeInfo->mLocalTimeSpan.GetStop();
		}

		SAnimationTake animTake;
		animTake.name = animStackNameArray[i]->Buffer();
		animTake.startFrame = (int)start.GetFrameCount();
		animTake.endFrame = (int)stop.GetFrameCount();
		m_animationTakes.push_back(animTake);
	}

	if (m_animationTakes.empty())
	{
		// Default take.
		SAnimationTake defaultTake;
		defaultTake.name = "";
		defaultTake.startFrame = (int)timeLineTimeSpan.GetStart().GetFrameCount();
		defaultTake.endFrame = (int)timeLineTimeSpan.GetStop().GetFrameCount();
		m_animationTakes.push_back(defaultTake);
	}
}

SNode* CScene::Initialize(const SFileImportDescriptor& desc)
{
	// Create and link nodes
	int nextId = 0;
	int meshCount = 0;

	const int nodeCount = m_pScene->GetNodeCount();

	// Create root node.

	std::vector<FbxNode*> associatedNodes(nodeCount, nullptr);
	std::vector<FbxMesh*> associatedMeshes(nodeCount, nullptr);

	m_pRootNode = NewNode();
	associatedNodes[m_pRootNode->id] = m_pScene->GetRootNode();

	// Create all nodes.

	std::vector<SNode*> nodeStack;
	nodeStack.push_back(m_pRootNode);
	while (!nodeStack.empty())
	{
		SNode* pNode = nodeStack.back();
		FbxNode* pFbxNode = associatedNodes[pNode->id];
		nodeStack.pop_back();

		InitializeNode(pNode, pFbxNode);

		for (int i = 0; i < pFbxNode->GetChildCount(); ++i)
		{
			SNode* pChild = NewNode();
			FbxNode* pFbxChild = pFbxNode->GetChild(i);
			associatedNodes[pChild->id] = pFbxChild;

			// Add child.
			pChild->pParent = pNode;
			pNode->childrenStorage.push_back(pChild);

			nodeStack.push_back(pChild);
		}

		if (pFbxNode->GetMesh())
		{
			meshCount++;
		}
	}

	m_nodeUserData.resize(m_nodes.size());

	if (!m_pRootNode)
	{
		return nullptr;
	}

	CreateAllMaterials(m_pScene.get(), m_materials);
	m_pDummyMaterial = m_materials[0].get();
	m_materialUserData.resize(m_materials.size());

	// Collect mesh instances
	const float progressDelta = 100.0f / meshCount;
	float progress = 0.0f;
	SCreateMeshStats stats;

	ITimer* const pTimer = gEnv->pSystem->GetITimer();
	const float startTime = pTimer->GetAsyncCurTime();

	for (auto& pNodeIt : m_nodes)
	{
		SNode* const pNode = pNodeIt.get();

		if (!pNode->bAttributeMesh)
		{
			continue;
		}

		FbxNode* pFbxNode = associatedNodes[pNode->id];

		for (int i = 0; i < pFbxNode->GetNodeAttributeCount(); ++i)
		{
			FbxNodeAttribute* pAttribute = pFbxNode->GetNodeAttributeByIndex(i);
			if (pAttribute->Is<FbxMesh>())
			{
				FbxMesh* pFbxMesh = (FbxMesh*)pAttribute;

				SMesh* pMesh = NewMesh();

				if (pMesh->id >= associatedMeshes.size())
				{
					associatedMeshes.resize(pMesh->id + 1);
				}
				associatedMeshes[pMesh->id] = pFbxMesh;

				if (InitializeMesh(desc.pCallbacks, pMesh, pFbxNode, pNode, pFbxMesh, &stats))
				{
					pNode->meshesStorage.push_back(pMesh);
				}
			}
		}

		progress += progressDelta;
		desc.pCallbacks->OnProgressPercentage((int)progress);
	}

	desc.pCallbacks->OnProgressMessage("Creating meshes...");
	desc.pCallbacks->OnProgressPercentage(0);

	desc.pCallbacks->OnProgressPercentage(100);

	// Delete unused materials.
	{
		if (!m_pDummyMaterial->numMeshesUsingIt)
		{
			m_pDummyMaterial = nullptr;
		}

		size_t i = 0;
		while (i < m_materials.size())
		{
			if (!m_materials[i]->numMeshesUsingIt)
			{
				std::swap(m_materials[i], m_materials.back());
				m_materials.pop_back();
			}
			else
			{
				i++;
			}
		}
	}

	// Create engine meshes.

	for (auto& meshIt : m_meshes)
	{
		SMesh* const pMesh = meshIt.get();
		const SNode* const pNode = pMesh->pNode;
		FbxNode* const pFbxNode = associatedNodes[pNode->id];
		FbxMesh* const pFbxMesh = associatedMeshes[pMesh->id];

		std::vector<std::unique_ptr<SDisplayMesh>> displayMeshes;
		const char* szError = CreateEngineMesh(*pFbxMesh, displayMeshes, pFbxNode, m_materials, &stats.createEngineMesh);
		if (szError)
		{
			LogPrintf("CreateEngineMesh() failed, %s\n", szError);
		}
		else
		{
			pMesh->displayMeshes = std::move(displayMeshes);
		}
	}

	desc.pCallbacks->OnProgressMessage("Indexing for user pointers...");

	// Indexing for user pointers
	for (auto& pNode : m_nodes)
	{
		pNode->numChildren = (TIndex)pNode->childrenStorage.size();
		pNode->ppChildren = pNode->numChildren != 0 ? pNode->childrenStorage.data() : nullptr;
		pNode->numMeshes = (TIndex)pNode->meshesStorage.size();
		pNode->ppMeshes = pNode->numMeshes != 0 ? pNode->meshesStorage.data() : nullptr;

		for (TIndex i = 0; i < pNode->numMeshes; ++i)
		{
			SMesh* const pMesh = static_cast<SMesh*>(pNode->meshesStorage[i]);
			pMesh->numMaterials = (TIndex)pMesh->materialsStorage.size();
			pMesh->ppMaterials = pMesh->numMaterials != 0 ? pMesh->materialsStorage.data() : nullptr;
		}
	}

	InitializeAnimations();	

	desc.pCallbacks->OnProgressMessage("Done.");

	return m_pRootNode;
}

SMesh::~SMesh() {}   // Needed for unique_ptr.

static void ComputeLocalTransform(const FbxTool::TVector& translation, const FbxTool::TVector& rotation, const FbxTool::TVector& scale, Matrix34r& transform)
{
	transform.Set(Vec3d(scale[0], scale[1], scale[2]), Quatd(rotation[3], rotation[0], rotation[1], rotation[2]), Vec3d(translation[0], translation[1], translation[2]));
}

static void ComputeGlobalTransform(const FbxTool::SMesh* pMesh, Matrix34& transform)
{
	assert(pMesh && pMesh->pNode);
	Matrix34d result;
	ComputeLocalTransform(pMesh->translation, pMesh->rotation, pMesh->scale, result);
	const FbxTool::SNode* pNode = pMesh->pNode;
	while (pNode)
	{
		Matrix34d parent;
		ComputeLocalTransform(pNode->translation, pNode->rotation, pNode->scale, parent);
		result = pNode->postTransform * parent * result;
		pNode = pNode->pParent;
	}
	transform = result;   // Convert to single precision
}

Matrix34 SMesh::GetGlobalTransform() const
{
	Matrix34 result;
	ComputeGlobalTransform(this, result);
	return result;
}

AABB SMesh::ComputeAabb(const Matrix34& transform) const
{
	AABB aabb(AABB::RESET);

	for (const auto& displayMesh : displayMeshes)
	{
		const CMesh* const pMesh = displayMesh.get()->pEngineMesh.get();

		if (!pMesh)
		{
			assert(0);
			continue;
		}

		const int vertexCount = pMesh->GetVertexCount();

		const Vec3* const pPositions = pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);

		if (vertexCount <= 0 || !pPositions)
		{
			continue;
		}

		assert(!pMesh->m_pPositionsF16);

		const int faceCount = pMesh->GetFaceCount();

		if (faceCount > 0)
		{
			const SMeshFace* const pFases = pMesh->GetStreamPtr<SMeshFace>(CMesh::FACES);

			for (int i = 0; i < faceCount; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					const int index = pFases[i].v[j];
					assert(index >= 0 && index < vertexCount);
					aabb.Add(transform.TransformPoint(pPositions[index]));
				}
			}
		}
		else
		{
			const int indexCount = pMesh->GetIndexCount();
			const vtx_idx* const pIndices = pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);

			for (int i = 0; i < indexCount; ++i)
			{
				aabb.Add(transform.TransformPoint(pPositions[pIndices[i]]));
			}
		}
	}

	return aabb;
}

bool CScene::InitializeMesh(ICallbacks* pCallbacks, SMesh* pMesh, FbxNode* pFbxNode, SNode* pNode, FbxMesh* pFbxMesh, SCreateMeshStats* pStats)
{
	ITimer* const pTimer = gEnv->pSystem->GetITimer();
	const float totalStartTime = pTimer->GetAsyncCurTime();
	float startTime, endTime;

	assert(pMesh && pFbxNode && pNode && pFbxMesh);
	pMesh->szName = pFbxMesh->GetName();
	if (pMesh->szName == nullptr || !*pMesh->szName)
	{
		pMesh->szName = kDefaultNodeName;
	}
	pMesh->pNode = pNode;
	pMesh->numTriangles = pFbxMesh->GetPolygonCount();
	pMesh->numVertices = pFbxMesh->GetControlPointsCount();

	pMesh->bHasSkin = pFbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;

	const int numMaterials = pFbxNode->GetMaterialCount();
	std::vector<FbxSurfaceMaterial*> usedMaterials(numMaterials, nullptr);

	bool bHasAnyMaterials = false;
	bool bHasInvalidIndices = false;

	startTime = pTimer->GetAsyncCurTime();

	const FbxLayerElementMaterial* const pMaterials = pFbxMesh->GetLayer(0) ? pFbxMesh->GetLayer(0)->GetMaterials() : nullptr;
	if (pMaterials)
	{
		const FbxLayerElementArrayTemplate<int>& materialIndices = pMaterials->GetIndexArray();
		const int numMaterialIndices = materialIndices.GetCount();

		for (int i = 0; i < numMaterialIndices; ++i)
		{
			const int materialIndex = materialIndices[i];
			if (0 > materialIndex || materialIndex >= numMaterials)
			{
				LogPrintf(
				  "In mesh '%s': material index %d is out of range [0, %d) "
				  "and will be assigned dummy material\n",
				  pFbxNode->GetName(), materialIndex, numMaterials);
				bHasInvalidIndices = true;
			}
			else if (!usedMaterials[materialIndex])
			{
				usedMaterials[materialIndex] = pFbxNode->GetMaterial(materialIndex);
				bHasAnyMaterials = true;
			}
		}
	}
	if (bHasAnyMaterials)
	{
		pMesh->materialsStorage.resize(numMaterials, nullptr);
		for (int i = 0; i < numMaterials; ++i)
		{
			FbxSurfaceMaterial* const pFbxMaterial = usedMaterials[i];
			if (pFbxMaterial)
			{
				SMaterial* pMaterial = GetMaterialByName_Internal(pFbxMaterial->GetName());
				pMaterial->numMeshesUsingIt++;
				pMesh->materialsStorage[i] = pMaterial;
			}
		}
	}
	if (bHasInvalidIndices || !bHasAnyMaterials)
	{
		SMaterial* pMaterial = m_pDummyMaterial;
		pMaterial->numMeshesUsingIt++;

		// This will assigned the highest material index to the dummy material.

		pMesh->materialsStorage.push_back(pMaterial);
	}

	endTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->elapsedSearchMaterials += endTime - startTime;
	}

	StoreTransform(pMesh, pNode->geometryTransform);

	const float totalEndTime = pTimer->GetAsyncCurTime();
	if (pStats)
	{
		pStats->elapsedTotal += totalEndTime - totalStartTime;
	}

	return true;
}

static const FbxUInt64 kMaterialMapId = 0;

bool CScene::IsDummyMaterial(const SMaterial* pMaterial) const
{
	return pMaterial == m_pDummyMaterial;
}

void CScene::SetNodeLod(const SNode* pNode, int lod)
{
	assert(pNode);
	assert(lod >= 0 && lod < MAX_STATOBJ_LODS_NUM);

	if (lod == m_nodeUserData[pNode->id].lod)
	{
		return;
	}

	m_nodeUserData[pNode->id].lod = lod;

	for (auto& obs : m_observers)
	{
		obs->OnNodeUserDataChanged(pNode);
	}
}

void CScene::SetProxy(const SNode* pNode, bool bProxy)
{
	assert(pNode);
	if (bProxy == m_nodeUserData[pNode->id].bIsProxy)
	{
		return;
	}

	m_nodeUserData[pNode->id].bIsProxy = bProxy;

	for (auto& obs : m_observers)
	{
		obs->OnNodeUserDataChanged(pNode);
	}
}

SMaterial* CScene::GetMaterialByName_Internal(const char* szName)
{
	const auto matIt = std::find_if(m_materials.begin(), m_materials.end(), [szName](const std::unique_ptr<SMaterial>& m)
		{
			return stricmp(m->szName, szName) == 0;
	  });
	return matIt != m_materials.end() ? matIt->get() : nullptr;
}

std::vector<string> GetPath(const FbxTool::SNode* pNode)
{
	assert(pNode);

	if (!pNode->pParent)
	{
		// Node is scene root.
		return std::vector<string>();
	}

	std::vector<string> pathToRoot;
	while (pNode->pParent)
	{
		pathToRoot.push_back(pNode->szName);
		pNode = pNode->pParent;
	}

	// Path should at least contain name of node itself.
	assert(!pathToRoot.empty());

	// Reverse order to get path from root to node.
	std::reverse(pathToRoot.begin(), pathToRoot.end());

	return pathToRoot;
}

static const SNode* FindChildNode(
  const SNode* pParent,
  std::vector<string>::const_iterator pathBegin,
  std::vector<string>::const_iterator pathEnd)
{
	assert(pParent);

	if (pathBegin == pathEnd)
	{
		return pParent;
	}

	for (int i = 0; i < pParent->numChildren; ++i)
	{
		const SNode* const pChild = pParent->ppChildren[i];
		if (string(pChild->szName) == *pathBegin)
		{
			return FindChildNode(pChild, pathBegin + 1, pathEnd);
		}
	}

	return nullptr;
}

const SNode* FindChildNode(const SNode* pParent, const std::vector<string>& path)
{
	if (path.empty())
	{
		return nullptr;
	}
	return FindChildNode(pParent, path.cbegin(), path.cend());
}

}

static const char* GetFormattedAxis(FbxTool::Axes::EAxis axis)
{
	switch (axis)
	{
	case FbxTool::Axes::EAxis::PosX:
		return "+X";
	case FbxTool::Axes::EAxis::PosY:
		return "+Y";
	case FbxTool::Axes::EAxis::PosZ:
		return "+Z";
	case FbxTool::Axes::EAxis::NegX:
		return "-X";
	case FbxTool::Axes::EAxis::NegY:
		return "-Y";
	case FbxTool::Axes::EAxis::NegZ:
		return "-Z";
	default:
		assert(0 && "unkown axis");
		return nullptr;
	}
}

// Returns axes string with the format expected by TransformHelpers functions.
string GetFormattedAxes(FbxTool::Axes::EAxis up, FbxTool::Axes::EAxis fwd)
{
	return string(GetFormattedAxis(fwd)) + string(GetFormattedAxis(up));
}

