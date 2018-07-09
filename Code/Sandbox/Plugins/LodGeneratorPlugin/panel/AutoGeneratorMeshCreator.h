#pragma once

namespace LODGenerator 
{
	class CAutoGeneratorMeshCreator
	{
	public:
		CAutoGeneratorMeshCreator();
		~CAutoGeneratorMeshCreator();

	public:
		bool Generate(const int nLodId);
		void SetPercentage(float fPercentage);

	private:
		bool PrepareMaterial(const string& materialName, bool bAddToMultiMaterial);
		int CreateLODMaterial(const string& submatName);
		int FindExistingLODMaterial(const string& matName);

	private:
		float m_fPercentage;
	};
}