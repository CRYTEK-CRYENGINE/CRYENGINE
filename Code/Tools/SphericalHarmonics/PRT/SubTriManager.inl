/*
	subdivision triangle template implementation
*/

#include "BasicUtils.h"
#include <CryMath/Cry_Geo.h>

template<class TSampleType>
void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::Construct(const uint8 cDepth, const Vec3& rVertex0, const Vec3& rVertex1, const Vec3& rVertex2)
{
	assert(cDepth < 255);
	m_Depth = cDepth;
	//compute number of nodes and leafs
	uint32 power = 1;
	uint32 sum = 0;
	for(int j=0; j<cDepth; ++j)
	{
		power *= 4;
		sum += power;
	}
	if(cDepth > 0)
		sum -= power; //last level counts only leafs

	if(sum > 0)
		m_NodeMemoryPool.Construct(sum);//4 nodes per hierarchy, top node not allocated in memory pool
	m_LeafMemoryPool.Construct(power);//4 leafes per leaf node, top node not allocated in memory pool
	//insert the top vertices
	m_Vertices.push_back(rVertex0);
	m_Vertices.push_back(rVertex1);
	m_Vertices.push_back(rVertex2);
	Subdivide();//start the subdivision
}

template<class TSampleType>
void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::ConstructOneNodeLevel(const uint8 cMaxLevel, const uint8 cCurrentLevel, SNodeData *pParentNode)
{
	if(pParentNode->hasLeafNodes)
	{
		//construct leaf nodes instead
		SLeafData *p0 = m_LeafMemoryPool.AllocateNext();		assert(p0);
		SLeafData *p1 = m_LeafMemoryPool.AllocateNext();		assert(p1);
		SLeafData *p2 = m_LeafMemoryPool.AllocateNext();		assert(p2);
		SLeafData *p3 = m_LeafMemoryPool.AllocateNext();		assert(p3);
		//record new leafs
		uint32_t index = (uint32_t)m_IndexLeafMap.size();
		m_IndexLeafMap.insert(std::pair<uint32, SLeafData*>(index, p0));
		m_LeafIndexMap.insert(std::pair<SLeafData*, uint32>(p0, index));
		index = m_IndexLeafMap.size();
		m_IndexLeafMap.insert(std::pair<uint32, SLeafData*>(index, p1));
		m_LeafIndexMap.insert(std::pair<SLeafData*, uint32>(p1, index));
		index = m_IndexLeafMap.size();
		m_IndexLeafMap.insert(std::pair<uint32, SLeafData*>(index, p2));
		m_LeafIndexMap.insert(std::pair<SLeafData*, uint32>(p2, index));
		index = m_IndexLeafMap.size();
		m_IndexLeafMap.insert(std::pair<uint32, SLeafData*>(index, p3));
		m_LeafIndexMap.insert(std::pair<SLeafData*, uint32>(p3, index));

		pParentNode->nextLevel.pLeafNodes[0] = p0;	
		pParentNode->nextLevel.pLeafNodes[1] = p1;
		pParentNode->nextLevel.pLeafNodes[2] = p2;
		pParentNode->nextLevel.pLeafNodes[3] = p3;
		//spawn new vertex	
		uint32 newIndex0 = 0, newIndex1 = 0, newIndex2 = 0;
		SpawnAndInsertNewVertices(newIndex0, newIndex1, newIndex2, pParentNode->indices[0], pParentNode->indices[1], pParentNode->indices[2]);
		p0->Construct(pParentNode,	pParentNode->indices[0],	newIndex0,									newIndex2);
		p1->Construct(pParentNode,	newIndex0,								pParentNode->indices[1],		newIndex1);
		p2->Construct(pParentNode,	newIndex1,								pParentNode->indices[2],		newIndex2);
		p3->Construct(pParentNode,	newIndex0,								newIndex1,									newIndex2);

		assert(m_LeafIndexMap.size() == m_IndexLeafMap.size());

		return;
	}
	SNodeData *p0 = m_NodeMemoryPool.AllocateNext();		assert(p0);
	SNodeData *p1 = m_NodeMemoryPool.AllocateNext();		assert(p1);
	SNodeData *p2 = m_NodeMemoryPool.AllocateNext();		assert(p2);
	SNodeData *p3 = m_NodeMemoryPool.AllocateNext();		assert(p3);

	pParentNode->nextLevel.pNextNodes[0] = p0;	
	pParentNode->nextLevel.pNextNodes[1] = p1;
	pParentNode->nextLevel.pNextNodes[2] = p2;
	pParentNode->nextLevel.pNextNodes[3] = p3;
	//spawn new vertex
	uint32 newIndex0 = 0, newIndex1 = 0, newIndex2 = 0;
	SpawnAndInsertNewVertices(newIndex0, newIndex1, newIndex2, pParentNode->indices[0], pParentNode->indices[1], pParentNode->indices[2]);
	p0->Construct(pParentNode->indices[0],		newIndex0,									newIndex2,		pParentNode, cMaxLevel, cCurrentLevel + 1);
	p1->Construct(newIndex0,									pParentNode->indices[1],		newIndex1,		pParentNode, cMaxLevel, cCurrentLevel + 1);
	p2->Construct(newIndex1,									pParentNode->indices[2],		newIndex2,		pParentNode, cMaxLevel, cCurrentLevel + 1);
	p3->Construct(newIndex0,									newIndex1,									newIndex2,		pParentNode, cMaxLevel, cCurrentLevel + 1);
	//continue recursion
	ConstructOneNodeLevel(cMaxLevel, cCurrentLevel + 1, p0);
	ConstructOneNodeLevel(cMaxLevel, cCurrentLevel + 1, p1);
	ConstructOneNodeLevel(cMaxLevel, cCurrentLevel + 1, p2);
	ConstructOneNodeLevel(cMaxLevel, cCurrentLevel + 1, p3);
}

