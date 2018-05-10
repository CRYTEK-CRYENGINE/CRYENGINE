// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorUVUnwrap.h"
#include <Cry3DEngine/IIndexedMesh.h>

#define LODGEN_ASSERT(condition) { if (!(condition)) __debugbreak(); }

namespace LODGenerator
{
	
	AutoUV::AutoUV()
	{
#ifdef ABF
		textureAtlasGenerator = new LODGenerator::CAtlasGenerator();
#endif

	}

	AutoUV::~AutoUV()
	{
// 		if (m_outMeshList.size() != 0)
// 		{
// 			for (int i=0;i<m_outMeshList.size();i++)
// 			{
// 				delete m_outMeshList[i];
// 			}
// 		}
		m_outMeshList.clear();

#ifdef ABF
		delete textureAtlasGenerator;
#endif
	}

	void AutoUV::Run(Vec3 *pVertices, vtx_idx *indices, int faces,EUVWarpType uvWarpType)
	{
		if (uvWarpType == eUV_Common)
		{
			Prepare(pVertices, indices, faces);
			Unwrap();
		}
		else if (uvWarpType == eUV_ABF)
		{
#ifdef ABF
			textureAtlasGenerator->setSurface(LODGenerator::CTopologyTransform::TransformToTopology(pVertices,indices,faces));
			textureAtlasGenerator->testGenerate_texture_atlas();

			std::vector<SAutoUVFinalVertex> vertices;
			std::vector<int> indices;

			m_outMesh=gEnv->p3DEngine->CreateStatObjOptionalIndexedMesh(false);
			m_outMesh->SetGeoName("PreLOD");
			textureAtlasGenerator->surface()->compute_vertex_normals();
			textureAtlasGenerator->surface()->compute_vertex_tangent(true);
			LODGenerator::CTopologyTransform::FillMesh(textureAtlasGenerator->surface(),m_outMesh);
			m_outMesh->Invalidate();
#endif
		}

	}


