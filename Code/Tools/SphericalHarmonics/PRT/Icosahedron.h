/*
	unit sphere icosahedron class for sample generator
*/

#pragma once

#include <PRT/PRTTypes.h>
#include <CryMath/Cry_Geo.h>


namespace NSH
{
	//!< namespace containing all isocahedron specific code used by CSampleGenerator
	namespace NIcosahedron
	{
		const int8				g_cIsocahedronFaceCount	= 20;					//!< definition for regular isocahedron with 12 vertices and 20 faces, need here for compile speed up
		static const int8 s_cVertexCount					= 12;					//!< definition for regular isocahedron with 12 vertices and 20 faces

		static const double s_cEpsilon						= 0.01;				//!< epsilon for normal check
		static const double s_cTriangleEps				= 0.00001;		//!< finder epsilon

		static const float s_cX										= .52573111f;	//!< coordinate for unit sphere cartesian location
		static const float s_cZ										= .85065080f;	//!< coordinate for unit sphere cartesian location

		static const Vec3 s_cVertices[s_cVertexCount] =		//!< the s_cVertexCount isocahedron vertices  
		{    
			Vec3(-s_cX, 0.0f, s_cZ), Vec3(s_cX, 0.0f, s_cZ), Vec3(-s_cX, 0.0f, -s_cZ), Vec3(s_cX, 0.0f, -s_cZ),    
			Vec3(0.0f, s_cZ, s_cX), Vec3(0.0f, s_cZ, -s_cX), Vec3(0.0f, -s_cZ, s_cX), Vec3(0.0f, -s_cZ, -s_cX),    
			Vec3(s_cZ, s_cX, 0.0f), Vec3(-s_cZ, s_cX, 0.0f), Vec3(s_cZ, -s_cX, 0.0f), Vec3(-s_cZ, -s_cX, 0.0f) 
		};
		
		static const uint32 s_cIndices[g_cIsocahedronFaceCount][3] =				//!< the indices of the g_cIsocahedronFaceCount face isocahedron 
		{ 
			{0,4,1},	{0,9,4},	{9,5,4},	{4,5,8},	{4,8,1},    
			{8,10,1}, {8,3,10}, {5,3,8},	{5,2,3},	{2,7,3},    
			{7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6}, 
			{6,1,10}, {9,0,11}, {9,11,2}, {9,2,5},	{7,2,11} 
		};

		static const Triangle_tpl<double> Triangles[g_cIsocahedronFaceCount] =	//!< redundant triangles, but we need them to be in place rather than calls to create them all the time
		{
			Triangle_tpl<double>(s_cVertices[s_cIndices[0][0]], s_cVertices[s_cIndices[0][1]], s_cVertices[s_cIndices[0][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[1][0]], s_cVertices[s_cIndices[1][1]], s_cVertices[s_cIndices[1][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[2][0]], s_cVertices[s_cIndices[2][1]], s_cVertices[s_cIndices[2][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[3][0]], s_cVertices[s_cIndices[3][1]], s_cVertices[s_cIndices[3][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[4][0]], s_cVertices[s_cIndices[4][1]], s_cVertices[s_cIndices[4][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[5][0]], s_cVertices[s_cIndices[5][1]], s_cVertices[s_cIndices[5][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[6][0]], s_cVertices[s_cIndices[6][1]], s_cVertices[s_cIndices[6][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[7][0]], s_cVertices[s_cIndices[7][1]], s_cVertices[s_cIndices[7][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[8][0]], s_cVertices[s_cIndices[8][1]], s_cVertices[s_cIndices[8][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[9][0]], s_cVertices[s_cIndices[9][1]], s_cVertices[s_cIndices[9][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[10][0]], s_cVertices[s_cIndices[10][1]], s_cVertices[s_cIndices[10][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[11][0]], s_cVertices[s_cIndices[11][1]], s_cVertices[s_cIndices[11][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[12][0]], s_cVertices[s_cIndices[12][1]], s_cVertices[s_cIndices[12][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[13][0]], s_cVertices[s_cIndices[13][1]], s_cVertices[s_cIndices[13][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[14][0]], s_cVertices[s_cIndices[14][1]], s_cVertices[s_cIndices[14][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[15][0]], s_cVertices[s_cIndices[15][1]], s_cVertices[s_cIndices[15][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[16][0]], s_cVertices[s_cIndices[16][1]], s_cVertices[s_cIndices[16][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[17][0]], s_cVertices[s_cIndices[17][1]], s_cVertices[s_cIndices[17][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[18][0]], s_cVertices[s_cIndices[18][1]], s_cVertices[s_cIndices[18][2]]),
			Triangle_tpl<double>(s_cVertices[s_cIndices[19][0]], s_cVertices[s_cIndices[19][1]], s_cVertices[s_cIndices[19][2]])
		};

		//!< cos angle from center to each vertex of a certain face (constant throughout the regular isocahedron)
		static const double s_cCenterDiffCosine = Abs(GetNormalized((s_cVertices[s_cIndices[0][0]] + s_cVertices[s_cIndices[0][1]] + s_cVertices[s_cIndices[0][2]]) * 1./3.) * s_cVertices[s_cIndices[0][0]]);

		//!< class managing the bins for isocahedron hierarchy
		class CIsocahedronManager
		{
		public:
			CIsocahedronManager()	//!< default constructor constructs centers
			{}
			
			static const Triangle_tpl<double>& GetFace(const uint8 cFaceIndex)
			{
				assert(cFaceIndex < g_cIsocahedronFaceCount);
				return Triangles[cFaceIndex];
			}

			const int8 GetFaceIndex(const Vec3& rcSphereCoord)const;	//!< returns the face index for a particular coordinate on the unit sphere, -1 in case of an error
			void RetrieveFaceVertices(const uint8 cFaceIndex, Vec3& rV0, Vec3& rV1, Vec3& rV2)const;	//!< returns the face vertices
		private:
		};
	}
}

inline const int8 NSH::NIcosahedron::CIsocahedronManager::GetFaceIndex(const Vec3& rcSphereCoord)const
{
	assert(rcSphereCoord.GetLengthSquared() < 1.01);	//make sure it is on the sphere
	//iterate through faces, check coarse normal angle and if within threshold, perform double check
	int8 index = -1; 
	Vec3 dummy;
	while(++index < g_cIsocahedronFaceCount)
	{
		//project sample into original triangle plane
		if(NRayTriangleIntersect::RayTriangleIntersect(Vec3(0.,0.,0.), rcSphereCoord, Triangles[index].v1, Triangles[index].v2, Triangles[index].v0, dummy))
			return index;
	}
	return -1;//should not happen
}

inline void NSH::NIcosahedron::CIsocahedronManager::RetrieveFaceVertices(const uint8 cFaceIndex, Vec3& rV0, Vec3& rV1, Vec3& rV2)const
{
	assert(cFaceIndex <= g_cIsocahedronFaceCount);
	rV0 = s_cVertices[s_cIndices[cFaceIndex][0]];
	rV1 = s_cVertices[s_cIndices[cFaceIndex][1]];
	rV2 = s_cVertices[s_cIndices[cFaceIndex][2]];
}

