/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2013.
*************************************************************************/

#pragma once

//TODOJM: fix this
class CCMeshBakerPopupPreview;
class CCRampControl;

#include "Util/LODGeneratorLib.h"

namespace LODGenerator 
{
	class CLodGeneratorInteractionManager
	{
	public:
		enum eTextureType
		{
			eTextureType_Diffuse = 0,
			eTextureType_Normal = 1,
			eTextureType_Spec = 2,
			eTextureType_Max = 3,
		};

	private:
		CLodGeneratorInteractionManager();
		virtual ~CLodGeneratorInteractionManager();

		static CLodGeneratorInteractionManager * m_pInstance;

	public:
		static CLodGeneratorInteractionManager* Instance();
		static void DestroyInstance();

		const string GetSelectedBrushFilepath();
		const string GetSelectedBrushMaterial();
		const string GetDefaultBrushMaterial(const string& filepath);

		const string GetParameterFilePath() { return m_parameterFilePath; };
		void SetParameterFilePath(const string& filePath) { m_parameterFilePath = filePath; };

		void OpenMaterialEditor();

		bool LoadStatObj(const string& filepath);
		bool LoadMaterial(string strMaterial);
		CMaterial* LoadSpecificMaterial(string strMaterial);

		IStatObj* GetLoadedModel(int nLod = 0);
		CMaterial* GetLoadedMaterial();
		CMaterial* GetLODMaterial();
		void SetLODMaterial(CMaterial *pMat);
		const string GetLoadedFilename();
		const string GetLoadedMaterialFilename();
		void ReloadModel();
		void ReloadMaterial();

		const IStatObj::SStatistics GetLoadedStatistics();
		float GetRadius(const int nSourceLod);
		int NumberOfLods();
		int GetSubMatId(const int nLodId);
		int GetSubMatCount(const int nLodId);
		int GetHighestLod();
		int GetMaxNumLods();
		float GetLodsPercentage(const int nLodIdx);
		int GetNumMoves();
		float GetErrorAtMove(const int nIndex);

		bool LodGenGenerate();
		bool LodGenGenerateTick(float* fProgress);
		void LogGenCancel();

		IStatObj* TryLoadCageObject(const int nLodId);
		bool UsingLodCage(const int nLodId);
		IStatObj* CreateLODStatObj(const int nLodId,const float fPercentage);

		bool GenerateTemporaryLod(const float fPercentage, CCMeshBakerPopupPreview* ctrl, CCRampControl* ramp);
		void GenerateLod(const int nLodId, const float fPercentage);
		void ClearUnusedLods(const int nNewLods);
		bool GenerateLodFromCageMesh(IStatObj **pLodStatObj);

		bool RunProcess(const int nLod, int nWidth, const int nHeight);
		void SaveTextures(const int nLod);
		string GetPreset(const int texturetype);
		const SMeshBakingOutput* GetResults(const int nLod);
		void ClearResults();

		void SaveSettings();
		void LoadSettings();
		XmlNodeRef GetSettings(const string& settings);
		void ResetSettings();

		template <class T> 
		T GetGeometryOption(string paramName)
		{
			T value;
			if (IVariable* pVar = m_pGeometryVarBlock->FindVariable(paramName))
				pVar->Get(value);
			return value;
		}

		template <class T> 
		void SetGeometryOption(string paramName, T paramValue)
		{
			if (IVariable* pVar = m_pGeometryVarBlock->FindVariable(paramName))
				pVar->Set(paramValue);
		}

		template <class T> 
		T GetMaterialOption(string paramName)
		{
			T value;
			if (IVariable* pVar = m_pMaterialVarBlock->FindVariable(paramName))
				pVar->Get(value);
			return value;
		}

		template <class T> 
		void SetMaterialOption(string paramName, T paramValue)
		{
			if (IVariable* pVar = m_pMaterialVarBlock->FindVariable(paramName))
				pVar->Set(paramValue);
		}

