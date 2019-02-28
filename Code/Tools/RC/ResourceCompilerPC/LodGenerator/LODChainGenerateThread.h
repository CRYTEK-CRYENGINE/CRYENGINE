// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VisualChangeCalculator.h"
namespace LODGenerator 
{
	class LODChainGenerateThread : public IThread
	{
	public:

		LODChainGenerateThread()
		{
			m_bDone=false;
			m_done.Reset();
			m_newTask.Reset();
			ResetProgress();

			if (!gEnv->pThreadManager->SpawnThread(this, "LODChainGenerateThread"))
			{
				CryFatalError("Error spawning \"LODChainGenerateThread\" thread.");
			}
		}

		~LODChainGenerateThread()
		{
			SignalStopWork();
			gEnv->pThreadManager->JoinThread(this, eJM_Join);
		}

		// Signals the thread that it should not accept anymore work and exit
		void SignalStopWork()
		{
			currentTask=TASK_QUIT;
			m_newTask.Set();
		}

		void Cancel()
		{
			m_progress.eStatus = ESTATUS_CANCEL;
			m_progress.fProgress = 0.0f;
			currentTask=TASK_QUIT;
			m_newTask.Set();
		}

		void WaitForCompletion()
		{
			m_done.Wait();
			m_done.Reset();
		}

		void CreateLodders(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues)
		{
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

		bool ProcessLodder(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutputList *pReturnValues)
		{
			if ( m_progress.eStatus == ESTATUS_RUNNING )
			{
				CreateLodders(pInputParams,pReturnValues);
				m_bDone=false;
				currentTask=TASK_GENERATELODCHAIN;
				m_newTask.Set();
				return true;
			}
			return false;
		}

		bool CheckStatus(float *pProgress)
		{
			if (pProgress)
				*pProgress=m_progress.fProgress;
			return m_bDone;
		}

		void ResetProgress()
		{
			m_progress.eStatus = ESTATUS_RUNNING;
			m_progress.fProgress = 0.0f;
			m_progress.fCompleted = 0;
			m_progress.fParts = 0;
		}

	private:
		enum ETask
		{
			TASK_GENERATELODCHAIN,
			TASK_QUIT
		};

		// Start accepting work on thread
		virtual void ThreadEntry()
		{
			while (true)
			{
				m_newTask.Wait();
				m_newTask.Reset();
				switch (currentTask)
				{
				case TASK_GENERATELODCHAIN:

					if ( m_progress.eStatus == ESTATUS_CANCEL )
						break;

					for(std::vector<VisualChangeCalculator *>::iterator lodder = m_Lodders.begin(), end = m_Lodders.end(); lodder != end; ++ lodder)
					{
						VisualChangeCalculator * pLodder = (*lodder);
						pLodder->InitialRender(&m_progress);

						if ( m_progress.eStatus == ESTATUS_CANCEL )
							break;

						pLodder->Process(&m_progress);

						if ( m_progress.eStatus == ESTATUS_CANCEL )
							break;

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
							if ( m_progress.eStatus == ESTATUS_CANCEL )
								break;

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
							if ( m_progress.eStatus == ESTATUS_CANCEL )
								break;

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
							if ( m_progress.eStatus == ESTATUS_CANCEL )
								break;

							pLodder->m_pReturnValues->moveList[i].from=pLodder->m_moveList[i].from;
							pLodder->m_pReturnValues->moveList[i].to=pLodder->m_moveList[i].to;
							pLodder->m_pReturnValues->moveList[i].error=pLodder->m_moveList[i].error;
						}
						m_progress.fCompleted++;
					}
					m_bDone=true;
					break;
				case TASK_QUIT:
					m_done.Set();
					return;
				}
				m_done.Set();
			}
		}

		volatile sProgress m_progress;
		volatile bool m_bDone;
		ETask currentTask;
		CryEvent m_newTask;
		CryEvent m_done;
	public:
		std::vector<VisualChangeCalculator *> m_Lodders;
	};
}


