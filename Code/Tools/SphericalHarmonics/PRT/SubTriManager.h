/*
	subdivision triangle manager
*/

#pragma once

#include <PRT/PRTTypes.h>
#include <map>

namespace NSH
{
	template< class T >
	class vectorpvector;

	template<class TNode>
	class CNodeMemoryPool_tpl;

	//!< namespace containing all triangle subdivision specific stuff
	namespace NTriSub
	{
		//!< enum for triangle<->hemisphere pos relationship
		typedef enum EHSRel
		{
			HSRel_OUTSIDE = 0,	//!< completely outside hemisphere
			HSRel_INSIDE,				//!< completely inside hemisphere
			HSRel_PART					//!< partially outside hemisphere
		}EHSRel;

		/************************************************************************************************************************************************/
		
		//!< node data
		struct SNodeData
		{
			INSTALL_CLASS_NEW(SNodeData)
			union
			{
				struct SNodeData *pNextNodes[4];	//!< the 4 next level pointers, remember: hierarchy is fully subdivided
				struct SLeafData *pLeafNodes[4];	//!< the 4 leaf pointers, remember: hierarchy is fully subdivided
			}nextLevel;
			uint32 indices[3];									//!< the 3 vertex indices for this triangle
			struct SNodeData *pParentNode;			//!< parent node
			bool hasLeafNodes;									//!< indicates whether this node has leaf data or not
			SNodeData() : pParentNode(NULL), hasLeafNodes(false)	//!< constructor to make it memory pool compliant
			{
				indices[0] = indices[1] = indices[2] = 0;
				nextLevel.pNextNodes[0] = nextLevel.pNextNodes[1] = nextLevel.pNextNodes[2] = NULL;
			}
			//!< construction for non top node
			void Construct(const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2, const struct SNodeData *cpParentNode, const uint8 cMaxDepth, const uint8 cCurrentDepth)
			{
				assert(cpParentNode);
				assert(cIndex0 != cIndex1 && cIndex0 != cIndex2);
				pParentNode = (SNodeData*)cpParentNode; 
				hasLeafNodes = (cCurrentDepth<(cMaxDepth-1)?false:true);
				indices[0] = cIndex0;					indices[1] = cIndex1;					indices[2] = cIndex2;
			}
			//!< construction for top node
			void Construct(const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2, const uint8 cMaxDepth, const uint8 cCurrentDepth)
			{
				hasLeafNodes = (cCurrentDepth<(cMaxDepth-1)?false:true);
				indices[0] = cIndex0;					indices[1] = cIndex1;					indices[2] = cIndex2;
			}
			const bool IsTopNode()const{return (pParentNode == NULL);}
		};

		/************************************************************************************************************************************************/

		//!< leaf data
		struct SLeafData
		{
			INSTALL_CLASS_NEW(SLeafData)

			TSampleVec samples;
			uint32 indices[3];									//!< the 3 vertex indices for this triangle
			struct SNodeData *pParentNode;			//!< parent node
			
			void Construct(const struct SNodeData *cpParentNode, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2)
			{
				assert(cpParentNode);
				assert(cIndex0 != cIndex1 && cIndex0 != cIndex2);
				pParentNode = (SNodeData*)cpParentNode;
				indices[0] = cIndex0;					indices[1] = cIndex1;					indices[2] = cIndex2;
				samples.clear();
			}			
			
			SLeafData() : pParentNode(NULL)
			{
				indices[0] = indices[1] = indices[2] = 0;
			}	
		};

/************************************************************************************************************************************************/

