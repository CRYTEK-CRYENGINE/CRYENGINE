// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Tools for FBX scene importing.

#pragma once
#if !defined(BUILDING_FBX_TOOL)
	#error This header must not be included directly, #include FbxTool.h instead
#endif

#include "FbxSdkInclude.h"
#include "FbxUtil.h"
#include "NodeProperties.h"
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>
#include <CrySerialization/IArchive.h>
#include <CryCore/StlUtils.h>
#include <array>
#include "AutoLodSettings.h"

class CMesh;

struct SCreateMeshStats;
struct AABB;

namespace FbxTool
{
class CScene;
struct SNode;

// Basic types, underlying data format dictated by FBX SDK
typedef int                    TIndex;
typedef double                 TScalar;
typedef std::array<TScalar, 4> TVector;

// Automatic deleter for FBX SDK types, for use with std::unique_ptr<FbxType>
struct SFbxSdkDeleter
{
	template<typename T>
	void operator()(T* pObject) const
	{
		pObject->Destroy();
	}
};

// Managed FBX pointer types
typedef std::unique_ptr<FbxManager, SFbxSdkDeleter> TFbxManagerPtr;
typedef std::unique_ptr<FbxScene, SFbxSdkDeleter>   TFbxScenePtr;

enum EMaterialChannelType
{
	eMaterialChannelType_Diffuse,
	eMaterialChannelType_Bump,
	eMaterialChannelType_Specular,
	eMaterialChannelType_COUNT
};

struct SMaterialChannel
{
	ColorF m_color;
	string m_textureName;
	string m_textureFilename;
	bool m_bHasTexture;

	SMaterialChannel();
};

struct SMaterial
{
	SMaterial()
		: id(-1)
		, numMeshesUsingIt(0)
	{}

	static const char* DummyMaterialName() { return "<unassigned>"; }

	int                id; // Uniquely identifies this material. In range [0, number of materials).

	const char*        szName;                  // Name of the material

	int                numMeshesUsingIt;

	SMaterialChannel m_materialChannels[eMaterialChannelType_COUNT];
};

struct SDisplayMesh
{
	// Identifies the single engine material (type IMaterial) that is used to draw this mesh.
	int                    uberMaterial;

	std::unique_ptr<CMesh> pEngineMesh;
};

struct SMesh
{
	int id;

	const char*  szName;                        // Name of the mesh
	TIndex       numTriangles;                  // Number of triangles in the mesh
	TIndex       numVertices;                   // Number of vertices in the mesh
	const SNode* pNode;                         // Node to which the mesh belongs

	std::vector<std::unique_ptr<SDisplayMesh>> displayMeshes;

	TIndex                  numMaterials;       // Number of items in ppMaterials array, always 0 if material import is disabled
	const SMaterial* const* ppMaterials;        // Indexed by pMaterialStream

	std::vector<SMaterial*> materialsStorage;

	TVector                 translation;        // Translation of the mesh, relative to the node (4th element unused)
	TVector                 rotation;           // Rotation of the mesh, relative to the node (stored as quaternion)
	TVector                 scale;              // Scale of the mesh, relative to the node (4th element unused)

	bool bHasSkin;

	~SMesh();

	Matrix34         GetGlobalTransform() const;

	AABB             ComputeAabb(const Matrix34& transform) const;

	const SMaterial* GetMaterialSafe(TIndex index) const
	{
		return index >= 0 && index < numMaterials ? ppMaterials[index] : nullptr;
	}
};

struct SNode
{
	int id;    // Uniquely identifies this node. In range [0, number of nodes).

	// Attributes.
	bool                bAnyAttributes;
	bool                bAttributeNull;
	bool                bAttributeMesh;
	std::vector<string> namedAttributes;

	const char*         szName;           // Name of the node
	TIndex              numChildren;      // Number of items in ppChildren array
	const SNode* const* ppChildren;       // Child nodes in the scene tree
	SNode*              pParent;          // Parent node, nullptr only for root node
	TIndex              numMeshes;        // Number of items in the ppMeshes array
	const SMesh* const* ppMeshes;         // Meshes attached to this node
	const CScene*       pScene;           // The scene to which this node belongs

	std::vector<SNode*> childrenStorage;
	std::vector<SMesh*> meshesStorage;
	FbxAMatrix          geometryTransform;

	TVector             translation;            // Translation of the node, relative to parent (4th element unused)
	TVector             rotation;               // Rotation of the node, relative to parent (stored as quaternion)
	TVector             scale;                  // Scale of the node, relative to parent (4th element unused)
	bool                bVisible;               // True if the node is considered visible

