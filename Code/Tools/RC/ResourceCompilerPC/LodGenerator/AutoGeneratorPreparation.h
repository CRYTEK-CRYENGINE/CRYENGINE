// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace LODGenerator 
{
	class CAutoGenerator;
	class CAutoGeneratorPreparation
	{
	public:
		CAutoGeneratorPreparation(CAutoGenerator* pAutoGenerator);
		~CAutoGeneratorPreparation();

	public:
//		bool Tick(float* fProgress);
		bool Generate();

	private:
		//bool m_bAwaitingResults;
		CAutoGenerator* m_AutoGenerator;
	};
}