template<class TSampleType>
void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::Subdivide()
{
	//per level: construct nodes and add a new vertex
	//first top node
	//don't forget to set leaf nodes and next nodes to the nodes 
	if(m_Depth < 1)
		return;//nothing to do
	m_IndexLeafMap.clear();
	//now construct top node
	m_TopNode.Construct(0, 1, 2, m_Depth, 0);
	//start recursion
	ConstructOneNodeLevel(m_Depth, 0, &m_TopNode);
}

template<class TSampleType>
const int8 NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::GetTriIndex(const Vec3& crPos, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2, const uint32 cIndexNew0, const uint32 cIndexNew1, const uint32 cIndexNew2) const 
{
	assert(cIndex0 < m_Vertices.size() && cIndex1 < m_Vertices.size() && cIndex2 < m_Vertices.size() && 
		cIndexNew0 < m_Vertices.size() && cIndexNew1 < m_Vertices.size() && cIndexNew2 < m_Vertices.size());
	const Vec3& rV0 = m_Vertices[cIndex0];		
	const Vec3& rV1 = m_Vertices[cIndex1];
	const Vec3& rV2 = m_Vertices[cIndex2];
	const Vec3& rVN0 = m_Vertices[cIndexNew0];		
	const Vec3& rVN1 = m_Vertices[cIndexNew1];
	const Vec3& rVN2 = m_Vertices[cIndexNew2];

	const double dist0 = Distance::Point_Triangle(crPos, Triangle_tpl<float>(rV0, rVN0, rVN2) );
	const double dist1 = Distance::Point_Triangle(crPos, Triangle_tpl<float>(rVN0, rV1, rVN1) );
	const double dist2 = Distance::Point_Triangle(crPos, Triangle_tpl<float>(rVN1, rV2, rVN2) );
	const double dist3 = Distance::Point_Triangle(crPos, Triangle_tpl<float>(rVN0, rVN1, rVN2) );
	double minDist = 1000.;
	int8 minIndex = -1;
	if(dist0 < dist1)
	{
		if(dist0 < dist2)
		{
			minIndex = 0;	minDist = dist0;
		}
		else
		{
			minIndex = 2;	minDist = dist2;
		}
	}
	else
	{
		if(dist1 < dist2)
		{
			minIndex = 1;	minDist = dist1;
		}
		else
		{
			minIndex = 2;	minDist = dist2;
		}
	}
	if(minDist > dist3)
	{
		minIndex = 3;	minDist = dist3;
	}
	const double s_cFailThreshold = 0.01;
	//we would need a planar projection of the point into the triangle here, just leave this sample out
	if(minDist > s_cFailThreshold)
		-1;
	return minIndex;
}

