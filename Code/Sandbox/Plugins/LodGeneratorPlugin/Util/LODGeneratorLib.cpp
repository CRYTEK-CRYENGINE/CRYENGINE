/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2014.
*************************************************************************/

#include "StdAfx.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include "LODGeneratorLib.h"
#include "panel/LODInterface.h"
#include <CryThreading/IThreadManager.h>
#include "AtlasGenerator.h"
#include "TopologyGraph.h"
#include "TopologyTransform.h"
#include "../panel/AutoGeneratorUVUnwrap.h"

namespace LODGenerator 
{

	// For documentation search for "LOD Generator" on Confluence

	//#define VISUAL_CHANGE_DEBUG			// Timings, debug printfs and double checking of error metric
	//#define LODGEN_VERIFY						// Full verify of all data structures. Slow but possibly useful for debugging
#define MAX_MOVELIMIT	4096				// Maximum moves one polygon can be involved in. (Summation of all moves each of it's vertices can do)
#define INCLUDE_LOD_GENERATOR

#define LODGEN_ASSERT(condition) { if (!(condition)) __debugbreak(); }

	///////////////////////////////////////////////////////////
	// Internal Workings
	///////////////////////////////////////////////////////////

#ifdef INCLUDE_LOD_GENERATOR 

	enum EStatus
	{
		ESTATUS_RUNNING,
		ESTATUS_CANCEL
	};

	struct sProgress
	{
		float fProgress;
		EStatus eStatus;
		float fParts;
		float fCompleted;
	};

	namespace VisualChange
	{
		typedef vtx_idx idT;

		struct Vertex
		{
			Vertex(Vec3& p)
			{
				pos=p;
			}
			Vec3 pos;
			std::vector<idT> polys; // All actuals this vertex is part of
		};

		struct Poly
		{
			Poly() {}
			Poly(int a, int b, int c)
			{
				v[0]=a; v[1]=b; v[2]=c;
			}
			int v[3];
			std::vector<idT> moves;
		};

		struct Move
		{
			Move(idT f, idT t)
			{
				from=f;
				to=t;
			}
			idT from;
			idT to;
		};

		class VisualChangeCalculatorView
		{
		public:
			struct ZSpanData
			{
				ZSpanData(float h, idT move, idT poly)
				{
					height=h;
					moveId=move;
					polyId=poly;
				};
				float height;
				idT moveId;		// Index into move array
				idT polyId;		// Source polygon id (modified) that produced data
			};

			struct ZSpan
			{
				float error;
				float reference;
				std::vector<ZSpanData> data;
			};

			std::vector<ZSpan> m_spans;
			std::vector<float> m_error;
			std::vector<Vec3> m_transVtx;
#ifdef VISUAL_CHANGE_DEBUG
			std::vector<float> m_startingError;
#endif
			const std::vector<Move> &m_moves;
			const std::vector<Poly> &m_polys;
			const std::vector<Vertex> &m_vertices;
			Matrix34 m_mtx;
			int m_width;
			int m_height;
			float m_farPlane;

			VisualChangeCalculatorView(const std::vector<Move> &moves, const std::vector<Poly> &polys, const std::vector<Vertex> &vertices, Vec3 viewDirection, float metersPerPixel, float silhouetteWeight) :
				m_moves(moves),
				m_polys(polys),
				m_vertices(vertices)
			{
				AABB bb;
				float rightLen, upLen;
				bb.Reset();
				// Set mtx/width/height
				for (std::vector<Vertex>::const_iterator it=vertices.begin(), end=vertices.end(); it!=end; ++it)
				{
					bb.Add(it->pos);
				}
				m_mtx=CreateViewMatrixFromDirection(viewDirection, bb, rightLen, upLen, metersPerPixel);
				m_farPlane=2.0f*bb.GetRadius()*(1.0f+silhouetteWeight)/metersPerPixel;
				m_width=max((int)ceilf(rightLen),1);
				m_height=max((int)ceilf(upLen),1);
				// Transform matrices
				m_transVtx.resize(vertices.size());
				for (int i=0; i<vertices.size(); i++)
				{
					m_transVtx[i]=m_mtx*vertices[i].pos;
					LODGEN_ASSERT(m_transVtx[i].z>=0.0f);
				}
				// Set error
				m_error.resize(moves.size());
				memset(&m_error[0], 0, m_error.size()*sizeof(m_error[0]));
				// Set spans
				CryLog("LODGen: Using resolution of:%dx%d\n", m_width, m_height);
				m_spans.clear();
				m_spans.resize(m_width*m_height);
				FullRender();
			};