	void AutoUV::Prepare(Vec3 *pVertices, vtx_idx *indices, int faces)
	{
		std::vector<int> newIndices;
		for (int i=0; i<faces*3; i++)
		{
			Vec3 *v=&pVertices[indices[i]];
			int idx=-1;
			for (std::vector<SAutoUVVertex>::iterator it=m_vertices.begin(), end=m_vertices.end(); it!=end; ++it)
			{
				if (*v==it->pos)
				{
					idx=(int)(&*it-&m_vertices[0]);
					break;
				}
			}
			if (idx==-1)
			{
				SAutoUVVertex vert;
				vert.pos=*v;
				vert.projected=Vec2(0,0);
				idx=m_vertices.size();
				m_vertices.push_back(vert);
			}
			newIndices.push_back(idx);
		}

		// Remove zero area faces.
		for (int i=0; i<faces*3; i+=3)
		{
			if(!(newIndices[i+0]!=newIndices[i+1] && newIndices[i+0]!=newIndices[i+2] && newIndices[i+1]!=newIndices[i+2]))
			{
				newIndices[i+0] = newIndices[faces*3 - 3];
				newIndices[i+1] = newIndices[faces*3 - 2];
				newIndices[i+2] = newIndices[faces*3 - 1];
				faces--;
				i-=3;
			}
		}

		// Split vertices if edge is hard
		float angleLimit=0.70f; // ~45 degrees
		std::vector<SAutoUVVertexNormal> normals;
		int newVerts=m_vertices.size();
		normals.resize(m_vertices.size());
		for (int i=0; i<faces; i++)
		{
			Vec3 n=(m_vertices[newIndices[3*i+2]].pos-m_vertices[newIndices[3*i+0]].pos).Cross(m_vertices[newIndices[3*i+1]].pos-m_vertices[newIndices[3*i+0]].pos).normalize();
			for (int k=0; k<3; k++)
			{
				SAutoUVVertexNormal &vertN=normals[newIndices[3*i+k]];
				if (vertN.weighting==0.0f)
				{
					vertN.normal=n;
					vertN.weighting=1.0f;
				}
				else
				{
					if (vertN.normal.Dot(n)>angleLimit)
					{
						vertN.normal=vertN.normal*vertN.weighting+n;
						vertN.normal.Normalize();
						vertN.weighting++;
					}
					else
					{
						// split vertex
						SAutoUVVertex &v=m_vertices[newIndices[3*i+k]];
						bool bFound=false;
						for (int j=newVerts; j<m_vertices.size(); j++)
						{
							if (m_vertices[j].pos==v.pos && normals[j].normal.Dot(n)>angleLimit)
							{
								SAutoUVVertexNormal &newVertN=normals[j];
								newVertN.normal=newVertN.normal*newVertN.weighting+n;
								newVertN.normal.Normalize();
								newVertN.weighting++;
								newIndices[3*i+k]=j;
								bFound=true;
								break;
							}
						}
						if (!bFound)
						{
							SAutoUVVertex newVert=v;
							SAutoUVVertexNormal newVertN;
							newVertN.normal=n;
							newVertN.weighting=1.0f;
							newIndices[3*i+k]=m_vertices.size();
							m_vertices.push_back(newVert);
							normals.push_back(newVertN);
						}
					}
				}
			}
		}

		for (int i=0; i<m_vertices.size(); i++)
		{
			m_vertices[i].smoothedNormal=normals[i].normal;
		}

		for (int i=0; i<faces*3; i+=3)
		{
			SAutoUVFace f;
			f.v[0]=&m_vertices[newIndices[i+0]];
			f.v[1]=&m_vertices[newIndices[i+1]];
			f.v[2]=&m_vertices[newIndices[i+2]];
			Vec3 n=(f.v[1]->pos-f.v[0]->pos).Cross(f.v[2]->pos-f.v[0]->pos);
			for (int k=0; k<3; k++)
			{
				Vec3 edge1=f.v[(k+2)%3]->pos-f.v[k]->pos;
				Vec3 edge2=f.v[(k+1)%3]->pos-f.v[k]->pos;
				Vec3 right=edge1.Cross(n).normalize();
				edge1.Normalize();
				edge2.Normalize();
				f.a[k]=atan2f(edge2.Dot(right), edge2.Dot(edge1));
			}
			f.bDone=false;
			m_faces.push_back(f);
		}

		int nSplitEdgesCount = 0;
		m_edges.reserve(m_faces.size()*3);
		for (std::vector<SAutoUVFace>::iterator fit=m_faces.begin(), fend=m_faces.end(); fit!=fend; ++fit)
		{
			SAutoUVFace *f=&*fit;
			for (int k=0; k<3; k++)
			{
				SAutoUVVertex *a=f->v[k];
				SAutoUVVertex *b=f->v[(k+2)%3];
				bool bFound=false;
				for (std::vector<SAutoUVEdge>::iterator it=m_edges.begin(), end=m_edges.end(); it!=end; ++it)
				{
					if ((it->v[0]==a && it->v[1]==b) || (it->v[1]==a && it->v[0]==b))
					{
						if (it->faces[1])
						{
							nSplitEdgesCount++;
							continue;
						}
						it->faces[1]=f;
						f->edges[k]=&*it;
						bFound=true;
						break;
					}
				}
				if (!bFound)
				{
					m_edges.push_back(SAutoUVEdge());
					SAutoUVEdge &e=m_edges.back();
					e.v[0]=a;
					e.v[1]=b;
					e.faces[0]=f;
					e.faces[1]=NULL;
					f->edges[k]=&e;
				}
			}
		}
		if(nSplitEdgesCount > 0)
		{
			CryLog("AutoUV: Input mesh has an %d edges shared by more than two faces. They were split.\n",nSplitEdgesCount);
		}
		CryLog("AutoUV: Total faces:%d vertices:%d edges:%d\n", faces, m_vertices.size(), m_edges.size());
	}