template<class TSampleType>
NSH::NTriSub::SLeafData* NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::FindLeafNode(const Vec3& crPos) const
{
	//go down the hierarchy til we are at leaf level
	int8 index = 0;
	const SNodeData *pCurNode = &m_TopNode;
	SLeafData *pLeafNode = NULL;
	while(!pCurNode->hasLeafNodes)
	{
		//retrieve vertices
		index = GetTriIndex
		(
			crPos, pCurNode->indices[0], pCurNode->indices[1], pCurNode->indices[2], 
			pCurNode->nextLevel.pNextNodes[0]->indices[1], pCurNode->nextLevel.pNextNodes[1]->indices[2], pCurNode->nextLevel.pNextNodes[0]->indices[2]
		);
		if(index == -1)
			return NULL;
		pCurNode = pCurNode->nextLevel.pNextNodes[index];
	}
	//now get leaf node
	index = GetTriIndex
	(
		crPos, pCurNode->indices[0], pCurNode->indices[1], pCurNode->indices[2], 
		pCurNode->nextLevel.pLeafNodes[0]->indices[1], pCurNode->nextLevel.pLeafNodes[0]->indices[2], pCurNode->nextLevel.pLeafNodes[1]->indices[2]
	);
	if(index == -1)
		return NULL;
	pLeafNode = pCurNode->nextLevel.pLeafNodes[index];
	return pLeafNode;
}

template<class TSampleType>
const bool NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::AddSample(TSampleType& rcSample, const uint32 cSubTriangleIndex, const uint32 cIsocahedronLevels)
{
	//retrieve pos on sphere
	const TCartesianCoord pos = rcSample.GetCartesianPos();
	//get intersection with triangle
	Vec3 newInPos;
	//check triangle orientation
	const Vec3 oneEdge = m_Vertices[0] - m_Vertices[1]; 
	const Vec3 secondEdge = m_Vertices[0] - m_Vertices[2]; 
	Vec3 trianglePlaneNormal =  oneEdge % secondEdge; 	
	if(trianglePlaneNormal * rcSample.GetCartesianPos() > 0)
	{
		if(!NRayTriangleIntersect::RayTriangleIntersect(Vec3(0.,0.,0.), rcSample.GetCartesianPos(), m_Vertices[0], m_Vertices[2], m_Vertices[1], newInPos))
			return false;
	}
	else
	{
		if(!NRayTriangleIntersect::RayTriangleIntersect(Vec3(0.,0.,0.), rcSample.GetCartesianPos(), m_Vertices[0], m_Vertices[1], m_Vertices[2], newInPos))
			return false;
	}
	//now go through all levels and find appropriate triangle where the projected sample pos lies in
	SLeafData* pLeafNode = FindLeafNode(newInPos);
	if(!pLeafNode)
		return false;
	//set new handle
	TIndexLeafIndexMap::const_iterator iter = m_LeafIndexMap.find(pLeafNode);
	assert(iter != m_LeafIndexMap.end());
	//now encode handle
	if(m_pHandleEncodeFunction)
	{
		TSampleHandle handle;
		if(!(m_pHandleEncodeFunction(&handle, cIsocahedronLevels, cSubTriangleIndex, (TSampleHandle)iter->second, (uint32)pLeafNode->samples.size())))
			return false;
		rcSample.SetHandle(handle);
	}
	pLeafNode->samples.push_back(rcSample);
	return true;
}

template<class TSampleType>
inline void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::SetHandleEncodeFunction(const bool (*pHandleEncodeFunction) (TSampleHandle* pOutputHandle, const uint32, const uint32, const uint32, const  uint32))
{
	m_pHandleEncodeFunction = pHandleEncodeFunction;
}

template<class TSampleType>
const NSH::NTriSub::EHSRel NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::GetHSRelation(const Vec3& crSpherePos, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2)const
{
	const double s_cThreshold = 0.0001;
	//don't normalize, just check sign
	const double cosAngle0 = crSpherePos * m_Vertices[cIndex0];
	const double cosAngle1 = crSpherePos * m_Vertices[cIndex1];
	const double cosAngle2 = crSpherePos * m_Vertices[cIndex2];

	if(cosAngle0 > -s_cThreshold && cosAngle1 > -s_cThreshold && cosAngle2 > -s_cThreshold)
		return HSRel_INSIDE;
	if(cosAngle0 < s_cThreshold && cosAngle1 < s_cThreshold && cosAngle2 < s_cThreshold)
		return HSRel_OUTSIDE;
	return HSRel_PART;
}