			void CalculateError()
			{
				// Explanation: moveList contains all moves that haven't yet hit (unless initially empty which means it contains every single move)
				//              Each hit filters down this list. If a move is hit and isn't isn't on the list no error as added (because hit happened earlier)
				idT moveList[2][MAX_MOVELIMIT];
				for (std::vector<ZSpan>::const_iterator sit=m_spans.begin(), send=m_spans.end(); sit!=send; ++sit)
				{
					const ZSpan &span=*sit;
					float reference=span.reference;
					float currentError=span.error;
					int currentList=0;
					idT *moveStart=moveList[currentList];
					idT *moveEnd=moveStart;
					bool bFirst=false;
					std::vector<ZSpanData>::const_iterator begin=span.data.begin();
					for (std::vector<ZSpanData>::const_iterator it=begin, end=span.data.end(); it!=end; ++it)
					{
						int actualId=it->polyId;
						if (it->moveId==0)
						{
							if (moveEnd==moveStart) // First time seeing an actual
							{
								for (std::vector<idT>::const_iterator mit=m_polys[actualId].moves.begin(), mend=m_polys[actualId].moves.end(); mit!=mend; ++mit)
								{
									// Search back for modifieds that are in the list (because we've already hit)
// 									std::vector<ZSpanData>::const_iterator rit=it-1;
// 									for (; rit!=(begin-1); --rit)
// 									{
// 										if (rit->moveId==*mit)
// 											break;
// 									}
// 
// 									if (rit==(begin-1))
// 									{
// 										*(moveEnd++)=*mit;
// 									}

									bool found = false;
									std::vector<ZSpanData>::const_iterator rit = it;
									int count = rit - begin;
									for (int i=0;i < count;++i)
									{
										--rit;
										if (rit->moveId==*mit)
										{
											found = true;
											break;
										}
									}

									if (!found)
									{
										*(moveEnd++)=*mit;
									}
								}
								LODGEN_ASSERT(moveEnd<=&moveStart[MAX_MOVELIMIT]);
								if (moveStart==moveEnd) // Found a hit on all moves
									break;
							}
							else // Not the first so merge lists
							{
								std::vector<idT>::const_iterator mend=m_polys[actualId].moves.end();
								std::vector<idT>::const_iterator mit=m_polys[actualId].moves.begin();
								idT *newMoveListEnd=moveList[1-currentList];
								for (idT *m=moveStart; m!=moveEnd; m++)
								{
									while (mit!=mend && *mit<*m) // Skip any not in list
										mit++;
									if (mit==mend || *mit!=*m)
									{
										m_error[*m]+=fabsf(it->height-reference) - currentError;
									}
									else
									{
										*(newMoveListEnd++)=*m;
									}
								}
								currentList=1-currentList;
								moveStart=moveList[currentList];
								moveEnd=newMoveListEnd;
								if (moveStart==moveEnd) // Found a hit on all moves
								{
									break;
								}
							}
						}
						else
						{
							if (moveEnd==moveStart) // Haven't hit an actual yet so just add error
							{
								m_error[it->moveId]+=fabsf(it->height-reference) - currentError;
							}
							else
							{
								for (idT *m=moveStart; m!=moveEnd; m++)
								{
									if (*m==it->moveId)
									{
										m_error[it->moveId]+=fabsf(it->height-reference) - currentError;
										idT *newMoveListEnd=moveList[1-currentList];
										for (idT *m2=moveStart; m2!=moveEnd; m2++)
										{
											if (m2!=m)
												*(newMoveListEnd++)=*m2;
										}
										currentList=1-currentList;
										moveStart=moveList[currentList];
										moveEnd=newMoveListEnd;
										break;
									}
									else if (*m>it->moveId)
									{
										break;
									}
								}
								if (moveStart==moveEnd) // Found a hit on all moves
								{
									break;
								}
							}
						}
					}
				}
			}

			void InsertIntoSpan(ZSpan &span, float height, idT moveId, idT polyId)
			{
				// Keep span sorted in height order. Only keep highest poly for a single move
				std::vector<ZSpanData>::iterator begin=span.data.begin();
				std::vector<ZSpanData>::iterator it=begin, end=span.data.end(), cur;
				for (; it!=end; ++it)
				{
					if (moveId && it->moveId==moveId && it->height<=height)
					{
						return;
					}
					if (it->height>=height)
					{
						break;
					}
				}
				LODGEN_ASSERT(it!=end);
				cur = span.data.insert(it, ZSpanData(height, moveId, polyId));
				if (moveId)
				{
					end=span.data.end();
					for (it=cur + 1; it!=end; ++it)
					{
						if (it->moveId==moveId)
						{
							span.data.erase(it);
							return;
						}
					}
				}
			}

			void GetBounds(const Vec3& a, const Vec3& b, const Vec3& c, int &mx, int &mX, int &my, int &mY)
			{
				Vec3 minBB, maxBB;
				mx=(int)floorf(min(a.x, min(b.x, c.x)));
				mX=(int)floorf(max(a.x, max(b.x, c.x)));
				my=(int)floorf(min(a.y, min(b.y, c.y)));
				mY=(int)floorf(max(a.y, max(b.y, c.y)));
				LODGEN_ASSERT(!(mx<0 || my<0 || mX>=m_width || mY>=m_height));
			}

			bool RayHit(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3 &n, int x, int y, float &outHeight)
			{
				Vec3 p((float)x,(float)y,0.0f);
				if ((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x)>=0.0f)
					return false;
				if ((c.x-b.x)*(p.y-b.y)-(c.y-b.y)*(p.x-b.x)>=0.0f)
					return false;
				if ((a.x-c.x)*(p.y-c.y)-(a.y-c.y)*(p.x-c.x)>=0.0f)
					return false;
				outHeight=(n.Dot(a)-p.x*n.x-p.y*n.y)/n.z;
				LODGEN_ASSERT(outHeight>=0.0f);
				return true;
			}

			void RenderPolygon(const Vec3& a, const Vec3 &b, const Vec3 &c, int polyId, int moveId)
			{
				int mx, mX, my, mY;
				Vec3 n=(b-a).Cross(c-a);
				if (n.z<0.0f)
				{
					GetBounds(a,b,c,mx,mX,my,mY);
					for (int y=my; y<=mY; y++)
					{
						for (int x=mx; x<=mX; x++)
						{
							float height;
							if (RayHit(a,b,c,n,x,y,height))
							{
								ZSpan &span=m_spans[y*m_width+x];
								InsertIntoSpan(span,height,moveId,polyId);	
								if (moveId==0)
								{
									for (std::vector<ZSpanData>::const_iterator it=span.data.begin(), end=span.data.end(); it!=end; ++it)
									{
										if (it->moveId==0) // actual
										{
											span.error=fabsf(span.reference-it->height);
											break;
										}
									}
								}
							}
						}
					}
				}
			}

