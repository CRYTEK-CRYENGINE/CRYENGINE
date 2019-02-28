// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VisualChangeCalculatorView.h"
#include "VisualChangeCalculatorViewJob.h"
namespace LODGenerator
{
	class VisualChangeCalculator
	{
	public:
		struct TakenMove 
		{
			TakenMove(idT _from, idT _to, float _error)
			{
				from=_from;
				to=_to;
				error=_error;
			}
			idT from;
			idT to;
			float error;
		};

		struct Tri
		{
			Tri(int other[3])
			{
				memcpy(v, other, sizeof(v));
			}
			int v[3];
		};

		CLODGeneratorLib::SLODSequenceGenerationOutput * m_pReturnValues;

		std::vector<Move> m_moves;
		std::vector<Poly> m_polys;
		std::vector<Vertex> m_vertices;
		std::vector<Vec3> m_noMoveList;

		std::vector<Tri> m_originalTris;
		std::vector<TakenMove> m_moveList;

		int m_numViews;
		VisualChangeCalculatorViewJob *m_views;
		ThreadUtils::StealingThreadPool* m_threadPool;

		float m_metersPerPixel;
		float m_silhouetteWeight;
		float m_weldDistance;
		bool m_bModelHasBase;
		int m_numElevations;
		int m_numAngles;
		bool m_bCheckTopology;

	public:
		VisualChangeCalculator()
		{
			m_pReturnValues = NULL;
			m_metersPerPixel=0.04f;
			m_bModelHasBase=false;
			m_numElevations=3;
			m_numAngles=8;
			m_numViews=0;
			m_silhouetteWeight=49;
			m_weldDistance=0.001f;
			m_bCheckTopology=true;
			m_views=NULL;

			m_threadPool = new ThreadUtils::StealingThreadPool(12, false);
			m_threadPool->Start();
		}

		~VisualChangeCalculator()
		{
			m_threadPool->WaitAllJobsTemporary();
			delete[] m_views;
			delete m_threadPool;
		}

		// Checks if vertex has any "flaps" (more than two faces share an edge) or is a "bow tie" (a single vertex connects two faces)
		bool IsPatchNice(const Vertex &v, int vertId, int other=-1)
		{
			int ends=0;
			for (std::vector<idT>::const_iterator it=v.polys.begin(), end=v.polys.end(); it!=end; ++it)
			{
				const Poly &p=m_polys[*it];
				int ids[3];
				ids[0]=(p.v[0]==other)?vertId:p.v[0];
				ids[1]=(p.v[1]==other)?vertId:p.v[1];
				ids[2]=(p.v[2]==other)?vertId:p.v[2];
				if (ids[0]!=ids[1] && ids[0]!=ids[2] && ids[1]!=ids[2])
				{
					for (int i=0; i<3; i++)
					{
						if (ids[i]!=vertId)
						{
							int count=0;
							for (std::vector<idT>::const_iterator it2=v.polys.begin(); it2!=end; ++it2)
							{
								if (it!=it2)
								{
									const Poly &p2=m_polys[*it2];
									int ids2[3];
									ids2[0]=(p2.v[0]==other)?vertId:p2.v[0];
									ids2[1]=(p2.v[1]==other)?vertId:p2.v[1];
									ids2[2]=(p2.v[2]==other)?vertId:p2.v[2];
									if (ids2[0]!=ids2[1] && ids2[0]!=ids2[2] && ids2[1]!=ids2[2])
									{
										if (ids2[0]==ids[i] || ids2[1]==ids[i] || ids2[2]==ids[i])
											count++;
									}
								}
							}
							if (count==0)
							{
								ends++;
								if (ends>2)
									return false;
							}
							else if (count>1)
							{
								return false;
							}
						}
					}
				}
			}
			LODGEN_ASSERT(!(ends&1));
			return true;
		}

