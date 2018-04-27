// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace LODGenerator
{
	enum EStatus
	{
		ESTATUS_RUNNING,
		ESTATUS_CANCEL
	};

	struct sProgress
	{
		sProgress()
		{
			fProgress = 0;
			eStatus = ESTATUS_RUNNING;
			fParts = 0;
			fCompleted = 0;
		}

		float fProgress;
		EStatus eStatus;
		float fParts;
		float fCompleted;
	};

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
								int count = (int)(rit - begin);
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


};