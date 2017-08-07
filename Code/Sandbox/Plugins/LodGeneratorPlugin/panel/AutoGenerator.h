#pragma once

#include "AutoGeneratorPreparation.h"
#include "AutoGeneratorMeshCreator.h"
#include "AutoGeneratorTextureCreator.h"

namespace LODGenerator 
{
	class CAutoGenerator
	{
		struct SLodInfo
		{
			int lod;
			float percentage;
			int width;
			int height;
		};

	public:
		CAutoGenerator();
		~CAutoGenerator();

	public:
		bool Init(string meshPath, string materialPath);
		bool Prepare();
		bool CreateLod(float percent,int width,int height);
		bool GenerateLodMesh(int level);
		bool GenerateAllLodMesh();
		bool GenerateLodTexture(int level);
		bool GenerateAllLodTexture();
		bool GenerateLod(int level);
		bool GenerateAllLod();
		float Tick();

		bool IsPrepareDone();
	private:
		std::vector<SLodInfo> m_LodInfoList;
		bool m_PrepareDone;

		CAutoGeneratorPreparation m_AutoGeneratorPreparation;
		CAutoGeneratorMeshCreator m_AutoGeneratorMeshCreator;
		CAutoGeneratorTextureCreator m_AutoGeneratorTextureCreator;
	};

}