		//!< manager class for a triangle subdivision hierarchy
		//!< each instance manages a hierarchy belonging to a original triangle
		//!< subdivision gets done equally by subdividing each triangle into 4 equal bins
		//!< since samples are supposed to be distributed randomly but as equal distributed as possible, a full subdivision is performed, 
		//!< making it faster but more memory consuming too
/*
	the subdivision goes like this: a,b and c is inserted into vertex buffer
				2
				/\
			/		\
	 c/-------\b
	/		\		 / \
0/______\/_____\1		
        a   

1st triangle indices: 0,a,c
2nd triangle indices: a,1,b
3rd triangle indices: b,2,c
4th triangle indices: a,b,c
*/
		template<class TSampleType>
		class CSampleSubTriManager_tpl
		{
		public:
			CSampleSubTriManager_tpl() : m_Depth(0), m_pHandleEncodeFunction(0){}						//!< 
			void Construct(const uint8 cDepth, const Vec3& rVertex0, const Vec3& rVertex1, const Vec3& rVertex2);
			void GetSphereSamples(vectorpvector<TSampleType>& rSampleVector)const;		//!< retrieves all stored samples 
			//!< adds a sample and stores it into the respective leaf node (projects into triangle plane first)
			//!< returns false if sample could not been inserted
			const bool AddSample(TSampleType& rcSample, const uint32 cSubTriangleIndex, const uint32 cIsocahedronLevels);	
			void Reset();	//!< resets leaf data contents
			const TSampleType& GetSample(const uint32 cSubTriangleIndex, const uint32 cTriangleIndex)const;//!< return a sample with index cTriangleIndex in subtriangle index cSubTriangleIndex
			void SetHandleEncodeFunction(const bool (*pHandleEncodeFunction) (TSampleHandle* pOutputHandle, const uint32, const uint32, const uint32, const uint32));

		private:
			typedef std::map<uint32, SLeafData*, std::less<uint32>, CSHAllocator<std::pair<const uint32, SLeafData*> > > TIndexLeafMap;
			typedef std::map<SLeafData*, uint32, std::less<SLeafData*>, CSHAllocator<std::pair<SLeafData* const, uint32> > > TIndexLeafIndexMap;

			CNodeMemoryPool_tpl<SNodeData> m_NodeMemoryPool;		//!< memory pool for nodes, don't waste new's throughout the hierarchy
			CNodeMemoryPool_tpl<SLeafData> m_LeafMemoryPool;		//!< memory pool for leafes, don't waste new's throughout the hierarchy
			TVec3Vec					m_Vertices;								//!< vertices of the triangle hierarchy, indices 0,1,2 are these for the original triangle
			SNodeData					m_TopNode;								//!< hierarchy start
			uint8							m_Depth;									//!< hierarchy depth
			TIndexLeafMap m_IndexLeafMap;								//!< map to map leaf indices to leafs					
			TIndexLeafIndexMap m_LeafIndexMap;					//!< map to map leafs to leaf indices

			const bool (*m_pHandleEncodeFunction) (TSampleHandle* pOutputHandle, const uint32, const uint32, const uint32, const uint32);		//!< pointer to handle encode function

			void Subdivide();																//!< performs the subdivision
			//!< adds the hemisphere relation between a triangle and a unit sphere pos for one node level
			void HSAddOneNodeLevel(const Vec3& rcSphereCoord, vectorpvector<TSampleType>& rSampleVector, const SNodeData *pParentNode, const bool cCompletelyInside = false)const;
			//!< recurses for one node level to retrieve all stored samples
			void SAddOneNodeLevel(vectorpvector<TSampleType>& rSampleVector, const SNodeData *pParentNode)const;
			const int8 GetTriIndex(const Vec3& crPos, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2, const uint32 cIndexNew0, const uint32 cIndexNew1, const uint32 cIndexNew2) const;	//!< retrieves the subdivided triangle index 
			SLeafData* FindLeafNode(const Vec3& crPos)const;			//!< retrieves the appropriate leaf node where to put sample in
			const EHSRel GetHSRelation(const Vec3& crSpherePos, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2)const;//!< retrieves the hemisphere relation between a triangle and a unit sphere pos
			void ConstructOneNodeLevel(const uint8 cMaxLevel, const uint8 cCurrentLevel, SNodeData *pParentNode);	//!< constructs one node
			void ClearOneNodeLevel(const SNodeData *pParentNode);	//!< clears one node level, recursive calls, used by Reset()
			void SpawnAndInsertNewVertices(uint32& rNewIndex0, uint32& rNewIndex1, uint32& rNewIndex2, const uint32 cIndex0, const uint32 cIndex1, const uint32 cIndex2);
		};
	}
}


#include "SubTriManager.inl"