template<class TSampleType>
void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::HSAddOneNodeLevel(const Vec3& rcSphereCoord, vectorpvector<TSampleType>& rSampleVector, const SNodeData *pParentNode, const bool cCompletelyInside)const
{
	//idea: check dot product between sphere pos and unit sphere->normalized) and all triangle vertices, if entire triangle outside hemisphere->discard, if entirely in: add all leaf vectors, if partially in, continue
	if(pParentNode->hasLeafNodes)
	{
		if(cCompletelyInside)
		{
			//no test necessary
			for(int i=0; i<4; i++)
				rSampleVector.push_back(&(pParentNode->nextLevel.pLeafNodes[i]->samples));
			return;
		}
		//add all samples which are at least partially on the the hemisphere around rcSphereCoord
		for(int i=0; i<4; i++)
		{
			const SLeafData& crLeaf = *(pParentNode->nextLevel.pLeafNodes[i]);
			if(GetHSRelation(rcSphereCoord, crLeaf.indices[0], crLeaf.indices[1], crLeaf.indices[2]) != HSRel_OUTSIDE)
			{
				//now add only samples when completely inside. some missing by this method
				rSampleVector.push_back(&(crLeaf.samples));
			}
		}
		return;
	}
	if(cCompletelyInside)
	{
		//no test necessary
		for(int i=0; i<4; i++)
			HSAddOneNodeLevel(rcSphereCoord, rSampleVector, pParentNode->nextLevel.pNextNodes[i], true);			
	}
	else
	{
		for(int i=0; i<4; i++)
		{
			const SNodeData* cpNode = pParentNode->nextLevel.pNextNodes[i];
			const EHSRel rel = GetHSRelation(rcSphereCoord, cpNode->indices[0], cpNode->indices[1], cpNode->indices[2]);
			if(rel == HSRel_OUTSIDE)
				continue;
			if(rel == HSRel_INSIDE)
				HSAddOneNodeLevel(rcSphereCoord, rSampleVector, cpNode, true);			
			else
				HSAddOneNodeLevel(rcSphereCoord, rSampleVector, cpNode, false);			
		}
	}
}

template<class TSampleType>
inline void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::SAddOneNodeLevel(vectorpvector<TSampleType>& rSampleVector, const SNodeData *pParentNode)const
{
	if(pParentNode->hasLeafNodes)
	{
		//no test necessary
		for(int i=0; i<4; i++)
			rSampleVector.push_back(&(pParentNode->nextLevel.pLeafNodes[i]->samples));
		return;
	}
	for(int i=0; i<4; i++)
		SAddOneNodeLevel(rSampleVector, pParentNode->nextLevel.pNextNodes[i]);			
}

template<class TSampleType>
inline void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::GetSphereSamples(vectorpvector<TSampleType>& rSampleVector)const
{
	rSampleVector.clear();
	SAddOneNodeLevel(rSampleVector, &m_TopNode);
}

template<class TSampleType>
inline void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::SpawnAndInsertNewVertices(uint32& rNewIndex0, uint32& rNewIndex1, uint32& rNewIndex2, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2)
{
	//insert the 3 new vertices
	assert(cIndex0 < m_Vertices.size());					assert(cIndex1 < m_Vertices.size());					assert(cIndex2 < m_Vertices.size());
	const Vec3 newVertex0 = (m_Vertices[cIndex0] + m_Vertices[cIndex1]) * (double)1./2.;
	const Vec3 newVertex1 = (m_Vertices[cIndex1] + m_Vertices[cIndex2]) * (double)1./2.;
	const Vec3 newVertex2 = (m_Vertices[cIndex2] + m_Vertices[cIndex0]) * (double)1./2.;
	m_Vertices.push_back(newVertex0);		rNewIndex0 = (uint32)m_Vertices.size() - 1;
	m_Vertices.push_back(newVertex1);		rNewIndex1 = (uint32)m_Vertices.size() - 1;
	m_Vertices.push_back(newVertex2);		rNewIndex2 = (uint32)m_Vertices.size() - 1;
}

template<class TSampleType>
inline void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::ClearOneNodeLevel(const SNodeData *pParentNode)
{
	if(pParentNode->hasLeafNodes)
	{
		//no test necessary
		for(int i=0; i<4; i++)
			(pParentNode->nextLevel.pLeafNodes[i]->samples).clear();//clear leaf data contents
		return;
	}
	for(int i=0; i<4; i++)
		ClearOneNodeLevel(pParentNode->nextLevel.pNextNodes[i]);			
}

template<class TSampleType>
inline void NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::Reset()
{
	ClearOneNodeLevel(&m_TopNode);
}

template<class TSampleType>
inline const TSampleType& NSH::NTriSub::CSampleSubTriManager_tpl<TSampleType>::GetSample(const uint32 cSubTriangleIndex, const uint32 cTriangleIndex)const
{
	assert(cSubTriangleIndex < m_IndexLeafMap.size());
	TIndexLeafMap::const_iterator iter = m_IndexLeafMap.find(cSubTriangleIndex);
	assert(iter != m_IndexLeafMap.end());
	const SLeafData* cpLeaf = iter->second;
	assert(cpLeaf);
	assert(cTriangleIndex < (uint32)cpLeaf->samples.size());
	return cpLeaf->samples[cTriangleIndex];
}