#pragma once
#include "Util/LODGeneratorLib.h"
#include "AutoGeneratorParams.h"

namespace LODGenerator 
{
	class CAutoGeneratorDataBase
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
		CAutoGeneratorDataBase();
		~CAutoGeneratorDataBase();

	public:
		static CAutoGeneratorDataBase* Instance();
		static void DestroyInstance();

		bool LoadStatObj(const string& filepath);
		void ReloadModel();
		IStatObj* GetLoadedModel(int nLod = 0);

		bool LoadMaterial(string strMaterial);
		void ReloadMaterial();
		CMaterial* GetLoadedMaterial();

		void SetLODMaterial(CMaterial *pMat);
		CMaterial* GetLODMaterial();
		
		CAutoGeneratorParams& GetParams();

		CLODGeneratorLib::SLODSequenceGenerationOutput& GetBakeMeshResult();

		void ClearAllTextureResults();
		void ClearTextureResults(int level);

		int GetSubMatId(const int nLodId);
		float GetRadius(int nLod);

		std::map<int,SMeshBakingOutput>& GetBakeTextureResult();

	private:
		int NumberOfLods();

	private:
		static CAutoGeneratorDataBase * m_pInstance;

	private:
		//-------------------------------------------------------------------------------------------
		// input mesh
		_smart_ptr<IStatObj> m_pLoadedStatObj;
		// input material
		_smart_ptr<CMaterial> m_pLoadedMaterial;
		_smart_ptr<CMaterial> m_pLODMaterial;

		//-------------------------------------------------------------------------------------------
		// output texture 
		std::map<int,SMeshBakingOutput> m_resultsMap;
		// output mesh 
		CLODGeneratorLib::SLODSequenceGenerationOutput m_results;

		//-------------------------------------------------------------------------------------------
		// parameters
		CAutoGeneratorParams m_autoGeneratorParams;
	};
}