			void DerenderPolygon(const Vec3& a, const Vec3 &b, const Vec3 &c, int polyId, int moveId)
			{
				int mx, mX, my, mY;
				Vec3 n=(b-a).Cross(c-a);
				if (n.z<0.0f)
				{
					GetBounds(a,b,c,mx,mX,my,mY);
					for (int y=my; y<=mY; y++)
					{
						for (int x=mx; x<=mX; x++)
						{
							float height;
							if (RayHit(a,b,c,n,x,y,height))
							{
								ZSpan &span=m_spans[y*m_width+x];
								std::vector<ZSpanData>::iterator it=span.data.begin(), end=span.data.end();
								for (; it!=end; ++it)
								{
									if (it->polyId==polyId && it->moveId==moveId)
										break;
								}
								if (it!=end)
								{
									span.data.erase(it);
								}
								if (moveId==0)
								{
									end=span.data.end();
									for (it=span.data.begin(); it!=end; ++it)
									{
										if (it->moveId==0) // actual
										{
											span.error=fabsf(span.reference-it->height);
											break;
										}
									}
								}
							}
						}
					}
				}
			}

			void FullRender()
			{
				float farPlane=m_farPlane;
				// Insert silhouette plane
				for (std::vector<ZSpan>::iterator sit=m_spans.begin(), send=m_spans.end(); sit!=send; ++sit)
				{
					ZSpan &span=*sit;
					span.data.push_back(ZSpanData(farPlane, 0, 0));
					span.error=0.0f;
					span.reference=farPlane;
				}
				// Insert actuals
				for (int i=0; i<m_polys.size(); i++)
				{
					const Poly &p=m_polys[i];
					RenderPolygon(m_transVtx[p.v[0]], m_transVtx[p.v[1]], m_transVtx[p.v[2]], i, 0);
				}
				// Insert modified
				for (int m=0; m<m_moves.size(); m++)
				{
					const Move &move=m_moves[m];
					const Vertex &v=m_vertices[move.from];
					if (move.from==move.to)
						continue;
					for (std::vector<idT>::const_iterator it=v.polys.begin(), end=v.polys.end(); it!=end; ++it)
					{
						const Poly &p=m_polys[*it];
						RenderPolygon(m_transVtx[(p.v[0]==move.from)?move.to:p.v[0]], m_transVtx[(p.v[1]==move.from)?move.to:p.v[1]], m_transVtx[(p.v[2]==move.from)?move.to:p.v[2]], *it, m);
					}
				}
				// Set reference data
				for (std::vector<ZSpan>::iterator sit=m_spans.begin(), send=m_spans.end(); sit!=send; ++sit)
				{
					ZSpan &span=*sit;
					float height=0.0f;
					for (std::vector<ZSpanData>::const_iterator it=span.data.begin(), end=span.data.end(); it!=end; ++it)
					{
						if (it->moveId==0) // actual
						{
							height=it->height;
							break;
						}
					}
					span.reference=height;
					span.error=0.0f;
				}
				VerifySpans();
			}

			void AssertDerendered(int moveId, int polyId)
			{
#ifdef LODGEN_VERIFY
				for (std::vector<ZSpan>::iterator sit=m_spans.begin(), send=m_spans.end(); sit!=send; ++sit)
				{
					ZSpan &span=*sit;
					for (std::vector<ZSpanData>::const_iterator it=span.data.begin(), end=span.data.end(); it!=end; ++it)
					{
						LODGEN_ASSERT(!(moveId==moveId && it->polyId==polyId));
					}
				}
#endif
			}