	// Applied after TRS transformation, so local transformation of node is
	// postTransform * TRS (TRS is translation, rotation, scaling matrix).
	Matrix34d postTransform;

	SNode()
		: id(-1)
		, pParent(nullptr)
		, bAnyAttributes(false)
		, bAttributeNull(false)
		, bAttributeMesh(false)
		, postTransform(IDENTITY)
	{}

	const SNode* GetChildSafe(TIndex index) const
	{
		return index >= 0 && index < numChildren ? ppChildren[index] : nullptr;
	}

	const SMesh* GetMeshSafe(TIndex index) const
	{
		return index >= 0 && index < numMeshes ? ppMeshes[index] : nullptr;
	}
};

std::vector<string> GetPath(const FbxTool::SNode* pNode);
const SNode*        FindChildNode(const SNode* pParent, const std::vector<string>& path);

struct SAnimationTake
{
	string name;
	int startFrame;  // Assuming 30fps.
	int endFrame;
};

// Feedback when importing FBX file
struct ICallbacks
{
	virtual void OnProgressMessage(const char* szMessage) = 0;
	virtual void OnProgressPercentage(int percentage) = 0;
	virtual void OnWarning(const char* szMessage) = 0;
	virtual void OnError(const char* szMessage) = 0;

	// In case the FBX file requires a password, this function will be called to retrieve it
	virtual const char* OnPasswordRequired()
	{
		return nullptr;
	}
};

enum class EGenerateNormals
{
	Never,                                      // Don't touch normal data
	Smooth,                                     // Create smooth normals, if not exists
	Hard,                                       // Create hard normals, if not exists
	OverwriteSmooth,                            // Overwrite existing with smooth normals
	OverwriteHard,                              // Overwrite existing with hard normals
};

struct SFileImportDescriptor
{
	ICallbacks*      pCallbacks;                // Callbacks for receiving progress information (optional)
	string           filePath;                  // Path to the file (required)
	string           password;                  // Password for the file (optional)
	bool             bTriangulate;              // If set, triangulates geometry that is not triangulated, including NURBS and PATCH geometry.
	bool             bRemoveDegeneratePolygons; // If set, removes degenerate polygons from meshes.
	EGenerateNormals generateNormals;           // Describes creation of normals on import

	SFileImportDescriptor()
		: pCallbacks(nullptr)
		, filePath()
		, password()
		, bTriangulate(false)
		, bRemoveDegeneratePolygons(true)
		, generateNormals(EGenerateNormals::Never)
	{}
};

// Returns pointer to first element in an array of file extensions.
// The number of extensions is returned through numExtensions parameter.
// File extensions do not include the dot prefix, and are null-terminated.
const char* const* GetSupportedFileExtensions(TIndex& numExtensions);

namespace Axes
{

enum class EAxis : int
{
	PosX = 0,
	PosY,
	PosZ,
	NegX,
	NegY,
	NegZ,
	COUNT,
};

EAxis Abs(EAxis axis);

}   // namespace Axes

enum ENodeExportSetting
{
	eNodeExportSetting_Exclude,
	eNodeExportSetting_Include,
	eNodeExportSetting_COUNT
};

enum EMaterialPhysicalizeSetting
{
	eMaterialPhysicalizeSetting_None,
	eMaterialPhysicalizeSetting_Default,
	eMaterialPhysicalizeSetting_Obstruct,
	eMaterialPhysicalizeSetting_NoCollide,
	eMaterialPhysicalizeSetting_ProxyOnly,
	EMaterialPhysicalizeSetting_COUNT
};

const char* MaterialPhysicalizeSettingToString(EMaterialPhysicalizeSetting ps);

class CScene
{
private:
	struct SNodeUserData
	{
		ENodeExportSetting           exportSetting;
		int                          lod;
		bool                         bIsProxy;
		std::vector<string>          userDefinedProperties;
		CNodeProperties              physicalProperties;
		CAutoLodSettings::sNodeParam autoLodProperties;

		SNodeUserData()
			: exportSetting(eNodeExportSetting_Include)
			, lod(0)
			, bIsProxy(false)
		{}

		bool operator==(const SNodeUserData& other) const
		{
			return
			  exportSetting == other.exportSetting &&
			  lod == other.lod &&
			  bIsProxy == other.bIsProxy &&
			  userDefinedProperties == other.userDefinedProperties &&
			  physicalProperties == other.physicalProperties &&
			  autoLodProperties == other.autoLodProperties;
		}
	};

	struct SMaterialUserData
	{
		EMaterialPhysicalizeSetting physicalizeSetting;