	Vec2 AutoUV::Project(SAutoUVStackEntry &se, SAutoUVFace *f, SAutoUVEdge *e)
	{
		for (int i=0; i<3; i++)
		{
			if (f->edges[i]==e)
			{
				float a=f->a[i];
				float sa=sinf(a);
				float ca=cosf(a);
				int n=(i==2)?0:(i+1);
				SAutoUVEdge *e2=f->edges[n];
				int sharedE=(e->v[1]==e2->v[0] || e->v[1]==e2->v[1])?1:0;
				int sharedF=(e->v[sharedE]==e2->v[1])?1:0;
				LODGEN_ASSERT(e2->v[sharedF]==e->v[sharedE]);
				Vec2 edge=se.projected[1-sharedE]-se.projected[sharedE];
				edge.Normalize();
				edge*=e2->v[1]->pos.GetDistance(e2->v[0]->pos);
				return se.projected[sharedE]+Vec2(ca*edge.x-sa*edge.y, ca*edge.y+sa*edge.x);
			}
		}
		LODGEN_ASSERT(false);
		return Vec2(0,0);
	}

	void AutoUV::SquarePacker(int &w, int &h, std::vector<SAutoUVSquare> &squares)
	{
		bool bDone=false;
		// Sort based on area
		std::sort(squares.begin(), squares.end());
		while (!bDone)
		{
			std::vector<int> free;
			free.resize(h, w);
			bDone=true;
			for (std::vector<SAutoUVSquare>::iterator it=squares.begin(), end=squares.end(); it!=end; ++it)
			{
				SAutoUVSquare sq=*it;
				int bestWaste=w*h+1;
				int bestX=-1;
				int bestRotation=-1;
				std::vector<int>::iterator bestStart=free.end();
				for (int i=0; i<2; i++)
				{
					std::vector<int>::iterator start=free.end();
					int waste=0;
					int minX=0;
					for (std::vector<int>::iterator fit=free.begin(); fit!=free.end(); ++fit)
					{
						if (*fit>=sq.w)
						{
							if (start==free.end())
							{
								start=fit;
								minX=*fit;
								waste=0;
							}
							else if (*fit<minX)
							{
								waste+=(int)((minX-*fit)*(fit-start));
								minX=*fit;
							}
							else if (*fit>minX)
							{
								waste+=(int)(*fit-minX);
							}
							if (fit-start+1==sq.h)
							{
								if (waste<bestWaste)
								{
									bestWaste=waste;
									bestX=minX;
									bestStart=start;
									bestRotation=i;
									if (waste==0)
										break;
								}
								fit=start;	// Will start at again at the next line
								start=free.end();
							}
						}
						else
						{
							start=free.end();
						}
					}
					if (i==0 && bestWaste==0)
						break;
					sq.Rotate();
				}
				if (bestStart!=free.end())
				{
					if (bestRotation==1)
					{
						sq.Rotate();
						it->Rotate();
						it->RotateTris(m_tris);
					}
					it->x=w-bestX;
					it->y=(int)(bestStart-free.begin());
					for (std::vector<int>::iterator fit=bestStart; fit!=bestStart+sq.h; ++fit)
					{
						*fit=bestX-sq.w;
					}
				}
				else if (h<w)
				{
					h=std::min(h+128, w);
					bDone=false;
					break;
				}
				else
				{
					w+=128;
					h+=128;
					bDone=false;
					break;
				}
			}
		}
		CryLog("AutoUV: Fitted into %d/%d\n", w, h);
	}

