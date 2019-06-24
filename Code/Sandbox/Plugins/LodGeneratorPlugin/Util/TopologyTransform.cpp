#include "StdAfx.h"
#include "TopologyTransform.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include "TopologyBuilder.h"

namespace LODGenerator
{
	CTopologyGraph* CTopologyTransform::TransformToTopology(Vec3 *pVertices, vtx_idx *indices, int faces)
	{
		if (pVertices == NULL || indices == NULL)
			return NULL;

		std::vector<Vec3> verticeList;
		std::vector<int> newIndices;
		for (int i=0; i<faces*3; i++)
		{
			Vec3 *v=&pVertices[indices[i]];
			int idx=-1;
			for (std::vector<Vec3>::iterator it=verticeList.begin(), end=verticeList.end(); it!=end; ++it)
			{
				if (*v==*it)
				{
					idx=(int)(&*it-&verticeList[0]);
					break;
				}
			}
			if (idx==-1)
			{
				Vec3 vert;
				vert = *v;
				idx= verticeList.size();
				verticeList.push_back(vert);
			}
			newIndices.push_back(idx);
		}


		CTopologyGraph* pMap = new CTopologyGraph();
		CTopologyBuilder builder(pMap);

		builder.begin_surface();
		for (int i=0;i<verticeList.size();i++)
		{
			builder.add_vertex(verticeList[i]);
		}

		for (int i=0;i<faces;i++)
		{
			int index = -1;
			builder.begin_facet();
			for (int j=0;j<3;j++)
			{
				index = newIndices[ j + 3*i];
				builder.add_vertex_to_facet(index) ;
			}
			builder.end_facet();
		}
		builder.end_surface();

		pMap->compute_normals();
		pMap->compute_vertex_tangent();

		return pMap;
	}


