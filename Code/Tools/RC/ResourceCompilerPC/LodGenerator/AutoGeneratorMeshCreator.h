// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <string>

namespace LODGenerator 
{
	class CAutoGenerator;
	class CAutoGeneratorMeshCreator
	{
	public:
		CAutoGeneratorMeshCreator(CAutoGenerator* pAutoGenerator);
		~CAutoGeneratorMeshCreator();

	public:
		bool Generate(const int index,const int nLodId);
		void SetPercentage(float fPercentage);

	private:
		bool PrepareMaterial(const int index);
		int CreateLODMaterial(const int index,int iLod);

	private:
		float m_fPercentage;
		CAutoGenerator* m_AutoGenerator;
	};
}