	void AutoUV::Unwrap(void)
	{
		int done=0;
		float finalArea=0.0f;
		float finalPolyArea=0.0f;
		float uvScale=100.0f;	// 100 "texels" per meter. Used by the expanding rasteriser to detect overlap
		int outW=32, outH=32;

		m_squares.clear();
		for (std::vector<SAutoUVFace>::iterator fit=m_faces.begin(), fend=m_faces.end(); fit!=fend; ++fit)
		{
			fit->bDone=false;
		}

		float xoff=0.0f;
		while (true)
		{
			SAutoUVExpandingRasteriser rasteriser(1.0f/uvScale);
			std::vector<SAutoUVStackEntry> stack;
			SAutoUVStackEntry root;
			float totalArea=0.0f;
			float mx=0.0f,Mx=0.0f;
			float my=0.0f,My=0.0f;

			SAutoUVEdge *startingEdge=NULL;
			float longestEdge=-1.0f;
			for (std::vector<SAutoUVFace>::iterator fit=m_faces.begin(), fend=m_faces.end(); fit!=fend; ++fit)
			{
				if (!fit->bDone)
				{
					for (int i=0; i<3; i++)
					{
						float dist=fit->edges[i]->v[0]->pos.GetSquaredDistance(fit->edges[i]->v[1]->pos);
						if (dist>longestEdge)
						{
							longestEdge=dist;
							startingEdge=fit->edges[(i+1)%3];
						}
					}
				}
			}
			if (startingEdge==NULL)
				break;

			root.edge=startingEdge;
			root.projected[0]=Vec2(xoff, 0.0f);
			root.projected[1]=Vec2(xoff+startingEdge->v[1]->pos.GetDistance(startingEdge->v[0]->pos), 0.0f);
			startingEdge->v[0]->projected=root.projected[0];
			startingEdge->v[1]->projected=root.projected[1];
			stack.push_back(root);
			mx=root.projected[0].x;
			Mx=root.projected[1].x;

			int lastDone=done;
			while (!stack.empty())
			{
				float bestRatio=-1.0f;
				int bestFace=-1;
				std::vector<SAutoUVStackEntry>::iterator bestEntry=stack.end();
				for (std::vector<SAutoUVStackEntry>::iterator it=stack.begin(), end=stack.end(); it!=end; ++it)
				{
					SAutoUVStackEntry &se=*it;
					SAutoUVEdge *e=se.edge;
					bool bValid=false;
					for (int f=0; f<2; f++)
					{
						if (e->faces[f] && !e->faces[f]->bDone)
						{
							float nmx,nMx,nmy,nMy;
							SAutoUVFace *face=e->faces[f];
							Vec2 newPos=Project(se, e->faces[f], e);

							bool bHit=false;
							for (int i=0; i<3; i++)
							{
								if (face->edges[i]==e)
								{
									int n=(i==2)?0:(i+1);
									SAutoUVEdge *e2=face->edges[n];
									int sharedE=(e->v[1]==e2->v[0] || e->v[1]==e2->v[1])?1:0;
									bHit=rasteriser.RasteriseTriangle(se.projected[1-sharedE], newPos, se.projected[sharedE], false);
									break;
								}
							}
							if (bHit)
							{
								continue;
							}

							float area=0.5f*(face->v[2]->pos-face->v[0]->pos).Cross(face->v[1]->pos-face->v[0]->pos).GetLength();
							bValid=true;
							nmx=min(mx, newPos.x);
							nMx=max(Mx, newPos.x);
							nmy=min(my, newPos.y);
							nMy=max(My, newPos.y);
							float packingRatio;
							if (((nMx-nmx)*(nMy-nmy))>0.0f)
								packingRatio=(totalArea+area)/((nMx-nmx)*(nMy-nmy));
							else
								packingRatio=1.0f;
							if (packingRatio>bestRatio)
							{
								bestRatio=packingRatio;
								bestFace=f;
								bestEntry=it;
							}
						}
					}
					if (!bValid)
					{
						stack.erase(it);
						it--;
						if (bestEntry==end)
							bestEntry=stack.end();
						end=stack.end();
					}
				}

				if (bestEntry!=stack.end() && ((done-lastDone)<3 || bestRatio>0.4f))
				{
					SAutoUVStackEntry &se=*bestEntry;
					SAutoUVEdge *e=se.edge;
					SAutoUVFace *face=e->faces[bestFace];
					e->v[0]->projected=se.projected[0];
					e->v[1]->projected=se.projected[1];
					Vec2 newPos=Project(se, face, e);

					for (int i=0; i<3; i++)
					{
						if (face->edges[i]==e)
						{
							int n=(i==2)?0:(i+1);
							SAutoUVEdge *e2=face->edges[n];
							int sharedE=(e->v[1]==e2->v[0] || e->v[1]==e2->v[1])?1:0;
							int sharedF=(e->v[sharedE]==e2->v[1])?1:0;
							e2->v[1-sharedF]->projected=newPos;
							m_tris.push_back(SAutoUVTri(se.projected[1-sharedE], se.projected[sharedE], newPos, e->v[1-sharedE]->pos, e->v[sharedE]->pos, e2->v[1-sharedF]->pos, e->v[1-sharedE], e->v[sharedE], e2->v[1-sharedF]));
							if (rasteriser.RasteriseTriangle(se.projected[1-sharedE], newPos, se.projected[sharedE], true))
							{
								LODGEN_ASSERT(false);
							}
							face->bDone=true;
							done++;
							break;
						}
					}

					mx=min(mx, newPos.x);
					Mx=max(Mx, newPos.x);
					my=min(my, newPos.y);
					My=max(My, newPos.y);
					float polyArea=0.5f*(face->v[2]->pos-face->v[0]->pos).Cross(face->v[1]->pos-face->v[0]->pos).GetLength();
					totalArea+=polyArea;
					finalPolyArea+=polyArea;
					if (e->faces[0]->bDone && (!e->faces[1] || e->faces[1]->bDone))
					{
						stack.erase(bestEntry);
					}

					for (int i=0; i<3; i++)
					{
						if (face->edges[i]!=e)
						{
							SAutoUVStackEntry entry;
							entry.edge=face->edges[i];
							entry.projected[0]=face->edges[i]->v[0]->projected;
							entry.projected[1]=face->edges[i]->v[1]->projected;
							stack.push_back(entry);
						}
					}
				}
				else
				{
					if (lastDone==done)
					{
						CryLogAlways("ERROR: AutoUV couldn't deal with unwrap at all for some weird reason %p\n", startingEdge);
						startingEdge->faces[0]->bDone=true;
						if (startingEdge->faces[1])
							startingEdge->faces[1]->bDone=true;
					}
					// All edges in stack are done already
					break;
				}
			}
			finalArea+=(My-my)*(Mx-mx);
			m_squares.push_back(SAutoUVSquare((int)ceilf((Mx-mx)*uvScale), (int)ceilf((My-my)*uvScale), mx, my, lastDone, done));
			SAutoUVSquare &newSquare=m_squares.back();
			if (newSquare.h>newSquare.w)
			{
				newSquare.Rotate();
				newSquare.RotateTris(m_tris);
			}
		}
		float fullArea=0.0f;
		for (std::vector<SAutoUVSquare>::iterator it=m_squares.begin(), end=m_squares.end(); it!=end; ++it)
		{
			outW=max(outW, it->w);
			fullArea+=it->w*it->h;
		}
		if (outW<sqrtf(fullArea))
		{
			outW=(int)ceilf(sqrtf(fullArea));
			outH=outW;
		}
		else
		{
			outH=(int)ceilf(fullArea/outW);
		}
		CryLog("AutoUV: Total unwrapped:%d (Edge:%f) Area(%f/%fm2 %f%%) %d\n", done, xoff, finalPolyArea, finalArea, 100.0f*finalPolyArea/(float)finalArea, m_squares.size());
		SquarePacker(outW, outH, m_squares);
		for (std::vector<SAutoUVSquare>::iterator it=m_squares.begin(), end=m_squares.end(); it!=end; ++it)
		{
			Vec2 offset;
			offset.x=it->x-uvScale*it->mx;
			offset.y=it->y-uvScale*it->my;
			for (std::vector<SAutoUVTri>::iterator tit=m_tris.begin()+it->s, tend=m_tris.begin()+it->e; tit!=tend; ++tit)
			{
				for (int i=0; i<3; i++)
					tit->v[i]=offset+uvScale*tit->v[i];
			}
		}
		m_outVertices.clear();
		m_outIndices.clear();
		for (std::vector<SAutoUVTri>::iterator it=m_tris.begin(), end=m_tris.end(); it!=end; ++it)
		{
			for (int i=0; i<3; i++)
			{
				std::vector<SAutoUVFinalVertex>::iterator vit=m_outVertices.begin(), vend=m_outVertices.end();
				for (; vit!=vend; ++vit)
				{
					if (vit->orig==it->orig[i] && (vit->uv-it->v[i]).GetLength2()<1.0f)
					{
						break;
					}
				}
				if (vit!=vend)
				{
					m_outIndices.push_back((int)(vit-m_outVertices.begin()));
				}
				else
				{
					SAutoUVFinalVertex newVert;
					newVert.pos=it->pos[i];
					newVert.uv=it->v[i];
					newVert.orig=it->orig[i];
					m_outIndices.push_back(m_outVertices.size());
					m_outVertices.push_back(newVert);
				}
			}
		}
		for (std::vector<SAutoUVFinalVertex>::iterator it=m_outVertices.begin(), end=m_outVertices.end(); it!=end; ++it)
		{
			Vec3 tangent(0,0,0);
			Vec3 bitangent(0,0,0);
			bool bFound=false;
			for (std::vector<SAutoUVTri>::iterator tit=m_tris.begin(), tend=m_tris.end(); tit!=tend; ++tit)
			{
				for (int i=0; i<3; i++)
				{
					if (it->orig==tit->orig[i] && it->uv==tit->v[i])
					{
						Vec3 edge1=tit->pos[1]-tit->pos[0];
						edge1.Normalize();
						Vec3 edge2=edge1.Cross(it->orig->smoothedNormal);
						edge2.Normalize();
						Vec2 uvEdge1=tit->v[1]-tit->v[0];
						Vec2 uvEdge2=uvEdge1.rot90cw();
						Vec2 u(1.0f, 0.0f);
						Vec2 v(0.0f, 1.0f);
						tangent+=uvEdge1.Dot(u)*edge1+uvEdge2.Dot(u)*edge2;
						bitangent+=uvEdge1.Dot(v)*edge1+uvEdge2.Dot(v)*edge2;
						bFound=true;
						break;
					}
				}
			}
			if (bFound)
			{
				tangent=bitangent.Cross(it->orig->smoothedNormal);
				tangent.Normalize();
				bitangent=it->orig->smoothedNormal.Cross(tangent);
				bitangent.Normalize();
				if (fabsf(bitangent.Dot(tangent))>0.05f)
				{
					CryLog("AutoUV: Not at right angles %f\n", bitangent.Dot(tangent));
				}
				if (it->orig->smoothedNormal.Dot(tangent.Cross(bitangent))<0.9f)
				{
					CryLog("AutoUV: Doesn't produce correct normal %f!=%f\n", it->orig->smoothedNormal.Dot(tangent.Cross(bitangent)), 1.0f);
				}
			}
			else
			{
				bitangent=Vec3(1,0,0);
				tangent=Vec3(0,1,0);
			}
			it->bitangent=bitangent;
			it->tangent=tangent;
			it->normal=it->orig->smoothedNormal;
			if (fabsf(it->bitangent.Dot(it->normal))>fabsf(it->tangent.Dot(it->normal)))
			{
				it->bitangent=it->normal.Cross(it->tangent).normalize();
				it->tangent=it->bitangent.Cross(it->normal).normalize();
			}
			else
			{
				it->tangent=it->bitangent.Cross(it->normal).normalize();
				it->bitangent=it->normal.Cross(it->tangent).normalize();
			}

			bitangent=it->bitangent;
			tangent=it->tangent;
			it->normal=-it->normal;

			if (it->tangent.Cross(it->bitangent).Dot(it->normal) < 0)
			{
				it->bitangent=tangent;
				it->tangent=bitangent;
			}

			it->uv.x/=(float)(outW-1);
			it->uv.y/=(float)(outH-1);
		}

		std::vector<SAutoUVFinalVertex> vertices;
		std::vector<int> indices;
		std::vector<int> used;
		used.resize(m_outVertices.size());
		vertices.resize(65536);
		indices.resize(m_outIndices.size());
		int i=0;
		while (i<m_outIndices.size())
		{
			memset(&used[0], 0xFF, used.size()*sizeof(used[0]));
			vertices.clear();
			indices.clear();
			int numVerts=0;
			for (; i<m_outIndices.size() && numVerts<65536-3; i+=3)
			{
				for (int k=0; k<3; k++)
				{
					if (used[m_outIndices[i+k]]==-1)
					{
						vertices.push_back(m_outVertices[m_outIndices[i+k]]);
						used[m_outIndices[i+k]]=numVerts++;
					}
					indices.push_back(used[m_outIndices[i+k]]);
				}
			}
			m_outMeshList.push_back(MakeMesh(vertices, indices));
		}
	}