			void VerifySpans()
			{
#ifdef LODGEN_VERIFY
				for (std::vector<ZSpan>::const_iterator sit=m_spans.begin(), send=m_spans.end(); sit!=send; ++sit)
				{
					const ZSpan &span=*sit;
					float height=0.0f;
					bool bFound=false;
					float current=-1.0f;
					int checked=0;
					for (std::vector<ZSpanData>::const_iterator it=span.data.begin(), end=span.data.end(); it!=end; ++it)
					{
						if (!bFound && it->moveId==0)
						{
							LODGEN_ASSERT(span.error==fabsf(span.reference-it->height));
							bFound=true;
						}
						LODGEN_ASSERT(height<=it->height);
						for (std::vector<ZSpanData>::const_iterator it2=it+1; it2!=end; ++it2)
						{
							LODGEN_ASSERT(!(it->moveId==it2->moveId && it->polyId==it2->polyId));
						}
						if (it->polyId!=0)
						{
							const Poly &p=m_polys[it->polyId];
							int id=sit-m_spans.begin();
							int ids[3]={p.v[0], p.v[1], p.v[2]};
							int x=id%m_width;
							int y=id/m_width;
							bool bDraw=true;
							if (it->moveId!=0)
							{
								const Move &m=m_moves[it->moveId];
								if (m.from==m.to) // FIXME: Degenerate moves have tris in buffer
									bDraw=false;
								ids[0]=(ids[0]==m.from)?m.to:ids[0];
								ids[1]=(ids[1]==m.from)?m.to:ids[1];
								ids[2]=(ids[2]==m.from)?m.to:ids[2];
							}
							if (bDraw)
							{
								Vec3 p0=m_transVtx[ids[0]];
								Vec3 p1=m_transVtx[ids[1]];
								Vec3 p2=m_transVtx[ids[2]];
								Vec3 n=(p1-p0).Cross(p2-p0);
								if (n.z<0.0f)
								{
									float height;
									checked++;
									if (RayHit(p0,p1,p2,n,x,y,height))
									{
										LODGEN_ASSERT(height==it->height);
									}
									else
									{
										LODGEN_ASSERT(false);
									}
								}
								else
								{
									LODGEN_ASSERT(false);
								}
							}
						}
						height=it->height;
					}
				}
				for (int i=0; i<m_polys.size(); i++)
				{
					const Poly &p=m_polys[i];
					int mx, mX, my, mY;
					Vec3 a=m_transVtx[p.v[0]];
					Vec3 b=m_transVtx[p.v[1]];
					Vec3 c=m_transVtx[p.v[2]];
					Vec3 n=(b-a).Cross(c-a);
					if (n.z<0.0f)
					{
						GetBounds(a,b,c,mx,mX,my,mY);
						for (int y=my; y<=mY; y++)
						{
							for (int x=mx; x<=mX; x++)
							{
								float height;
								if (RayHit(a,b,c,n,x,y,height))
								{
									ZSpan &span=m_spans[y*m_width+x];
									bool bFound=false;
									for (std::vector<ZSpanData>::const_iterator it=span.data.begin(), end=span.data.end(); it!=end; ++it)
									{
										if (it->moveId==0 && it->polyId==i)
										{
											LODGEN_ASSERT(it->height==height);
											bFound=true;
											break;
										}
									}
									LODGEN_ASSERT(bFound);
								}
							}
						}
					}
				}
				// Insert modified
				for (int m=0; m<m_moves.size(); m++)
				{
					const Move &move=m_moves[m];
					const Vertex &v=m_vertices[move.from];
					if (move.from==move.to)
						continue;
					for (std::vector<idT>::const_iterator it=v.polys.begin(), end=v.polys.end(); it!=end; ++it)
					{
						const Poly &p=m_polys[*it];
						int mx, mX, my, mY;
						Vec3 a=m_transVtx[(p.v[0]==move.from)?move.to:p.v[0]];
						Vec3 b=m_transVtx[(p.v[1]==move.from)?move.to:p.v[1]];
						Vec3 c=m_transVtx[(p.v[2]==move.from)?move.to:p.v[2]];
						Vec3 n=(b-a).Cross(c-a);
						if (n.z<0.0f)
						{
							GetBounds(a,b,c,mx,mX,my,mY);
							for (int y=my; y<=mY; y++)
							{
								for (int x=mx; x<=mX; x++)
								{
									float height;
									if (RayHit(a,b,c,n,x,y,height))
									{
										ZSpan &span=m_spans[y*m_width+x];
										bool bFound=false;
										for (std::vector<ZSpanData>::const_iterator sit=span.data.begin(), send=span.data.end(); sit!=send; ++sit)
										{
											if (sit->moveId==m && sit->polyId==*it)
											{
												LODGEN_ASSERT(sit->height==height);
												bFound=true;
												break;
											}
											else if (sit->moveId==m && sit->height<=height)
											{
												bFound=true;
												break;
											}
										}
										LODGEN_ASSERT(bFound);
									}
								}
							}
						}
					}
				}
#endif
			}

			Matrix34 CreateViewMatrixFromDirection(Vec3 direction, AABB bb, float &rightLen, float &upLen, float distancePerPixel)
			{
				Vec3 up=Vec3(0.0f, 0.0f, 1.0f);
				Vec3 right=Vec3(1.0f, 0.0f, 0.0f);
				if (fabsf(direction.Dot(up))>fabsf(direction.Dot(right)))
				{
					up=right;
				}
				right=up.Cross(direction);
				right.Normalize();
				up=direction.Cross(right);
				up.Normalize();
				Matrix34 imtx, mtx, orthographic, rot;
				float radius=bb.GetRadius();
				rot=Matrix34::CreateFromVectors(right, up, direction, bb.GetCenter()-direction*radius);
				orthographic.SetIdentity();
				float rMax=0.0f, uMax=0.0f;
				for (int i=0; i<4; i++)
				{
					Vec3 brace;
					brace.x=(i&1)?(bb.max.x-bb.min.x):(bb.min.x-bb.max.x);
					brace.y=(i&2)?(bb.max.y-bb.min.y):(bb.min.y-bb.max.y);
					brace.z=bb.max.z-bb.min.z;
					rMax=max(fabsf(right.Dot(brace)), rMax);
					uMax=max(fabsf(up.Dot(brace)), uMax);
				}
				orthographic.m00=rMax*1.01f;
				orthographic.m11=uMax*1.01f;
				orthographic.m03=-0.5f*orthographic.m00;
				orthographic.m13=-0.5f*orthographic.m11;
				imtx=rot*orthographic;
				mtx=imtx;
				mtx.Invert();
				rightLen=rMax/distancePerPixel;
				upLen=uMax/distancePerPixel;
				mtx.m00*=rightLen;
				mtx.m01*=rightLen;
				mtx.m02*=rightLen;
				mtx.m03*=rightLen;
				mtx.m10*=upLen;
				mtx.m11*=upLen;
				mtx.m12*=upLen;
				mtx.m13*=upLen;
				mtx.m20*=1.0f/distancePerPixel;
				mtx.m21*=1.0f/distancePerPixel;
				mtx.m22*=1.0f/distancePerPixel;
				mtx.m23*=1.0f/distancePerPixel;
				return mtx;
			}