		void Update(int currentMoveId, float expectedErrorChange)
		{
			std::set<std::pair<idT, idT> > rerenderPolygons;
			int vertexId=m_moves[currentMoveId].from;
			int targetVertex=m_moves[currentMoveId].to;
			Vertex &srcV=m_vertices[vertexId];
			Vertex &dstV=m_vertices[targetVertex];
			for (int phase=0; phase<2; phase++)
			{
#ifdef VISUAL_CHANGE_DEBUG
				CTimeValue dt=gEnv->pTimer->GetAsyncTime();
#endif
				// Update moves
				if (phase==0) // Gather every poly this move might effect
				{
					for (std::vector<idT>::const_iterator it=srcV.polys.begin(), end=srcV.polys.end(); it!=end; ++it)
					{
						const Poly &p=m_polys[*it];
						//if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2]) // Can't check this as polygons maybe connected by a degenerate
						{
							rerenderPolygons.insert(std::pair<idT,idT>(*it, 0));
							for (std::vector<idT>::const_iterator mit=p.moves.begin(), mend=p.moves.end(); mit!=mend; ++mit)
							{
								if (m_moves[*mit].from!=m_moves[*mit].to)
									rerenderPolygons.insert(std::pair<idT,idT>(*it, *mit));
							}
							for (int i=0; i<3; i++)
							{
								Vertex &v=m_vertices[p.v[i]];
								if (p.v[i]==vertexId)
									continue;
								for (std::vector<idT>::const_iterator it2=v.polys.begin(), end2=v.polys.end(); it2!=end2; ++it2)
								{
									const Poly &p2=m_polys[*it2];
									if (p2.v[0]!=p2.v[1] && p2.v[0]!=p2.v[2] && p2.v[1]!=p2.v[2])
									{
										for (std::vector<idT>::const_iterator mit=p2.moves.begin(), mend=p2.moves.end(); mit!=mend; ++mit)
										{
											Move &m=m_moves[*mit];
											if (m.from!=m.to)
											{
												//												if (((p2.v[0]==m.from)?m.to:p2.v[0])==vertexId || ((p2.v[1]==m.from)?m.to:p2.v[1])==vertexId || ((p2.v[2]==m.from)?m.to:p2.v[2])==vertexId) // Due to overlapping issues (only top plane is kept) it's better to rerender the whole patch (would be performance optimisation to restore this once fixed)
												{
													rerenderPolygons.insert(std::pair<idT,idT>(*it2, *mit));
												}
											}
										}
									}
								}
							}
						}
					}
				}
				for (int view=0; view<m_numViews; view++)
				{
					m_views[view].UpdateSpans(phase, rerenderPolygons, m_views[view].GetError(currentMoveId));
					m_threadPool->Submit(VisualChangeCalculatorViewJob::TaskUpdateSpans,&m_views[view]);
				}
				//m_threadPool->Start();
				m_threadPool->WaitAllJobsTemporary();

#ifdef VISUAL_CHANGE_DEBUG
				dt=gEnv->pTimer->GetAsyncTime()-dt;
				CryLog("%d: Rasterisation Time:%fms\n", phase, dt.GetMilliSeconds());
#endif
				if (phase==0)
				{
					// Patch moves
					for (std::vector<Move>::iterator it=m_moves.begin(), end=m_moves.end(); it!=end; ++it)
					{
						if (it->to==vertexId)
							it->to=targetVertex;
						if (it->from==vertexId)
							if (it->to==targetVertex) // Preserve "from" if becoming degenerate (for error checking)
								it->to=it->from;
							else
								it->from=targetVertex;
					}
					// Patch polys
					for (std::vector<Poly>::iterator it=m_polys.begin(), end=m_polys.end(); it!=end; ++it)
					{
						bool bWasNotDegenerate=(it->v[0]!=it->v[1] && it->v[0]!=it->v[2] && it->v[1]!=it->v[2]);
						for (int i=0; i<3; i++)
							if (it->v[i]==vertexId)
								it->v[i]=targetVertex;
						if (bWasNotDegenerate && (it->v[0]==it->v[1] || it->v[0]==it->v[2] || it->v[1]==it->v[2])) // If now degenerate remove duplicate moves
						{
							for (std::vector<idT>::const_iterator mit=it->moves.begin(), mend=it->moves.end(); mit!=mend; ++mit)
							{
								if (m_moves[*mit].from!=m_moves[*mit].to)
								{
									for (std::vector<idT>::const_iterator mit2=mit+1; mit2!=mend; ++mit2)
									{
										if (*mit!=*mit2 && m_moves[*mit].from==m_moves[*mit2].from && m_moves[*mit].to==m_moves[*mit2].to)
										{
											m_moves[*mit2].from=m_moves[*mit2].to;
										}
									}
								}
							}
						}
					}
					// Patch vertex
					for (std::vector<idT>::const_iterator it=srcV.polys.begin(), end=srcV.polys.end(); it!=end; ++it)
					{
						const Poly &p=m_polys[*it];
						//if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2]) // Can't ignore degenerate in case two polys are connected by one
						{
							bool found=false;
							for (std::vector<idT>::const_iterator dit=dstV.polys.begin(), dend=dstV.polys.end(); dit!=dend; ++dit)
							{
								if (*dit==*it)
								{
									found=true;
									break;
								}
							}
							if (!found)
							{
								dstV.polys.push_back(*it);
							}
						}
					}
#ifdef LODGEN_VERIFY
					if (m_bCheckTopology)
					{
						if (!IsPatchNice(dstV, targetVertex))
						{
							CryLogAlways("Patch not nice: %d\n", targetVertex);
							for (int i=0; i<dstV.polys.size(); i++)
							{
								CryLogAlways("Poly%d: %d %d %d\n", i, m_polys[dstV.polys[i]].v[0], m_polys[dstV.polys[i]].v[1], m_polys[dstV.polys[i]].v[2]);
							}
							LODGEN_ASSERT(false);
						}
					}
#endif
					for (std::vector<Move>::iterator it=m_moves.begin(), end=m_moves.end(); it!=end; ++it)
					{
						if (it->from!=it->to && it->from==targetVertex)
						{
							idT moveId=(idT)(it-m_moves.begin());
							for (std::vector<idT>::const_iterator dit=dstV.polys.begin(), dend=dstV.polys.end(); dit!=dend; ++dit)
							{
								Poly &p=m_polys[*dit];
								bool found=false;
								for (std::vector<idT>::iterator mit=p.moves.begin(), mend=p.moves.end(); mit!=mend; )
								{
									if (*mit==moveId)
									{
										found=true;
									}
									if (m_moves[*mit].to==m_moves[*mit].from) // Remove any invalid moves
									{
										mit = p.moves.erase(mit);
										mend = p.moves.end();
									}
									else
									{
										++mit;
									}
								}
								if (!found)
								{
									rerenderPolygons.insert(std::pair<idT,idT>(*dit, moveId));
									// Sorted insert
									for (std::vector<idT>::iterator mit=p.moves.begin(), mend=p.moves.end(); mit!=mend; ++mit)
									{
										if (*mit>moveId)
										{
											found=true;
											p.moves.insert(mit, moveId);
											break;
										}
									}
									if (!found)
										p.moves.push_back(moveId);
								}
							}
						}
					}
				}
			}
			VerifyStructures();
		}

