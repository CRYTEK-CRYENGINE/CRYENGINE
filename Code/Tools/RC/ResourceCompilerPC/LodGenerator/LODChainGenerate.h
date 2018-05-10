// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AutoGeneratorLib.h"

namespace LODGenerator 
{
	class LODChainGenerate
	{
	public:

		LODChainGenerate()
		{
		}

		~LODChainGenerate()
		{

		}

		bool ProcessLodder(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues)
		{		
			CreateLodders(pInputParams,pReturnValues);
			HandleLodder();		
			return true;
		}

	private:
		void CreateLodders(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues)
		{
			RCLog("Create Lodders");

			int nodeCout = pInputParams->nodeList.size();
			for(int i = 0; i<nodeCout; ++i)
			{
				CLODGeneratorLib::SLODSequenceGenerationOutput * pSubReturnValues = new CLODGeneratorLib::SLODSequenceGenerationOutput();
				if (!pSubReturnValues)
				{
					CryLog("LODGen: Failed to allocate SubObjects Generation Output, possible out of memory problem");
					return;
				}

				pSubReturnValues->node = pInputParams->nodeList[i];
				pSubReturnValues->indices=NULL;
				pSubReturnValues->positions=NULL;
				pSubReturnValues->moveList=NULL;
				pSubReturnValues->numIndices=0;
				pSubReturnValues->numMoves=0;
				pSubReturnValues->numPositions=0;
				pReturnValues->m_pSubObjectOutput.push_back(pSubReturnValues);

				VisualChangeCalculator * pLodder=new VisualChangeCalculator();
				if ( !pLodder )
				{
					CryLog("LODGen: Failed to allocate visual change calculator, possible out of memory problem");
					return;
				}

				pLodder->SetParameters(pInputParams);
				pLodder->FillData(pSubReturnValues);
				m_Lodders.push_back(pLodder);

				m_progress.fParts++;
			}
		}

		void HandleLodder()
		{
			RCLog("Handle Lodder");

			int index = 0;
			int iAllLodderCount = m_Lodders.size();
			for(std::vector<VisualChangeCalculator *>::iterator lodder = m_Lodders.begin(), end = m_Lodders.end(); lodder != end; ++ lodder,index++)
			{
				RCLog("Handle Lodder: %d/%d",index,iAllLodderCount);

				VisualChangeCalculator * pLodder = (*lodder);
				RCLog("Initial Render");
				pLodder->InitialRender(&m_progress);
				RCLog("Process Lodder");
				pLodder->Process(&m_progress);

				pLodder->m_pReturnValues->numPositions=pLodder->m_vertices.size();
				pLodder->m_pReturnValues->positions=new Vec3[pLodder->m_pReturnValues->numPositions];
				if (!pLodder->m_pReturnValues->positions)
				{
					CryLog("LODGen: Failed to allocate numPositions '%d' while generating lod chain, possible out of memory problem", pLodder->m_pReturnValues->numPositions);
					pLodder->m_pReturnValues->numPositions = 0;
					continue;
				}

				for (int i=0; i<pLodder->m_pReturnValues->numPositions; i++)
				{
					pLodder->m_pReturnValues->positions[i]=pLodder->m_vertices[i].pos;
				}

				pLodder->m_pReturnValues->numIndices=3*pLodder->m_originalTris.size();
				pLodder->m_pReturnValues->indices=new vtx_idx[pLodder->m_pReturnValues->numIndices];
				if (!pLodder->m_pReturnValues->indices)
				{
					CryLog("LODGen: Failed to allocate numIndices '%d' while generating lod chain, possible out of memory problem", pLodder->m_pReturnValues->numIndices);
					pLodder->m_pReturnValues->numIndices = 0;
					continue;
				}

				for (int i=0; i<pLodder->m_originalTris.size(); i++)
				{
					pLodder->m_pReturnValues->indices[3*i+0]=pLodder->m_originalTris[i].v[0];
					pLodder->m_pReturnValues->indices[3*i+1]=pLodder->m_originalTris[i].v[1];
					pLodder->m_pReturnValues->indices[3*i+2]=pLodder->m_originalTris[i].v[2];
				}

				pLodder->m_pReturnValues->numMoves=pLodder->m_moveList.size();
				pLodder->m_pReturnValues->moveList=new CLODGeneratorLib::SLODSequenceGenerationOutput::SMove[pLodder->m_pReturnValues->numMoves];
				if (!pLodder->m_pReturnValues->moveList)
				{
					CryLog("LODGen: Failed to allocate numMoves '%d' while generating lod chain, possible out of memory problem", pLodder->m_pReturnValues->numMoves);
					pLodder->m_pReturnValues->numMoves = 0;
					continue;
				}

				for (int i=0; i<pLodder->m_pReturnValues->numMoves; i++)
				{
					pLodder->m_pReturnValues->moveList[i].from=pLodder->m_moveList[i].from;
					pLodder->m_pReturnValues->moveList[i].to=pLodder->m_moveList[i].to;
					pLodder->m_pReturnValues->moveList[i].error=pLodder->m_moveList[i].error;
				}
				m_progress.fCompleted++;
			}

		}

		volatile sProgress m_progress;
	public:
		std::vector<VisualChangeCalculator *> m_Lodders;
	};
}