			void UpdateSpans(int phase, const std::set<std::pair<idT,idT> > &rerenderPolygons, float expectedError)
			{
#ifdef VISUAL_CHANGE_DEBUG
				if (phase==0)
				{
					m_startingError.resize(m_spans.size());
					for (int i=0; i<m_spans.size(); i++)
					{
						m_startingError[i]=m_spans[i].error;
					}
				}
#endif
				for (std::set<std::pair<idT,idT> >::const_iterator it=rerenderPolygons.begin(), end=rerenderPolygons.end(); it!=end; ++it)
				{
					const Poly &p=m_polys[it->first];
					if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2])
					{
						if (phase==0)
						{
							if (it->second==0)
							{
								DerenderPolygon(m_transVtx[p.v[0]], m_transVtx[p.v[1]], m_transVtx[p.v[2]], it->first, it->second);
							}
							else
							{
								const Move &m=m_moves[it->second];
								DerenderPolygon(m_transVtx[(p.v[0]==m.from)?m.to:p.v[0]], m_transVtx[(p.v[1]==m.from)?m.to:p.v[1]], m_transVtx[(p.v[2]==m.from)?m.to:p.v[2]], it->first, it->second);
							}
							AssertDerendered(it->second, it->first);
						}
						else
						{
							AssertDerendered(it->second, it->first);
							if (it->second==0)
							{
								RenderPolygon(m_transVtx[p.v[0]], m_transVtx[p.v[1]], m_transVtx[p.v[2]], it->first, it->second);
							}
							else
							{
								const Move &m=m_moves[it->second];
								RenderPolygon(m_transVtx[(p.v[0]==m.from)?m.to:p.v[0]], m_transVtx[(p.v[1]==m.from)?m.to:p.v[1]], m_transVtx[(p.v[2]==m.from)?m.to:p.v[2]], it->first, it->second);
							}
						}
					}
				}
#ifdef VISUAL_CHANGE_DEBUG
				if (phase==1)
				{
					float totalError=0.0f;
					VerifySpans();
					for (int i=0; i<m_spans.size(); i++)
					{
						totalError+=m_spans[i].error-m_startingError[i];
					}
					if (fabsf(totalError-expectedError)>0.001f)
					{
						CryLogAlways("LODGen Warning: Was expecting an error of %f but got %f\n", expectedError, totalError);
						// LODGEN_ASSERT(false); // Never happens but possible due to floating point precision issues
					}
				}
#endif
			}
		};

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
			VisualChangeCalculatorViewThread *m_views;

			float m_metersPerPixel;
			float m_silhouetteWeight;
			float m_weldDistance;
			bool m_bModelHasBase;
			int m_numElevations;
			int m_numAngles;
			bool m_bCheckTopology;

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
			}

			~VisualChangeCalculator()
			{
				// The threads contend on the allocator when freeing data as they shut down. Batching up eleviates this somewhat
				int batch=8;
				for (int view=0; view<m_numViews; view++)
				{
					if (view>=batch)
					{
						m_views[view-batch].WaitForCompletion();
						CryLog("LODGen: Done waiting for %d to quit\n", view-batch);
					}
					m_views[view].SignalStopWork();
				}
				for (int view=max(m_numViews-batch,0); view<m_numViews; view++)
				{
					m_views[view].WaitForCompletion();
					gEnv->pThreadManager->JoinThread(&m_views[view], eJM_Join);
					CryLog("LODGen: Done waiting for %d to quit\n", view);
				}
				delete[] m_views;
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
					}
					for (int view=0; view<m_numViews; view++)
					{
						m_views[view].WaitForCompletion();
					}
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
				IIndexedMesh *pIndexedMesh=pReturnValues->pStatObj->GetIndexedMesh(true);
				if (pIndexedMesh)
				{
					CMesh *pMesh=pIndexedMesh->GetMesh();
					if (pMesh)
					{
						int faces=pMesh->GetIndexCount()/3;
						Vec3* pVertices=pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);
						vtx_idx *pIndices=pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
						IMaterial * pMaterial = pReturnValues->pStatObj->GetMaterial();
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
							bool include = true;

							int matId = pMesh->m_subsets[subsetidx].nMatID;
							IMaterial * pSubMat = pMaterial->GetSubMtlCount() > 0 ? pMaterial->GetSubMtl(matId) : pMaterial;
							if (!pSubMat)
								continue;

							IShader * pShader = pSubMat->GetShaderItem().m_pShader;
							if (!pShader)
								continue;

							SShaderGen *pShaderGen = pShader->GetGenerationParams();    
							if (!pShaderGen)
								continue;

							uint64 nMask = pShader->GetGenerationMask();
							for (int nBit(0); nBit < pShaderGen->m_BitMask.size(); ++nBit)
							{
								SShaderGenBit *pBit = pShaderGen->m_BitMask[nBit];
								if( nMask & pBit->m_Mask )
								{                
									string genparam(pBit->m_ParamName);
									if ( genparam == "%DECAL" )
									{
										include = false;
										break;
									}
								}
							}

							if ( !include )
								continue;

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
				m_views=new VisualChangeCalculatorViewThread[m_numViews];
				int batch=8;
				for (int view=0; view<m_numViews; view++)
				{
					if ( pProgress->eStatus == ESTATUS_CANCEL )
						return;

					if (view>=batch)
					{
						m_views[view-batch].WaitForCompletion();
						CryLog("LODGen: Done waiting for %d (%dx%d) to do initial raster\n", view-batch, m_views[view-batch].m_view->m_width, m_views[view-batch].m_view->m_height);
					}
					directions[view].Normalize();
					m_views[view].CreateView(m_moves, m_polys, m_vertices, directions[view], m_metersPerPixel, m_silhouetteWeight);
				}
				for (int view=max(m_numViews-batch,0); view<m_numViews; view++)
				{
					if ( pProgress->eStatus == ESTATUS_CANCEL )
						return;

					m_views[view].WaitForCompletion();
					CryLog("LODGen: Done waiting for %d (%dx%d) to do initial raster\n", view, m_views[view].m_view->m_width, m_views[view].m_view->m_height);
				}
			}

			void Process(volatile sProgress *pProgress)
			{
				float maxError=0.0f;
				while (true)
				{
					if ( pProgress->eStatus == ESTATUS_CANCEL )
						return;

					CTimeValue dt=gEnv->pTimer->GetAsyncTime();
					for (int view=0; view<m_numViews; view++)
					{
						if ( pProgress->eStatus == ESTATUS_CANCEL )
							return;

						m_views[view].CalculateError();
					}
					for (int view=0; view<m_numViews; view++)
					{
						if ( pProgress->eStatus == ESTATUS_CANCEL )
							return;

						m_views[view].WaitForCompletion();
					}

					if ( pProgress->eStatus == ESTATUS_CANCEL )
						return;

					float minError=maxError;
					float minDist=0.0f;
					int bestMove=-1;
					int checkedMoves=0;
					for (int i=0; i<m_moves.size(); i++)
					{
						if ( pProgress->eStatus == ESTATUS_CANCEL )
							return;

						if (m_moves[i].from!=m_moves[i].to)
						{
							float error=0.0f;
							for (int view=0; view<m_numViews; view++)
							{
								if ( pProgress->eStatus == ESTATUS_CANCEL )
									return;

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
										if ( pProgress->eStatus == ESTATUS_CANCEL )
											return;

										const Poly &p=m_polys[*it];
										if (p.v[0]!=p.v[1] && p.v[0]!=p.v[2] && p.v[1]!=p.v[2]) // ignore degenerates
										{
											bool found=false;
											for (std::vector<idT>::const_iterator dit=tmpVtx.polys.begin(), dend=tmpVtx.polys.end(); dit!=dend; ++dit)
											{
												if ( pProgress->eStatus == ESTATUS_CANCEL )
													return;

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

					if ( pProgress->eStatus == ESTATUS_CANCEL )
						return;

					if (bestMove==-1)
						break;
#ifdef VISUAL_CHANGE_DEBUG
					dt=gEnv->pTimer->GetAsyncTime()-dt;
					CryLog("LODGen: Best move: %d %d->%d Error:%f Time:%fms\n", bestMove, m_moves[bestMove].from, m_moves[bestMove].to, minError, dt.GetMilliSeconds());
					dt=gEnv->pTimer->GetAsyncTime();
#endif
					pProgress->fProgress = (1.0f-(checkedMoves/(float)m_moves.size()) + pProgress->fCompleted) / pProgress->fParts;
					m_moveList.push_back(TakenMove(m_moves[bestMove].from, m_moves[bestMove].to, minError));
					Update(bestMove, minError);
					for (int view=0; view<m_numViews; view++)
					{
						if ( pProgress->eStatus == ESTATUS_CANCEL )
							return;

						m_views[view].ZeroErrorValues();
					}
#ifdef VISUAL_CHANGE_DEBUG
					dt=gEnv->pTimer->GetAsyncTime()-dt;
					CryLog("LODGen: Update Time:%fms\n", dt.GetMilliSeconds());
#endif
				}
				pProgress->fProgress = (pProgress->fCompleted + 1.0f) / pProgress->fParts;
			}

			void Render(void)
			{
				IRenderAuxGeom *pAuxGeom=gEnv->pRenderer->GetIRenderAuxGeom();
				if (pAuxGeom)
				{
					ColorB col0(255,0,255,255);
					ColorB col1(255,255,0,255);
					ColorB col2(0,255,255,255);
					static int maxMoves=0;
					std::vector<int> remap;
					maxMoves++;
					if (maxMoves>=m_moveList.size())
						maxMoves=0;
					remap.resize(m_vertices.size());
					for (int i=0; i<remap.size(); i++)
						remap[i]=i;
					for (int i=0; i<m_moveList.size() && i<maxMoves; i++)
						remap[m_moveList[i].from]=m_moveList[i].to;
					for (int i=0; i<remap.size(); i++)
					{
						int cur=i;
						while (remap[cur]!=cur)
							cur=remap[cur];
						remap[i]=cur;
					}
					for (std::vector<Tri>::iterator it=m_originalTris.begin(), end=m_originalTris.end(); it!=end; ++it)
					{
						int id[3];
						id[0]=remap[it->v[0]];
						id[1]=remap[it->v[1]];
						id[2]=remap[it->v[2]];
						if (id[0]!=id[1] && id[0]!=id[2] && id[1]!=id[2])
						{
							Vec3 pos0=m_vertices[id[0]].pos;
							Vec3 pos1=m_vertices[id[1]].pos;
							Vec3 pos2=m_vertices[id[2]].pos;
							pAuxGeom->DrawTriangle(pos0, col0, pos1, col1, pos2, col2);
						}
					}
				}
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
	};

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

		void CreateLodders(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutput *pReturnValues)
		{
			bool genIndexedMesh = false;
			if ( pReturnValues->pStatObj->GetRenderMesh() && !pReturnValues->pStatObj->GetIndexedMesh(false))
				genIndexedMesh = true;

			bool bIgnoreChildren = false;

			if (pReturnValues->pStatObj->GetIndexedMesh(genIndexedMesh))
			{
				VisualChange::VisualChangeCalculator * pLodder=new VisualChange::VisualChangeCalculator();
				if ( !pLodder )
				{
					CryLog("LODGen: Failed to allocate visual change calculator, possible out of memory problem");
					return;
				}

				pLodder->SetParameters(pInputParams);
				pLodder->FillData(pReturnValues);
				m_Lodders.push_back(pLodder);

				string rmTypeName(pReturnValues->pStatObj->GetRenderMesh()->GetTypeName());
				string checktype("StatObj_Merged");

				if ( rmTypeName.CompareNoCase(checktype) == 0 )
					bIgnoreChildren = true;

				m_progress.fParts++;
			}

			if(bIgnoreChildren)
				return;

			const int subobjCount = pReturnValues->pStatObj->GetSubObjectCount();
			for(int subobjIdx = 0; subobjIdx<subobjCount; ++subobjIdx)
			{
				IStatObj::SSubObject * pSubObj = pReturnValues->pStatObj->GetSubObject(subobjIdx);
				if (!pSubObj)
					continue;
				if (!pSubObj->pStatObj)
					continue;
				if (pSubObj->bShadowProxy)
					continue;
				if (pSubObj->nType != STATIC_SUB_OBJECT_MESH)
					continue;

				CLODGeneratorLib::SLODSequenceGenerationOutput * pSubReturnValues = new CLODGeneratorLib::SLODSequenceGenerationOutput();
				if (!pSubReturnValues)
				{
					CryLog("LODGen: Failed to allocate SubObjects Generation Output, possible out of memory problem");
					return;
				}

				pSubReturnValues->indices=NULL;
				pSubReturnValues->positions=NULL;
				pSubReturnValues->moveList=NULL;
				pSubReturnValues->numIndices=0;
				pSubReturnValues->numMoves=0;
				pSubReturnValues->numPositions=0;
				pSubReturnValues->pStatObj = pSubObj->pStatObj;
				pSubReturnValues->m_pSubObjectOutput.clear();
				pReturnValues->m_pSubObjectOutput.push_back(pSubReturnValues);
				CreateLodders(pInputParams,pSubReturnValues);
			}
		}

		bool ProcessLodder(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutput *pReturnValues)
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

					for(std::vector<VisualChange::VisualChangeCalculator *>::iterator lodder = m_Lodders.begin(), end = m_Lodders.end(); lodder != end; ++ lodder)
					{
						VisualChange::VisualChangeCalculator * pLodder = (*lodder);
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
		std::vector<VisualChange::VisualChangeCalculator *> m_Lodders;
	};
#endif // INCLUDE_LOD_GENERATOR

	///////////////////////////////////////////////////////////
	// External Interface
	///////////////////////////////////////////////////////////

	bool CLODGeneratorLib::GenerateLODSequence(const CLODGeneratorLib::SLODSequenceGenerationInputParams *pInputParams, CLODGeneratorLib::SLODSequenceGenerationOutput *pReturnValues, bool bProcessAsync)
	{
#ifdef INCLUDE_LOD_GENERATOR
		pReturnValues->indices=NULL;
		pReturnValues->positions=NULL;
		pReturnValues->moveList=NULL;
		pReturnValues->numIndices=0;
		pReturnValues->numMoves=0;
		pReturnValues->numPositions=0;
		pReturnValues->pStatObj = NULL;
		pReturnValues->m_pSubObjectOutput.clear();
		if (pInputParams->pInputMesh)
		{
			pReturnValues->pStatObj = pInputParams->pInputMesh;
			pReturnValues->pThread=new LODChainGenerateThread();
			return pReturnValues->pThread->ProcessLodder(pInputParams, pReturnValues);
		}
#endif
		return false;
	}

	bool CLODGeneratorLib::GetAsyncLODSequenceResults(CLODGeneratorLib::SLODSequenceGenerationOutput *pReturnValues, float *pProgress)
	{
#ifdef INCLUDE_LOD_GENERATOR
		if (pReturnValues->pThread)
		{
			if (!pReturnValues->pThread->CheckStatus(pProgress))
				return false;
			pReturnValues->pThread->WaitForCompletion();
			SAFE_DELETE(pReturnValues->pThread);
		}
#endif
		return true;
	}

	bool CLODGeneratorLib::FreeLODSequence(CLODGeneratorLib::SLODSequenceGenerationOutput *pReturnValues)
	{
#ifdef INCLUDE_LOD_GENERATOR
		while (!GetAsyncLODSequenceResults(pReturnValues, NULL));
		delete[] pReturnValues->indices;
		delete[] pReturnValues->positions;
		delete[] pReturnValues->moveList;
		pReturnValues->indices=NULL;
		pReturnValues->positions=NULL;
		pReturnValues->moveList=NULL;
		pReturnValues->numIndices=0;
		pReturnValues->numMoves=0;
		pReturnValues->numPositions=0;
		pReturnValues->pStatObj = NULL;
		pReturnValues->m_pSubObjectOutput.clear();
#endif
		return true;
	}

	IStatObj * CLODGeneratorLib::BuildStatObj(SLODSequenceGenerationOutput *pSequence, const CLODGeneratorLib::SLODGenerationInputParams *pInputParams)
	{
		bool genIndexedMesh = false;
		if ( pSequence->pStatObj->GetRenderMesh() && !pSequence->pStatObj->GetIndexedMesh(false))
			genIndexedMesh = true;

		IStatObj *pOutStatObj=NULL;
		if ( pSequence->pStatObj->GetIndexedMesh(genIndexedMesh) )
		{
			int numMoves=(int)floorf(pSequence->numMoves*(1.0f-pInputParams->nPercentage/100.0f)+0.5f);
			int newFaces=0;
			std::vector<vtx_idx> pNewIndices;
			std::vector<int> remap;
			remap.resize(pSequence->numPositions);
			for (int i=0; i<remap.size(); i++)
				remap[i]=i;
			for (int i=0; i<numMoves; i++)
				remap[pSequence->moveList[i].from]=pSequence->moveList[i].to;
			for (int i=0; i<remap.size(); i++)
			{
				int cur=i;
				while (remap[cur]!=cur)
					cur=remap[cur];
				remap[i]=cur;
			}
			for (vtx_idx *it=pSequence->indices, *end=&pSequence->indices[pSequence->numIndices]; it!=end; it+=3)
			{
				int id[3];
				id[0]=remap[*(it+0)];
				id[1]=remap[*(it+1)];
				id[2]=remap[*(it+2)];
				if (id[0]!=id[1] && id[0]!=id[2] && id[1]!=id[2])
				{
					pNewIndices.push_back((vtx_idx)id[0]);
					pNewIndices.push_back((vtx_idx)id[1]);
					pNewIndices.push_back((vtx_idx)id[2]);
					newFaces++;
				}
			}

			if (pInputParams->bUnwrap)
			{
				AutoUV uv;
				uv.Run(pSequence->positions, &pNewIndices[0], newFaces,eUV_ABF);

				pOutStatObj=uv.m_outMesh;
				pOutStatObj->AddRef();
			}
			else
			{
				CMesh *pMesh=new CMesh();
				std::vector<int> posRemap;
				std::vector<int> outVerts;
				posRemap.resize(pSequence->numPositions, -1);
				pMesh->SetIndexCount(pNewIndices.size());
				vtx_idx *pIndices=pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
				for (std::vector<vtx_idx>::iterator it=pNewIndices.begin(), end=pNewIndices.end(); it!=end; ++it)
				{
					if (posRemap[*it]==-1)
					{
						posRemap[*it]=outVerts.size();
						outVerts.push_back(*it);
					}
					*(pIndices++)=posRemap[*it];
				}
				pMesh->SetVertexCount(outVerts.size());
				Vec3* pVertices=pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);
				for (std::vector<int>::iterator it=outVerts.begin(), end=outVerts.end(); it!=end; ++it)
				{
					*(pVertices++)=pSequence->positions[*it];
				}
				SMeshSubset subset;
				subset.nNumIndices=pNewIndices.size();
				subset.nNumVerts=outVerts.size();
				subset.fRadius=100.0f;
				subset.fTexelDensity=1.0f;
				pMesh->m_subsets.push_back(subset);

				pOutStatObj=gEnv->p3DEngine->CreateStatObjOptionalIndexedMesh(false);
				IIndexedMesh *pIdxMesh=pOutStatObj->GetIndexedMesh(false);
				if (!pIdxMesh)
					pIdxMesh=pOutStatObj->CreateIndexedMesh();
				pIdxMesh->SetMesh(*pMesh);
				delete pMesh;
			}
		}
		else
		{
			pOutStatObj=gEnv->p3DEngine->CreateStatObjOptionalIndexedMesh(false);
		}

		pOutStatObj->SetBBoxMin(pSequence->pStatObj->GetBoxMin());
		pOutStatObj->SetBBoxMax(pSequence->pStatObj->GetBoxMax());
		pOutStatObj->SetGeoName(pSequence->pStatObj->GetGeoName());

		for ( std::vector<SLODSequenceGenerationOutput*>::iterator item = pSequence->m_pSubObjectOutput.begin(), end = pSequence->m_pSubObjectOutput.end(); item != end; ++item)
		{
			if ( IStatObj * pSubStatObj = BuildStatObj((*item), pInputParams) )
			{
				pOutStatObj->AddSubObject(pSubStatObj);
			}
		}

		pOutStatObj->Invalidate();
		return pOutStatObj;
	}

	bool CLODGeneratorLib::GenerateLOD(const CLODGeneratorLib::SLODGenerationInputParams *pInputParams, CLODGeneratorLib::SLODGenerationOutput *pReturnValues)
	{
#ifdef INCLUDE_LOD_GENERATOR
		SLODSequenceGenerationOutput *pSequence=pInputParams->pSequence;
		if (pSequence && pSequence->pStatObj && ( pReturnValues->pStatObj = BuildStatObj(pSequence,pInputParams) ) )
			return true;
#endif
		pReturnValues->pStatObj=NULL;
		return false;
	}

	IStatObj* CLODGeneratorLib::GenerateUVs(Vec3* pPositions, vtx_idx *pIndices, int nNumFaces)
	{			
		AutoUV uv;
		uv.Run(pPositions, pIndices, nNumFaces,eUV_ABF);
		IStatObj * pOutStatObj = uv.m_outMesh;
		pOutStatObj->AddRef();
		return pOutStatObj;
	}

	void CLODGeneratorLib::CancelLODGeneration(CLODGeneratorLib::SLODSequenceGenerationOutput *pReturnValues)
	{
		if (pReturnValues)
		{
			if (pReturnValues->pThread)
			{
				pReturnValues->pThread->Cancel();
				gEnv->pThreadManager->JoinThread(pReturnValues->pThread, eJM_Join);

				delete pReturnValues->pThread;
				pReturnValues->pThread = NULL;
			}

			FreeLODSequence(pReturnValues);
		}
	}
}