		void SetParameters(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams)
		{
			m_metersPerPixel=pInputParams->metersPerPixel;
			m_bModelHasBase=pInputParams->bObjectHasBase;
			m_numElevations=pInputParams->numViewElevations;
			m_numAngles=pInputParams->numViewsAround;
			m_silhouetteWeight=pInputParams->silhouetteWeight;
			m_weldDistance=pInputParams->vertexWeldingDistance;
			m_bCheckTopology=pInputParams->bCheckTopology;
		}

		void FillData(CLODGeneratorLib::SLODSequenceGenerationOutput * pReturnValues)
		{
			m_pReturnValues = pReturnValues;
			if (pReturnValues->node != NULL)
			{

				CMesh *pMesh=pReturnValues->node->pMesh;
				if (pMesh)
				{
					
					Vec3* pVertices = pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);
					vtx_idx *pIndices = pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
					SMeshFace* pFaces = pMesh->GetStreamPtr<SMeshFace>(CMesh::FACES);
					int faces = pMesh->GetIndexCount()/3;
				
					// Set polys/vertices
					float epsilon=m_weldDistance*m_weldDistance; // Weld very close vertices together
					int numMoves=0;
					m_polys.clear();
					m_polys.reserve(faces+1);
					m_polys.push_back(Poly(0,0,0));
					m_originalTris.clear();
					m_originalTris.reserve(faces);
					m_moveList.clear();
					m_vertices.clear();
					m_noMoveList.clear();

					const int subsets = pMesh->GetSubSetCount();
					for (int subsetidx = 0; subsetidx < subsets; ++subsetidx)
					{
						int matId = pMesh->m_subsets[subsetidx].nMatID;

						int indexFirst = pMesh->m_subsets[subsetidx].nFirstIndexId;
						int indexLast = pMesh->m_subsets[subsetidx].nNumIndices + indexFirst;

						int subsetFaces = pMesh->m_subsets[subsetidx].nNumIndices/3;

						for (int i=0; i<subsetFaces; i++)
						{
							Poly p;
							int pId=m_polys.size();

							for (int k=0; k<3; k++)
							{
								Vec3 &o=pVertices[pIndices[indexFirst + (3*i+k)]];
								Vec3 v=o;
								std::vector<Vertex>::iterator it=m_vertices.begin(), end=m_vertices.end();
								for (; it!=end; ++it)
								{
									if (v.GetSquaredDistance(it->pos)<=epsilon)
									{
										p.v[k]=(int)(it-m_vertices.begin());
										break;
									}
								}
								if (it==end)
								{
									p.v[k]=m_vertices.size();
									m_vertices.push_back(Vertex(v));
								}
							}

							if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2])
							{
								for (int k=0; k<3; k++)
								{
									Vertex &vert=m_vertices[p.v[k]];
									vert.polys.push_back((idT)pId);
									numMoves++;
								}
								m_polys.push_back(p);
								m_originalTris.push_back(Tri(p.v));
							}
						}
					}
					if (m_bCheckTopology)
					{
						int nTopologyErrors = 0;
						for (std::vector<Vertex>::iterator it=m_vertices.begin(), end=m_vertices.end(); it!=end; ++it)
						{
							if (!IsPatchNice(*it, (int)(&*it-&m_vertices[0])))
							{
								nTopologyErrors++;
							}
						}
						if(nTopologyErrors > 0)
						{
							CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "LOD generator input mesh doesn't meet topology rules. Found %d violations.\n",nTopologyErrors);
						}
					}
					// Find ids from no move vertices
					std::vector<int> noMoveIds;
					for (std::vector<Vec3>::iterator nit=m_noMoveList.begin(), nend=m_noMoveList.end(); nit!=nend; ++nit)
					{
						Vec3 v=*nit;
						std::vector<Vertex>::iterator it=m_vertices.begin(), end=m_vertices.end();
						for (; it!=end; ++it)
						{
							if (v.GetSquaredDistance(it->pos)<=epsilon)
							{
								noMoveIds.push_back((int)(it-m_vertices.begin()));
								break;
							}
						}
					}
					std::sort(noMoveIds.begin(), noMoveIds.end());
					// Set moves and errors
					m_moves.clear();
					m_moves.reserve(numMoves+1);
					m_moves.push_back(Move(0,0));
					int noMoveStart=0;
					int noMoveSize=noMoveIds.size();
					for (int v=0; v<m_vertices.size(); v++)
					{
						Vertex &vert=m_vertices[v];
						if (noMoveStart<noMoveSize && noMoveIds[noMoveStart]==v)
						{
							while (noMoveStart+1<noMoveSize && noMoveIds[++noMoveStart]==v);
							continue; // Don't move this vertex
						}
						for (std::vector<idT>::iterator vit=vert.polys.begin(), vend=vert.polys.end(); vit!=vend; ++vit)
						{
							Poly &p=m_polys[*vit];
							if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2])
							{
								int to=p.v[0];
								for (int i=0; i<2; i++)
									if (v==p.v[i])
										to=p.v[i+1];
								for (std::vector<idT>::iterator vit2=vert.polys.begin(), vend2=vert.polys.end(); vit2!=vend2; ++vit2)
								{
									Poly &p2=m_polys[*vit2];
									p2.moves.push_back(m_moves.size());
								}
								m_moves.push_back(Move(v, to));
							}
						}
					}
					// Verify
					VerifyStructures();
				}
			}
		}

		void InitialRender(volatile sProgress *pProgress)
		{
			if ( pProgress->eStatus == ESTATUS_CANCEL )
				return;

			int numElevations=m_numElevations;
			int numAngles=m_numAngles;
			float phaseStep=gf_PI2/(float)numAngles;
			float elevationAngle=gf_PI/(float)(numElevations+1);
			if (!m_bModelHasBase)
				numElevations=(numElevations+1)/2;
			float phaseChangePerElevation=phaseStep/(float)numElevations;
			std::vector<Vec3> directions;
			directions.reserve(numElevations*numAngles+2);
			for (int k=0; k<numElevations; k++)
			{
				if ( pProgress->eStatus == ESTATUS_CANCEL )
					return;

				float phase=k*phaseChangePerElevation;
				float zAngle=(k+1)*elevationAngle;
				float sz=sinf(zAngle);
				float cz=-cosf(zAngle);
				for (int i=0; i<numAngles; i++)
				{
					if ( pProgress->eStatus == ESTATUS_CANCEL )
						return;

					Vec3 dir(sz*sinf(phase), sz*cosf(phase), cz);
					directions.push_back(dir);
					phase+=phaseStep;
				}
			}
			directions.push_back(Vec3(0.0f, 0.0f, -1.0f));
			if (m_bModelHasBase)
				directions.push_back(Vec3(0.0f, 0.0f, +1.0f));
			m_numViews=directions.size();
			// Batch up starting new threads as initial render causes lots of allocations which contend if lots of threads are active
			m_views=new VisualChangeCalculatorViewJob[m_numViews];
			int batch=8;
			for (int view=0; view<m_numViews; view++)
			{
				if ( pProgress->eStatus == ESTATUS_CANCEL )
					return;

				directions[view].Normalize();
				m_views[view].CreateView(m_moves, m_polys, m_vertices, directions[view], m_metersPerPixel, m_silhouetteWeight);
				m_threadPool->Submit(VisualChangeCalculatorViewJob::TaskCreateView,&m_views[view]);
			}
			//m_threadPool->Start();
			m_threadPool->WaitAllJobsTemporary();
		}

		void Process(volatile sProgress *pProgress)
		{
			int iCount = 0;

			int iDebugIndex = 0;
			float maxError=0.0f;
			while (true)
			{
				iDebugIndex++;

				if ( pProgress->eStatus == ESTATUS_CANCEL )
					return;

				for (int view=0; view<m_numViews; view++)
				{
					m_views[view].CalculateError();
					m_threadPool->Submit(VisualChangeCalculatorViewJob::TaskCalculateerror,&m_views[view]);
				}
				//m_threadPool->Start();
				m_threadPool->WaitAllJobsTemporary();

				float minError=maxError;
				float minDist=0.0f;
				int bestMove=-1;
				int checkedMoves=0;
				for (int i=0; i<m_moves.size(); i++)
				{
					if (m_moves[i].from!=m_moves[i].to)
					{
						float error=0.0f;
						for (int view=0; view<m_numViews; view++)
						{
							error+=m_views[view].GetError(i);
						}
						checkedMoves++;
						float dist=m_vertices[m_moves[i].from].pos.GetSquaredDistance(m_vertices[m_moves[i].to].pos);
						if (bestMove==-1 || error<minError || (error==minError && dist<minDist))
						{
							if (m_bCheckTopology)
							{
								Vertex srcV=m_vertices[m_moves[i].from];
								Vertex dstV=m_vertices[m_moves[i].to];
								Vertex tmpVtx=dstV;
								for (std::vector<idT>::const_iterator it=srcV.polys.begin(), end=srcV.polys.end(); it!=end; ++it)
								{
									const Poly &p=m_polys[*it];
									if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2]) // ignore degenerates
									{
										bool found=false;
										for (std::vector<idT>::const_iterator dit=tmpVtx.polys.begin(), dend=tmpVtx.polys.end(); dit!=dend; ++dit)
										{
											if (*dit==*it)
											{
												found=true;
												break;
											}
										}
										if (!found)
										{
											tmpVtx.polys.push_back(*it);
										}
									}
								}
								if (!IsPatchNice(tmpVtx, m_moves[i].to, m_moves[i].from))
								{
									continue;
								}
							}
							minError=error;
							minDist=dist;
							bestMove=i;
						}
					}
				}

				if (bestMove==-1)
					break;
