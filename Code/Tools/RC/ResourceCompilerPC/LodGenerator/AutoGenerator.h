// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AutoGeneratorPreparation.h"
#include "AutoGeneratorMeshCreator.h"
#include "AutoGeneratorDataBase.h"

class CMesh;
struct CMaterialCGF;
struct CNodeCGF;
class CContentCGF;
class CAutoLodSettings;
class CImportRequest;
namespace LODGenerator 
{
	class CAutoGenerator
	{
		struct SLodInfo
		{
			int lod;
			float percentage;
		};

	public:
		CAutoGenerator();
		~CAutoGenerator();

	public:
		// init
		bool Init(CContentCGF& cgf,const CImportRequest& ir);
		// prepare
		bool Prepare();
		// create lod info
		//bool CreateLod(float percent);
		// create mesh
		//int GetLodCount();
		bool GenerateLodMesh(int index,int level);
		bool GenerateAllLodMesh();

		bool GetLodMesh(int index,int level, std::vector<CMesh*>** ppLodMesh);

		// create auto lod
		bool GenerateLod(int index,int level);
		bool GenerateAllLod();

		bool IsPrepareDone();

		CAutoGeneratorDataBase& GetDataBase()
		{
			return m_AutoGeneratorDataBase;
		}

		bool AutoCreateNodes();

	private:
		CContentCGF* m_ContentCGF;
		const CAutoLodSettings* m_AutoLodSettings;

		std::map<CNodeCGF*,std::vector<SLodInfo>> m_NodeLodInfoList;
		bool m_PrepareDone;

		CAutoGeneratorPreparation* m_AutoGeneratorPreparation;
		CAutoGeneratorMeshCreator* m_AutoGeneratorMeshCreator;
		CAutoGeneratorDataBase m_AutoGeneratorDataBase;

		std::string m_ObjName;
	};

}