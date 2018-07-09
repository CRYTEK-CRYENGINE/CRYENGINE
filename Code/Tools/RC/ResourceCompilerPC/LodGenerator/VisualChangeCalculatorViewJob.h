// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace ThreadUtils
{
	class StealingThreadPool;
}
namespace LODGenerator 
{
	class VisualChangeCalculatorViewJob
	{
	public:

		VisualChangeCalculatorViewJob()
		{
			m_view=NULL;
		}

		~VisualChangeCalculatorViewJob()
		{

		}

		// Start accepting work on thread
// 		virtual void ThreadEntry()
// 		{
// 			while (true)
// 			{
// 				m_newTask.Wait();
// 				m_newTask.Reset();
// 				switch (currentTask)
// 				{
// 				case TASK_CREATEVIEW:
// 					m_view=new VisualChangeCalculatorView(*taskData.createView.moves, *taskData.createView.polys, *taskData.createView.vertices, *taskData.createView.viewDirection, taskData.createView.metersPerPixel, taskData.createView.silhouetteWeight);
// 					break;
// 				case TASK_CALCULATEERROR:
// 					m_view->CalculateError();
// 					break;
// 				case TASK_UPDATESPANS:
// 					m_view->UpdateSpans(taskData.updateSpans.phase, *taskData.updateSpans.rerenderPolygons, taskData.updateSpans.expectedError);
// 					break;
// 				case TASK_QUIT:
// 					delete m_view;
// 					m_view=NULL;
// 					m_done.Set();
// 					return;
// 				}
// 				m_done.Set();
// 			}
// 		}

		static void TaskCreateView(VisualChangeCalculatorViewJob* cCView)
		{
			cCView->m_view=new VisualChangeCalculatorView(*cCView->taskData.createView.moves, *cCView->taskData.createView.polys, *cCView->taskData.createView.vertices, *cCView->taskData.createView.viewDirection, cCView->taskData.createView.metersPerPixel, cCView->taskData.createView.silhouetteWeight);
		}

		static void TaskCalculateerror(VisualChangeCalculatorViewJob* cCView)
		{
			cCView->m_view->CalculateError();
		}

		static void TaskUpdateSpans(VisualChangeCalculatorViewJob* cCView)
		{
			cCView->m_view->UpdateSpans(cCView->taskData.updateSpans.phase, *cCView->taskData.updateSpans.rerenderPolygons, cCView->taskData.updateSpans.expectedError);
		}

		static void TaskQuit(VisualChangeCalculatorViewJob* cCView)
		{
			delete cCView->m_view;
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
		}

		void CalculateError()
		{
			currentTask=TASK_CALCULATEERROR;
		}

		void UpdateSpans(int phase, const std::set<std::pair<idT,idT> > &rerenderPolygons, float expectedError)
		{
			currentTask=TASK_UPDATESPANS;
			taskData.updateSpans.phase=phase;
			taskData.updateSpans.rerenderPolygons=&rerenderPolygons;
			taskData.updateSpans.expectedError=expectedError;
		}

		float GetError(int idx)
		{
			return m_view->m_error[idx];
		}

		void ZeroErrorValues()
		{
			memset(&m_view->m_error[0], 0, sizeof(m_view->m_error[0])*m_view->m_error.size());
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
	};
}
