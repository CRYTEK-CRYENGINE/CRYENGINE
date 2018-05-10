// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VisualChangeCalculatorView.h"

namespace LODGenerator
{
	class VisualChangeCalculatorViewThread : public IThread
	{
	public:

		VisualChangeCalculatorViewThread()
		{
			m_view=NULL;
			m_done.Reset();
			m_newTask.Reset();

			if (!gEnv->pThreadManager->SpawnThread(this, "VisualChangeCalculatorView"))
			{
				CryFatalError("Error spawning \"VisualChangeCalculatorView\" thread.");
			}
		}

		~VisualChangeCalculatorViewThread()
		{
			SignalStopWork();
			gEnv->pThreadManager->JoinThread(this, eJM_Join);
		}

		// Start accepting work on thread
		virtual void ThreadEntry()
		{
			while (true)
			{
				m_newTask.Wait();
				m_newTask.Reset();
				switch (currentTask)
				{
				case TASK_CREATEVIEW:
					m_view=new VisualChangeCalculatorView(*taskData.createView.moves, *taskData.createView.polys, *taskData.createView.vertices, *taskData.createView.viewDirection, taskData.createView.metersPerPixel, taskData.createView.silhouetteWeight);
					break;
				case TASK_CALCULATEERROR:
					m_view->CalculateError();
					break;
				case TASK_UPDATESPANS:
					m_view->UpdateSpans(taskData.updateSpans.phase, *taskData.updateSpans.rerenderPolygons, taskData.updateSpans.expectedError);
					break;
				case TASK_QUIT:
					delete m_view;
					m_view=NULL;
					m_done.Set();
					return;
				}
				m_done.Set();
			}
		}

		// Signals the thread that it should not accept anymore work and exit
		void SignalStopWork()
		{
			currentTask=TASK_QUIT;
			m_newTask.Set();
		}

		void CreateView(const std::vector<Move> &moves, const std::vector<Poly> &polys, const std::vector<Vertex> &vertices, Vec3 &viewDirection, float metersPerPixel, float silhouetteWeight)
		{
			currentTask=TASK_CREATEVIEW;
			taskData.createView.moves=&moves;
			taskData.createView.polys=&polys;
			taskData.createView.vertices=&vertices;
			taskData.createView.viewDirection=&viewDirection;
			taskData.createView.metersPerPixel=metersPerPixel;
			taskData.createView.silhouetteWeight=silhouetteWeight;
			m_newTask.Set();
		}

		void CalculateError()
		{
			currentTask=TASK_CALCULATEERROR;
			m_newTask.Set();
		}

		void UpdateSpans(int phase, const std::set<std::pair<idT,idT> > &rerenderPolygons, float expectedError)
		{
			currentTask=TASK_UPDATESPANS;
			taskData.updateSpans.phase=phase;
			taskData.updateSpans.rerenderPolygons=&rerenderPolygons;
			taskData.updateSpans.expectedError=expectedError;
			m_newTask.Set();
		}

		float GetError(int idx)
		{
			return m_view->m_error[idx];
		}

		void ZeroErrorValues()
		{
			memset(&m_view->m_error[0], 0, sizeof(m_view->m_error[0])*m_view->m_error.size());
		}

		void WaitForCompletion()
		{
			m_done.Wait();
			m_done.Reset();
		}

		enum ETask
		{
			TASK_CREATEVIEW,
			TASK_CALCULATEERROR,
			TASK_UPDATESPANS,
			TASK_QUIT
		};

		ETask currentTask;
		union
		{
			struct 
			{
				const std::vector<Move> *moves;
				const std::vector<Poly> *polys;
				const std::vector<Vertex> *vertices;
				Vec3 *viewDirection;
				float metersPerPixel;
				float silhouetteWeight;
			} createView;
			struct  
			{
				int phase;
				const std::set<std::pair<idT,idT> > *rerenderPolygons;
				float expectedError;
			} updateSpans;
		} taskData;

		VisualChangeCalculatorView *m_view;
		CryEvent m_newTask;
		CryEvent m_done;
	};
}