		string ExportPath();
		void OpenExportPath();
		void ShowLodInExplorer(const int nLodId);
		void UpdateFilename(string filename);

		bool CreateChangelist();
		bool CheckoutInSourceControl(const string& filename);
		bool CheckoutOrExtract(const char * filename);

		CVarBlock* GetGeometryVarBlock() { return m_pGeometryVarBlock; }
		CVarBlock* GetMaterialVarBlock() { return m_pMaterialVarBlock; }

	protected:

		bool PrepareMaterial(const string& materialName, bool bAddToMultiMaterial);
		int FindExistingLODMaterial(const string& matName);
		CMaterial * CreateMaterialFromTemplate(const string& matName);
		int CreateLODMaterial(const string& submatName);
		void SetLODMaterialId(IStatObj * pStatObj, const int matId);

		string GetDefaultTextureName(const int i, const int nLod, const bool bAlpha, const string& exportPath, const string& fileNameTemplate);
		bool SaveTexture(ITexture* pTex, const bool bAlpha, const string& fileName, const string& texturePreset);
		void AssignToMaterial(const int nLod, const bool bAlpha, const bool bSaveSpec, const string& exportPath, const string& fileNameTemplate);
		bool VerifyNoOverlap(const int nSubMatIdx);
		bool RasteriseTriangle(Vec2 *tri, byte *buffer, const int width, const int height);

		void CreateSettings();

	private:

		_smart_ptr<IStatObj> m_pLoadedStatObj;
		_smart_ptr<CMaterial> m_pLoadedMaterial;
		_smart_ptr<CMaterial> m_pLODMaterial;

		std::map<int,SMeshBakingOutput> m_resultsMap;
		CLODGeneratorLib::SLODSequenceGenerationOutput m_results;
		string m_parameterFilePath;
		bool m_bAwaitingResults;
		string m_changeId;
		XmlNodeRef m_xmlSettings;

		_smart_ptr<CVarBlock>		m_pGeometryVarBlock;
		CSmartVariable<int>			m_nSourceLod;
		CSmartVariable<bool>		m_bObjectHasBase;
		CSmartVariable<int>			m_nViewsAround;
		CSmartVariable<int>			m_nViewElevations;
		CSmartVariable<float>		m_fSilhouetteWeight;
		CSmartVariable<float>		m_fViewResolution;
		CSmartVariable<float>		m_fVertexWelding;
		CSmartVariable<bool>		m_bCheckTopology;
		CSmartVariable<bool>		m_bWireframe;
		CSmartVariable<bool>		m_bExportObj;
		CSmartVariable<bool>		m_bAddToParentMaterial;
		CSmartVariable<bool>		m_bUseCageMesh;
		CSmartVariable<bool>		m_bPreviewSourceLod;

		_smart_ptr<CVarBlock>		m_pMaterialVarBlock;
		CSmartVariable<float>		m_fRayLength;
		CSmartVariable<float>		m_fRayStartDist;
		CSmartVariable<bool>		m_bBakeAlpha;
		CSmartVariable<bool>		m_bBakeSpec;
		CSmartVariable<bool>		m_bSmoothCage;
		CSmartVariable<bool>		m_bDilationPass;
		CSmartVariable<Vec3>		m_cBackgroundColour;
		CSmartVariable<string>		m_strExportPath;
		CSmartVariable<string>		m_strFilename;
		CSmartVariable<string>		m_strPresetDiffuse;
		CSmartVariable<string>		m_strPresetNormal;
		CSmartVariable<string>		m_strPresetSpecular;
		CSmartVariable<bool>		m_useAutoTextureSize;
		CSmartVariable<float>		m_autoTextureRadius1;
		CSmartVariable<int>			m_autoTextureSize1;
		CSmartVariable<float>		m_autoTextureRadius2;
		CSmartVariable<int>			m_autoTextureSize2;
		CSmartVariable<float>		m_autoTextureRadius3;
		CSmartVariable<int>			m_autoTextureSize3;
	};
}