		// Non-negative values are valid sub-material indices. A value of -1 means this sub-material is
		// marked for deletion.
		int    subMaterialIndex;

		bool   bMarkedForIndexAutoAssignment;

		ColorF color;

		SMaterialUserData()
			: physicalizeSetting(eMaterialPhysicalizeSetting_None)
			, subMaterialIndex(0)
			, bMarkedForIndexAutoAssignment(true)
			, color(ColorF(1.0f, 1.0f, 1.0f))
		{}
	};
public:
	typedef SMaterial* MaterialHandle;
	typedef const SMaterial* ConstMaterialHandle;

	struct IObserver
	{
		virtual ~IObserver() {}

		virtual void OnNodeUserDataChanged(const SNode* pNode)             {}
		virtual void OnMaterialUserDataChanged() {}
	};

	class CBatchMaterialSignals : IObserver
	{
	public:
		CBatchMaterialSignals(CScene& scene)
			: m_scene(scene)
			, m_savedObservers(scene.m_observers)
			, m_bSignalNeeded(false)
		{
			m_scene.m_observers.clear();
			m_scene.m_observers.push_back(this);
		}

		~CBatchMaterialSignals()
		{
			m_scene.m_observers = m_savedObservers;

			if (m_bSignalNeeded)
			{
				for (auto& obs : m_scene.m_observers)
				{
					obs->OnMaterialUserDataChanged();
				}
			}
		}

		virtual void OnMaterialUserDataChanged() override
		{
			m_bSignalNeeded = true;
		}
	private:
		CScene& m_scene;
		std::vector<IObserver*> m_savedObservers;
		bool m_bSignalNeeded;
	};

	// Imports an FBX scene from a file.
	// The function will return nullptr if the file can't be loaded.
	static std::unique_ptr<CScene> ImportFile(const SFileImportDescriptor& desc);

	// Imports an FBX scene from a file with default settings.
	// The function will return nullptr if the file can't be loaded.
	static std::unique_ptr<CScene> ImportFile(const char* szPath)
	{
		SFileImportDescriptor desc;
		desc.filePath = szPath;
		return ImportFile(desc);
	}

	CScene(TFbxManagerPtr pManager, TFbxScenePtr pScene, const SFileImportDescriptor& fileImportDesc)
		: m_pManager(std::move(pManager))
		, m_pScene(std::move(pScene))
		, m_globalScale(1.0)
		, m_globalRotation(IDENTITY)
		, m_fileImportDesc(fileImportDesc)
		, m_fileScale(m_pScene->GetGlobalSettings().GetSystemUnit().GetScaleFactor())
		, m_pDummyMaterial(nullptr)
	{
		m_pRootNode = Initialize(m_fileImportDesc);
	}

	~CScene() {}

	SNode* GetRootNode()
	{
		return m_pRootNode;
	}

	// Get the root node of the scene.
	const SNode* GetRootNode() const
	{
		return m_pRootNode;
	}

	Axes::EAxis GetUpAxis();
	Axes::EAxis GetForwardAxis();

	double      GetFileUnitSizeInCm() const;

	// The SetGlobal* methods below only set the transformation of the root
	// node, and both the local transformations and vertex positions of all
	// other nodes remain unchanged.

	// Sets the scale multiplier for the scene.
	// 'scale' is usually a product of user-provided scale and unit conversion
	// multiplier.
	void SetGlobalScale(double scale);

	// Sets the rotation of the scene.
	void             SetGlobalRotation(const Matrix33d& rotation);

	int              GetNodeCount() const;
	int              GetMaterialCount() const;
	int              GetMeshCount() const;
	int GetAnimationTakeCount() const;

	const SNode* GetNodeByIndex(int index) const
	{
		return m_nodes[index].get();
	}

	const SAnimationTake* GetAnimationTakeByIndex(int index) const
	{
		return &m_animationTakes[index];
	}

	const SMaterial* GetMaterialByIndex(int index) const
	{
		assert(0 <= index && index < GetMaterialCount());
		return m_materials[index].get();
	}

	const SMaterial* GetMaterialByName(const char* szName) const
	{
		for (size_t i = 0; i < m_materials.size(); ++i)
		{
			if (!stricmp(m_materials[i]->szName, szName))
			{
				return m_materials[i].get();
			}
		}
		return nullptr;
	}

	bool                                           IsDummyMaterial(const SMaterial* pMaterial) const;

	const std::vector<std::unique_ptr<SMaterial>>& GetMaterials() const;
	const std::vector<std::unique_ptr<SMesh>>&     GetMeshes() const;