	CMesh* AutoUV::MakeMesh(std::vector<SAutoUVFinalVertex> &vertices, std::vector<int> &indices)
	{
		CMesh *outMesh=new CMesh();
		outMesh->SetIndexCount(indices.size());
		outMesh->SetVertexCount(vertices.size());
		outMesh->ReallocStream(CMesh::TEXCOORDS, vertices.size());
		outMesh->ReallocStream(CMesh::TANGENTS, vertices.size());

		AABB bb(AABB::RESET);

		Vec3* pVertices0=outMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);
		SMeshTexCoord *pTexCoords0=outMesh->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);
		SMeshTangents *pOutTangents=outMesh->GetStreamPtr<SMeshTangents>(CMesh::TANGENTS);
		Vec3* pVertices=pVertices0;
		SMeshTexCoord *pTexCoords=pTexCoords0;
		for (std::vector<SAutoUVFinalVertex>::iterator vit=vertices.begin(), vend=vertices.end(); vit!=vend; ++vit)
		{
			bb.Add(vit->pos);

			*pVertices++    = vit->pos;
			*pOutTangents++ = SMeshTangents(vit->tangent, vit->bitangent, vit->normal);
			*pTexCoords++   = SMeshTexCoord(vit->uv.x, vit->uv.y);
		}
		vtx_idx *pIndices0=outMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
		vtx_idx *pIndices=pIndices0;
		for (std::vector<int>::iterator it=indices.begin(), end=indices.end(); it!=end; ++it)
		{
			*(pIndices++)=(vtx_idx)*it;
		}

		float area=0.0f;
		float tarea=0.0f;
		for (int i=0; i<indices.size(); i+=3)
		{
			int idx0=pIndices0[i+0];
			int idx1=pIndices0[i+1];
			int idx2=pIndices0[i+2];
			Vec3 e0=pVertices0[idx1]-pVertices0[idx0];
			Vec3 e1=pVertices0[idx2]-pVertices0[idx0];
			Vec2 t0 = pTexCoords0[idx0].GetUV();
			Vec2 t1 = pTexCoords0[idx1].GetUV();
			Vec2 t2 = pTexCoords0[idx2].GetUV();
			Vec2 ta=Vec2(t1.x-t0.x, t1.y-t0.y);
			Vec2 tb=Vec2(t2.x-t0.x, t2.y-t0.y);
			area+=e0.Cross(e1).len();
			tarea+=ta.Cross(tb);
		}

		SMeshSubset subset;
		subset.nFirstIndexId=0;
		subset.nNumIndices=indices.size();
		subset.nFirstVertId=0;
		subset.nNumVerts=vertices.size();
		subset.nMatID=0;
		subset.nMatFlags=0;
		subset.fRadius=100.0f;
		if (area!=0.0f)
			subset.fTexelDensity=tarea/area;
		else
			subset.fTexelDensity=1.0f;
		outMesh->m_subsets.push_back(subset);

		return outMesh;
	}

}