	CTopologyGraph* CTopologyTransform::TransformToTopology(IStatObj *pObject,bool bCalNormal)
	{
		IIndexedMesh* pIIndexedMesh = pObject->GetIndexedMesh(true);
		pIIndexedMesh->RestoreFacesFromIndices();

		int posCount,normalCount,tangentCount;
		Vec3* const positions = pIIndexedMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::POSITIONS,&posCount);
		Vec3* const normals = pIIndexedMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::NORMALS,&normalCount);
		SMeshTangents *pOutTangents = pIIndexedMesh->GetMesh()->GetStreamPtr<SMeshTangents>(CMesh::TANGENTS,&tangentCount);

		int texCount,faceCount,indexCount;
		SMeshTexCoord* const texcoords = pIIndexedMesh->GetMesh()->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS,&texCount);
		SMeshFace* const faces = pIIndexedMesh->GetMesh()->GetStreamPtr<SMeshFace>(CMesh::FACES,&faceCount);
		vtx_idx *pIndices= pIIndexedMesh->GetMesh()->GetStreamPtr<vtx_idx>(CMesh::INDICES,&indexCount);

		ASSERT(posCount == normalCount == tangentCount);

		if (posCount == 0)
			return NULL;

		CTopologyGraph* pMap = new CTopologyGraph();
		CTopologyBuilder builder(pMap);

		builder.begin_surface();
		for (int i=0;i<posCount;i++)
		{
			builder.add_vertex(positions[i]);
		}

		for (int i=0;i<texCount;i++)
		{
			builder.add_tex_vertex(texcoords[i].GetUV()) ;
		}
		for (int i=0;i<faceCount;i++)
		{
			int index = -1;
			builder.begin_facet();
			for (int j=0;j<3;j++)
			{
				index = faces[i].v[j];
				builder.add_vertex_to_facet(index);
				builder.set_corner_tex_vertex(index);
			}
			builder.end_facet();
		}
		builder.end_surface(false);

		if (bCalNormal)
		{
			pMap->compute_normals();
			pMap->compute_vertex_tangent();
		}

		if(!bCalNormal)
		{
			for (int i=0;i<posCount;i++)
			{
				CVertex* vertex =  builder.vertex(i);
				if(vertex == NULL)
					continue;
				std::shared_ptr<CTexVertex>& texVertex = vertex->halfedge()->tex_vertex();

				if (texVertex == NULL)
					continue;

				if(normals != NULL)
				{
					texVertex->set_normal(normals[i]);
				}

				if(pOutTangents != NULL)
				{
					Vec3 bn,t;
					Vec4sf othert, otherb;
					pOutTangents[i].ExportTo(othert, otherb);
					PackingSNorm::tPackB2F(othert,t);
					PackingSNorm::tPackB2F(otherb,bn);

					texVertex->set_tangent(t);
					texVertex->set_binormal(bn);
				}

			}
		}	

		builder.clear();

		return pMap;
	}

	void CTopologyTransform::FillMesh( CTopologyGraph* pPolygon, IStatObj *pObject )
	{
		IIndexedMesh *pMesh = pObject->GetIndexedMesh();
		if (!pMesh)
			pMesh=pObject->CreateIndexedMesh();

		int vertexCount = pPolygon->size_of_vertices();
		int faceCount = pPolygon->size_of_facets();

		pMesh->FreeStreams();
		pMesh->SetVertexCount(vertexCount);
		pMesh->SetFaceCount(faceCount);
		pMesh->SetIndexCount(faceCount*3);
		pMesh->SetTexCoordCount(vertexCount);
		pMesh->SetTangentCount(vertexCount);

		Vec3* const positions = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::POSITIONS);
		Vec3* const normals = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::NORMALS);
		SMeshTangents *pOutTangents = pMesh->GetMesh()->GetStreamPtr<SMeshTangents>(CMesh::TANGENTS);

		SMeshTexCoord* const texcoords = pMesh->GetMesh()->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);
		SMeshFace* const faces = pMesh->GetMesh()->GetStreamPtr<SMeshFace>(CMesh::FACES);
		vtx_idx *pIndices= pMesh->GetMesh()->GetStreamPtr<vtx_idx>(CMesh::INDICES);

		std::map<CVertex*,int> vertex_id;

		int i = 0;
		for(CTopologyGraph::Vertex_iterator it=(pPolygon)->vertices_begin();it!=(pPolygon)->vertices_end(); it++,i++)
		{
			Vec3d vetex = (*it)->point();
			CHalfedge* jt = (*it)->halfedge() ;
			Vec3d normal = jt->tex_vertex()->normal();
			Vec2d st = jt->tex_coord() ;
			Vec3d tangent = jt->tex_vertex()->tangent();
			Vec3d binormal = jt->tex_vertex()->binormal();

			positions[i] = Vec3d(vetex.x,vetex.y,vetex.z);
			normals[i] = Vec3d(normal.x,normal.y,normal.z);
			texcoords[i] = SMeshTexCoord(st);
			pOutTangents[i]  = SMeshTangents(Vec4sf(PackingSNorm::tPackF2B(tangent.x), PackingSNorm::tPackF2B(tangent.y), PackingSNorm::tPackF2B(tangent.z), PackingSNorm::tPackS2B(1))
				,Vec4sf(PackingSNorm::tPackF2B(binormal.x), PackingSNorm::tPackF2B(binormal.y), PackingSNorm::tPackF2B(binormal.z), PackingSNorm::tPackS2B(1)));

			vertex_id[*it] = i;
		}
		i = 0;
		for(CTopologyGraph::Facet_iterator it=(pPolygon)->facets_begin();it!=(pPolygon)->facets_end(); it++,i++)
		{
			CHalfedge* jt = (*it)->halfedge() ;
			int k = 0;
			do 
			{
				int id = vertex_id[jt->vertex()];
				faces[i].v[k] = id;
				pIndices[3*i + k] = id;
				jt = jt->next() ;
				k++;		
			} while(jt != (*it)->halfedge()) ;
			ASSERT( k == 3);		
		}

		float area=0.0f;
		float tarea=0.0f;
		for (int i=0; i< faceCount * 3; i+=3)
		{
			int idx0=pIndices[i+0];
			int idx1=pIndices[i+1];
			int idx2=pIndices[i+2];
			Vec3d e0=positions[idx1]-positions[idx0];
			Vec3d e1=positions[idx2]-positions[idx0];
			Vec2d t0 = texcoords[idx0].GetUV();
			Vec2d t1 = texcoords[idx1].GetUV();
			Vec2d t2 = texcoords[idx2].GetUV();
			Vec2d ta=Vec2d(t1.x-t0.x, t1.y-t0.y);
			Vec2d tb=Vec2d(t2.x-t0.x, t2.y-t0.y);
			area+=e0.Cross(e1).len();
			tarea+=ta.Cross(tb);
		}

		SMeshSubset subset;
		subset.nFirstIndexId = 0;
		subset.nNumIndices = faceCount * 3;
		subset.nFirstVertId = 0;
		subset.nNumVerts = vertexCount;
		subset.nMatID = 0;
		subset.nMatFlags = 0;
		subset.fRadius=100.0f;
		if (area!=0.0f)
			subset.fTexelDensity=tarea/area;
		else
			subset.fTexelDensity=1.0f;

		pMesh->GetMesh()->m_subsets.push_back(subset);

		pMesh->SetSubsetMaterialId(0,0);
		pMesh->SetSubsetIndexVertexRanges(0,0,faceCount*3,0,vertexCount);

		pMesh->CalcBBox();

		pObject->SetBBoxMin(pMesh->GetBBox().min);
		pObject->SetBBoxMax(pMesh->GetBBox().max);

		pObject->Invalidate(true);
	}

};