	static void                                    SetAssociatedNode(FbxNode* pNode, SNode* pAssociated)
	{
		assert(pNode && pAssociated);
		pNode->SetUserDataPtr(pAssociated);
	}

	static SNode* GetAssociatedNode(FbxNode* pNode)
	{
		assert(pNode);
		return static_cast<SNode*>(pNode->GetUserDataPtr());
	}

	SNode*    Initialize(const SFileImportDescriptor& desc);

	FbxScene* GetFbxScene()
	{
		return m_pScene.get();
	}

	const FbxScene* GetFbxScene() const
	{
		return m_pScene.get();
	}

	// Node user data.

	ENodeExportSetting GetNodeExportSetting(const SNode* pNode) const
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].exportSetting;
	}

	void SetNodeExportSetting(const SNode* pNode, ENodeExportSetting exportSetting)
	{
		assert(pNode);
		if (m_nodeUserData[pNode->id].exportSetting != exportSetting)
		{
			m_nodeUserData[pNode->id].exportSetting = exportSetting;

			for (auto& obs : m_observers)
			{
				obs->OnNodeUserDataChanged(pNode);
			}
		}
	}

	int GetNodeLod(const SNode* pNode) const
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].lod;
	}

	void SetNodeLod(const SNode* pNode, int lod);

	bool IsProxy(const SNode* pNode) const
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].bIsProxy;
	}

	void SetProxy(const SNode* pNode, bool bProxy);

	const std::vector<string>& GetUserDefinedProperties(const SNode* pNode) const
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].userDefinedProperties;
	}

	void SetUserDefinedProperties(const SNode* pNode, const std::vector<string>& properties)
	{
		assert(pNode);
		m_nodeUserData[pNode->id].userDefinedProperties = properties;
	}

	const CNodeProperties& GetPhysicalProperties(const SNode* pNode) const
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].physicalProperties;
	}

	const CAutoLodSettings::sNodeParam& GetAutoLodProperties(const SNode* pNode) const
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].autoLodProperties;
	}

	void SetAutoLodProperties(const SNode* pNode, const CAutoLodSettings::sNodeParam& properties)
	{
		assert(pNode);
		m_nodeUserData[pNode->id].autoLodProperties = properties;
	}

	CNodeProperties& GetPhysicalProperties(const SNode* pNode)
	{
		assert(pNode);
		return m_nodeUserData[pNode->id].physicalProperties;
	}

	void SetPhysicalProperties(const SNode* pNode, const CNodeProperties& properties)
	{
		assert(pNode);
		m_nodeUserData[pNode->id].physicalProperties = properties;
	}

	// A node is included if its export setting is "include" and none of its ancestors is excluded.
	bool IsNodeIncluded(const SNode* pNode) const
	{
		while (pNode)
		{
			if (GetNodeExportSetting(pNode) == eNodeExportSetting_Exclude)
			{
				return false;
			}
			pNode = pNode->pParent;
		}
		return true;
	}

	// Material user data.

	void SetMaterialPhysicalizeSetting(const SMaterial* pMaterial, EMaterialPhysicalizeSetting setting)
	{
		assert(pMaterial);
		if (m_materialUserData[pMaterial->id].physicalizeSetting != setting)
		{
			m_materialUserData[pMaterial->id].physicalizeSetting = setting;

			for (auto& obs : m_observers)
			{
				obs->OnMaterialUserDataChanged();
			}
		}
	}

	EMaterialPhysicalizeSetting GetMaterialPhysicalizeSetting(const SMaterial* pMaterial) const
	{
		assert(pMaterial);
		return m_materialUserData[pMaterial->id].physicalizeSetting;
	}

	void SetMaterialSubMaterialIndex(const SMaterial* pMaterial, int index)
	{
		assert(pMaterial);
		if (m_materialUserData[pMaterial->id].subMaterialIndex != index)
		{
			m_materialUserData[pMaterial->id].subMaterialIndex = index;

			for (auto& obs : m_observers)
			{
				obs->OnMaterialUserDataChanged();
			}
		}
	}

	int GetMaterialSubMaterialIndex(const SMaterial* pMaterial) const
	{
		assert(pMaterial);
		return m_materialUserData[pMaterial->id].subMaterialIndex;
	}

	bool IsMaterialMarkedForIndexAutoAssignment(const SMaterial* pMaterial) const
	{
		assert(pMaterial);
		return m_materialUserData[pMaterial->id].bMarkedForIndexAutoAssignment;
	}

	void MarkMaterialForIndexAutoAssignment(const SMaterial* pMaterial, bool bEnabled)
	{
		assert(pMaterial);
		m_materialUserData[pMaterial->id].bMarkedForIndexAutoAssignment = bEnabled;
	}

	const ColorF& GetMaterialColor(const SMaterial* pMaterial) const
	{
		assert(pMaterial);
		return m_materialUserData[pMaterial->id].color;
	}

	void SetMaterialColor(const SMaterial* pMaterial, const ColorF& color)
	{
		assert(pMaterial);
		m_materialUserData[pMaterial->id].color = color;
	}

	void AddObserver(IObserver* pObserver)
	{
		assert(pObserver);
		stl::push_back_unique(m_observers, pObserver);
	}

	void RemoveObserver(IObserver* pObserver)
	{
		stl::find_and_erase(m_observers, pObserver);
	}