#ifdef VISUAL_CHANGE_DEBUG
				dt=gEnv->pTimer->GetAsyncTime()-dt;
				CryLog("LODGen: Best move: %d %d->%d Error:%f Time:%fms\n", bestMove, m_moves[bestMove].from, m_moves[bestMove].to, minError, dt.GetMilliSeconds());
				dt=gEnv->pTimer->GetAsyncTime();
#endif
				pProgress->fProgress = (1.0f-(checkedMoves/(float)m_moves.size()) + pProgress->fCompleted) / pProgress->fParts;

				int iProcess = (int)(pProgress->fProgress*100);
				if (iProcess > iCount)
				{
					iCount = iProcess;
					RCLog("Process Lodder Process: %d/100",iCount);
				}

				//RCLog("------------------Test bestMove:%d iDebugIndex:%d",bestMove,iDebugIndex);

				m_moveList.push_back(TakenMove(m_moves[bestMove].from, m_moves[bestMove].to, minError));
				Update(bestMove, minError);
				for (int view=0; view<m_numViews; view++)
				{
					m_views[view].ZeroErrorValues();
				}
#ifdef VISUAL_CHANGE_DEBUG
				dt=gEnv->pTimer->GetAsyncTime()-dt;
				CryLog("LODGen: Update Time:%fms\n", dt.GetMilliSeconds());