private:
	SMaterial*       GetMaterialByName_Internal(const char* name);
private:
	SNode* NewNode();
	SMesh* NewMesh();
	void InitializeNode(SNode* pNode, FbxNode* pFbxNode);
	bool InitializeMesh(ICallbacks* pCallbacks, SMesh* pMesh, FbxNode* pFbxNode, SNode* pNode, FbxMesh* pFbxMesh, SCreateMeshStats* pStats);

	void InitializeAnimations();

	static std::unique_ptr<CScene> ImportFileInternal(SFileImportDescriptor desc);

	SFileImportDescriptor m_fileImportDesc;

	// Owned state from FBX
	const TFbxManagerPtr m_pManager;
	const TFbxScenePtr   m_pScene;

	double               m_fileScale;

	void UpdateRootTransformation();

	SNode*    m_pRootNode;

	double    m_globalScale;
	Matrix33d m_globalRotation;

	// Allocated state
	std::vector<std::unique_ptr<SMesh>>     m_meshes;
	std::vector<std::unique_ptr<SNode>>     m_nodes;
	std::vector<SNodeUserData>              m_nodeUserData;     // Indexed by node ID.
	std::vector<SMaterialUserData>          m_materialUserData; // Indexed by material ID.
	std::vector<std::unique_ptr<SMaterial>> m_materials;
	std::vector<SAnimationTake> m_animationTakes;
	SMaterial* m_pDummyMaterial;

	std::vector<IObserver*>                 m_observers;
};
}

struct SCreateEngineMeshStats
{
	float elapsedTotal;
	float elapsedCreateDirectMesh;
	float elapsedTranslateMaterialIndices;
	float elapsedCreateMesh;

	struct STranslateMaterialIndicesStats
	{
		int nSearches;
		int nLookups;

		STranslateMaterialIndicesStats()
			: nSearches(0)
			, nLookups(0)
		{}
	};
	STranslateMaterialIndicesStats translateMaterialIndices;

	struct SCreateMeshStats
	{
		float elapsedSortVertices;
		float elapsedMakeUniqueVertices;
		float elapsedSortTriangles;
		float elapsedCollectSubsets;
		float elapsedProcessSubsets;
		float elapsedCreateCMesh;

		SCreateMeshStats()
			: elapsedSortVertices(0.0f)
			, elapsedMakeUniqueVertices(0.0f)
			, elapsedSortTriangles(0.0f)
			, elapsedCollectSubsets(0.0f)
			, elapsedProcessSubsets(0.0f)
			, elapsedCreateCMesh(0.0f)
		{}
	};
	SCreateMeshStats createMesh;

	SCreateEngineMeshStats()
		: elapsedTotal(0.0f)
		, elapsedCreateDirectMesh(0.0f)
		, elapsedTranslateMaterialIndices(0.0f)
		, elapsedCreateMesh(0.0f)
	{}

	static void Log(const SCreateEngineMeshStats& stats);
};

struct SCreateMeshStats
{
	float                  elapsedTotal;
	float                  elapsedSearchMaterials;
	SCreateEngineMeshStats createEngineMesh;

	SCreateMeshStats()
		: elapsedTotal(0.0f)
		, elapsedSearchMaterials(0.0f)
		, createEngineMesh()
	{}

	static void Log(const SCreateMeshStats& stats);
};

// Returns nullptr on success, and error string otherwise.
const char* CreateEngineMesh(
  const FbxMesh& inputMesh,
  std::vector<std::unique_ptr<FbxTool::SDisplayMesh>>& outputMeshes,
  const FbxNode* pNode,
  const std::vector<std::unique_ptr<FbxTool::SMaterial>>& materials,
  SCreateEngineMeshStats* pStats = nullptr);

// TODO: Maybe put this somewhere else.
string GetFormattedAxes(FbxTool::Axes::EAxis up, FbxTool::Axes::EAxis fwd);