#endif
			}
			pProgress->fProgress = (pProgress->fCompleted + 1.0f) / pProgress->fParts;
		}


		void VerifyStructures()
		{
#ifdef LODGEN_VERIFY
			for (std::vector<Poly>::iterator pit=m_polys.begin(), pend=m_polys.end(); pit!=pend; ++pit)
			{
				if (pit->v[0]!=pit->v[1] && pit->v[0]!=pit->v[2] && pit->v[1]!=pit->v[2])
				{
					for (std::vector<Poly>::iterator pit2=pit+1; pit2!=pend; ++pit2)
					{
						LODGEN_ASSERT(!(pit->v[0]==pit2->v[0] && pit->v[1]==pit2->v[1] && pit->v[2]==pit2->v[2]));
						LODGEN_ASSERT(!(pit->v[0]==pit2->v[1] && pit->v[1]==pit2->v[2] && pit->v[2]==pit2->v[0]));
						LODGEN_ASSERT(!(pit->v[0]==pit2->v[2] && pit->v[1]==pit2->v[0] && pit->v[2]==pit2->v[1]));
					}
					for (int i=0; i<3; i++)
					{
						Vertex &v=m_vertices[pit->v[i]];
						bool found=false;
						for (std::vector<idT>::iterator it=v.polys.begin(), end=v.polys.end(); it!=end; ++it)
						{
							if (*it==pit-m_polys.begin())
							{
								LODGEN_ASSERT(!found);
								found=true;
							}
						}
						LODGEN_ASSERT(found);
						found=false;
						for (std::vector<Move>::const_iterator it=m_moves.begin(), end=m_moves.end(); it!=end; ++it)
						{
							if (it->from!=it->to)
							{
								if (it->from==pit->v[i])
								{
									bool found2=false;
									for (std::vector<idT>::iterator it2=pit->moves.begin(), end2=pit->moves.end(); it2!=end2; ++it2)
									{
										if (*it2==it-m_moves.begin())
										{
											LODGEN_ASSERT(!found2):
												found2=true;
										}
									}
									LODGEN_ASSERT(found2):
								}
								if (it->from==pit->v[i] && it->to==pit->v[(i==2)?0:(i+1)])
								{
									if (found)
									{
										CryLogAlways("Duplicate move %d->%d\n", it->from, it->to);
										//it->from=it->to;
									}
									found=true;
								}
							}
						}
						LODGEN_ASSERT(found);
					}
				}
			}
#endif
		}
	};
}
