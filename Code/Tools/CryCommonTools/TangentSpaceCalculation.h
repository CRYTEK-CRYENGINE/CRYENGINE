// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Permission to distribute for commercial and non-commercial purposes.
// This source code is provided "as it" without support and any warranty.
//
//  File:TangentSpaceCalculation.h
//  Description: calculated the tangent space base vector for a given mesh
//  Dependencies: none
//  Documentation: ShaderX4 "triangle mesh tangent space calculation"
//
//  Usage:
//    implement the proxy class: CTriangleInputProxy
//    instance the proxy class: CTriangleInputProxy MyProxy(MyData);
//    instance the calculation helper: CTangentSpaceCalculation<MyProxy> MyTangent;
//    do the calculation: MyTangent.CalculateTangentSpace(MyProxy);
//    get the data back: MyTangent.GetTriangleIndices(), MyTangent.GetBase()

#pragma once

#include <vector>									// STL vector<> 
#include <map>										// STL map<,,> multimap<>

#define BASEMATRIXMERGEBIAS	0.9f

/* // use this as reference
class CTriangleInputProxy
{
public:

	//! /return 0..
	unsigned int GetTriangleCount( void ) const;	

	//! /param indwTriNo 0..
	//! /param outdwPos
	//! /param outdwNorm
	//! /param outdwUV
	void GetTriangleIndices( const unsigned int indwTriNo, unsigned int outdwPos[3], unsigned int outdwNorm[3], unsigned int outdwUV[3] ) const;	

	//! /param indwPos 0..
	//! /param outfPos
	void GetPos( const unsigned int indwPos, float outfPos[3] ) const;

	//! /param indwPos 0..
	//! /param outfUV 
	void GetUV( const unsigned int indwPos, float outfUV[2] ) const;
};
*/
 

// ---------------------------------------------------------------------------------------------------------------


// InputProxy - use CTriangleInputProxy as reference
template <class InputProxy>
class CTangentSpaceCalculation
{
public:

	// IN ------------------------------------------------

	//! /param inInput - the normals are only used as smoothing input - we calculate the normals ourself
	void CalculateTangentSpace( const InputProxy &inInput );

	// OUT -----------------------------------------------

	//! /return the number of base vectors that are stored 0..
	size_t GetBaseCount( void );

	//!
	//! /param indwTriNo 0..
	//! /param outdwBase 
	void GetTriangleBaseIndices( const unsigned int indwTriNo, unsigned int outdwBase[3] );
	
	//! returns a orthogonal base (perpendicular and normalized)
	//! /param indwPos 0..
	//! /param outU in object/worldspace
	//! /param outV in object/worldspace
	//! /param outN in object/worldspace
	void GetBase( const unsigned int indwPos, float outU[3], float outV[3], float outN[3] );

private:	// -----------------------------------------------------------------

	class CVec2
	{
	public:
		float x,y;

		CVec2(){}
		CVec2(float fXval, float fYval) { x=fXval; y=fYval; }
		friend CVec2 operator - (const CVec2 &vec1, const CVec2 &vec2) { return CVec2(vec1.x-vec2.x, vec1.y-vec2.y); }
		operator float * () { return((float *)this); }
	};

	class CVec3
	{
	public:
		float x,y,z;

		CVec3(){}
		CVec3(float fXval, float fYval, float fZval) { x=fXval; y=fYval; z=fZval; }
		friend CVec3 operator - (const CVec3 &vec1, const CVec3 &vec2) { return CVec3(vec1.x-vec2.x, vec1.y-vec2.y, vec1.z-vec2.z); }
		friend CVec3 operator - (const CVec3 &vec1) { return CVec3(-vec1.x, -vec1.y, -vec1.z); }
		friend CVec3 operator + (const CVec3 &vec1, const CVec3 &vec2) { return CVec3(vec1.x+vec2.x, vec1.y+vec2.y, vec1.z+vec2.z); }
		friend CVec3 operator * (const CVec3 &vec1, const float fVal)	{ return CVec3(vec1.x*fVal, vec1.y*fVal, vec1.z*fVal); }
		friend float operator * (const CVec3 &vec1, const CVec3 &vec2)	{ return( vec1.x*vec2.x + vec1.y*vec2.y + vec1.z*vec2.z); }
		operator float * () { return((float *)this); }
		void Negate() { x=-x;y=-y;z=-z; }		
		friend CVec3 normalize( const CVec3 &vec ) { CVec3 ret; float fLen=length(vec); if(fLen<0.00001f)return(vec); fLen=1.0f/fLen;ret.x=vec.x*fLen;ret.y=vec.y*fLen;ret.z=vec.z*fLen;return(ret); }
		friend CVec3 cross( const CVec3 &vec1, const CVec3 &vec2 ) { return CVec3(vec1.y*vec2.z-vec1.z*vec2.y, vec1.z*vec2.x-vec1.x*vec2.z, vec1.x*vec2.y-vec1.y*vec2.x); }
		friend float length( const CVec3 &invA ) { return (float)sqrt(invA.x*invA.x+invA.y*invA.y+invA.z*invA.z); }
		friend float CalcAngleBetween( const CVec3 &invA, const CVec3 &invB )
		{
			float LengthQ=length(invA)*length(invB);

			if(LengthQ<0.0001f)LengthQ=0.0001f;			// to prevent division by zero

			float f=(invA*invB)/LengthQ;

			if(f>1.0f)f=1.0f;												// acos need input in the range [-1..1]
				else if(f<-1.0f)f=-1.0f;							//

			float fRet=(float)acos(f);								// cosf is not avaiable on every plattform
			
			return(fRet);
		}
		friend bool IsZero( const CVec3 &invA ) { return(invA.x==0.0f && invA.y==0.0f && invA.z==0.0f); }
		friend bool IsNormalized( const CVec3 &invA ) { float f=length(invA);return(f>=0.95f && f<=1.05f); }
	};

	class CBaseIndex
	{
	public:
		unsigned int m_dwPosNo;				//!< 0..
		unsigned int m_dwNormNo;				//!< 0..
	};

	// helper to get order for CVertexLoadHelper
	struct CBaseIndexOrder: public std::binary_function< CBaseIndex, CBaseIndex, bool>
	{
		bool operator() ( const CBaseIndex &a, const CBaseIndex &b ) const
		{
			// first sort by position
			if(a.m_dwPosNo<b.m_dwPosNo)return(true);
			if(a.m_dwPosNo>b.m_dwPosNo)return(false);

			// then by normal
			if(a.m_dwNormNo<b.m_dwNormNo)return(true);
			if(a.m_dwNormNo>b.m_dwNormNo)return(false);

			return(false);
		}
	};


	class CBase33
	{
	public:

		CBase33() { }
		CBase33(CVec3 Uval, CVec3 Vval, CVec3 Nval) { u=Uval; v=Vval; n=Nval; }

		CVec3 u;								//!<
		CVec3 v;								//!<
		CVec3 n;								//!< is part of the tangent base but can be used also as vertex normal
	};

	class CTriBaseIndex
	{
	public:
		unsigned int p[3];							//!< index in m_BaseVectors 
	};

	// output data -----------------------------------------------------------------------------------

	std::vector<CTriBaseIndex>										m_TriBaseAssigment;			//!< [0..dwTriangleCount]
	std::vector<CBase33>													m_BaseVectors;					//!< [0..] generated output data

	//! /param indwPosNo
	//! /param indwNormNo
	CBase33 &GetBase( std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder> &inMap, const unsigned int indwPosNo, const unsigned int indwNormNo );

private:
	//! creates, copies or returns a reference
	//! /param inMap
	//! /param indwPosNo
	//! /param indwNormNo
	//! /param inU weighted
	//! /param inV weighted
	//! /param inN normalized
	unsigned int AddUV2Base( std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder> &inMap, const unsigned int indwPosNo, const unsigned int indwNormNo, const CVec3 &inU, const CVec3 &inV, const CVec3 &inNormN );

	//! /param inMap
	//! /param indwPosNo
	//! /param indwNormNo
	//! /param inNormal weighted normal
	void AddNormal2Base( std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder> &inMap, const unsigned int indwPosNo, const unsigned int indwNormNo, const CVec3 &inNormal );

	//! this code was heavly tested with external test app by SergiyM and MartinM
	//! rotates the input vector with the rotation to-from
	//! /param vFrom has to be normalized
	//! /param vTo has to be normalized
	//! /param vInput 
	static CVec3 Rotate( const CVec3 &vFrom, const CVec3 &vTo, const CVec3 &vInput )
	{
		// no mesh is perfect 	
//		assert(IsNormalized(vFrom));
		// no mesh is perfect 	
//		assert(IsNormalized(vTo));

		CVec3 vRotAxis=cross(vFrom,vTo);												// rotation axis

		float fSin=length(vRotAxis);
		float fCos=vFrom*vTo;

		if(fSin<0.00001f)																				// no rotation
			return(vInput);

		vRotAxis=vRotAxis*(1.0f/fSin);													// normalize

		CVec3 vFrom90deg=normalize(cross(vRotAxis,vFrom));			// perpendicular to vRotAxis and vFrom90deg

		// Base is vFrom,vFrom90deg,vRotAxis

		float fXInPlane=vFrom*vInput;
		float fYInPlane=vFrom90deg*vInput;

		CVec3 a=vFrom*(fXInPlane*fCos-fYInPlane*fSin);
		CVec3 b=vFrom90deg*(fXInPlane*fSin+fYInPlane*fCos);
		CVec3 c=vRotAxis*(vRotAxis*vInput);

		return( a + b + c );
	}

	void DebugMesh( const InputProxy &inInput ) const;
};





// ---------------------------------------------------------------------------------------------------------------

template <class InputProxy>
void CTangentSpaceCalculation<InputProxy>::DebugMesh( const InputProxy &inInput ) const
{
	unsigned int dwTriCount=inInput.GetTriangleCount();

	// search for polygons that use the same indices (input data problems)
	for(unsigned int a=0;a<dwTriCount;a++)
	{
		unsigned int dwAPos[3],dwANorm[3],dwAUV[3];

		inInput.GetTriangleIndices(a,dwAPos,dwANorm,dwAUV);

		for(unsigned int b=a+1;b<dwTriCount;b++)
		{
			unsigned int dwBPos[3],dwBNorm[3],dwBUV[3];

			inInput.GetTriangleIndices(b,dwBPos,dwBNorm,dwBUV);

			assert(!(  dwAPos[0]==dwBPos[0] && dwAPos[1]==dwBPos[1] && dwAPos[2]==dwBPos[2] ));
			assert(!(  dwAPos[1]==dwBPos[0] && dwAPos[2]==dwBPos[1] && dwAPos[0]==dwBPos[2] ));
			assert(!(  dwAPos[2]==dwBPos[0] && dwAPos[0]==dwBPos[1] && dwAPos[1]==dwBPos[2] ));

			assert(!(  dwAPos[1]==dwBPos[0] && dwAPos[0]==dwBPos[1] && dwAPos[2]==dwBPos[2] ));
			assert(!(  dwAPos[2]==dwBPos[0] && dwAPos[1]==dwBPos[1] && dwAPos[0]==dwBPos[2] ));
			assert(!(  dwAPos[0]==dwBPos[0] && dwAPos[2]==dwBPos[1] && dwAPos[1]==dwBPos[2] ));
		}
	}
}




template <class InputProxy>
void CTangentSpaceCalculation<InputProxy>::CalculateTangentSpace( const InputProxy &inInput )
{
	unsigned int dwTriCount=inInput.GetTriangleCount();

	// clear result
	m_BaseVectors.clear();
	m_TriBaseAssigment.clear();
	m_TriBaseAssigment.reserve(dwTriCount);
	assert(m_BaseVectors.size()==0);
	assert(m_TriBaseAssigment.size()==0);

	std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder>		mBaseMap;					// second=index into m_BaseVectors, generated output data
	std::vector<CBase33> vTriangleBase;																	// base vectors per triangle

//	DebugMesh(inInput);		// only for debugging - slow

	// calculate the base vectors per triangle -------------------------------------------
	{
		for(unsigned int i=0;i<dwTriCount;i++)
		{
			// get data from caller ---------------------------
			unsigned int dwPos[3],dwNorm[3],dwUV[3];

			inInput.GetTriangleIndices(i,dwPos,dwNorm,dwUV);

			CVec3 vPos[3];
			CVec2 vUV[3];

			for(int e=0;e<3;e++)
			{
				inInput.GetPos(dwPos[e],vPos[e]);
				inInput.GetUV(dwUV[e],vUV[e]);
			}

			// calculate tangent vectors ---------------------------

			CVec3 vA=vPos[1]-vPos[0];
			CVec3 vB=vPos[2]-vPos[0];

/*
			char str[2024];

			sprintf(str,"in: vA=(%.3f %.3f %.3f) vB=(%.3f %.3f %.3f)\n",vA.x,vA.y,vA.z,vA.x,vA.y,vA.z);
			OutputDebugString(str);
*/

			float fDeltaU1=vUV[1].x-vUV[0].x;
			float fDeltaU2=vUV[2].x-vUV[0].x;
			float fDeltaV1=vUV[1].y-vUV[0].y;
			float fDeltaV2=vUV[2].y-vUV[0].y;

			float div	=(fDeltaU1*fDeltaV2-fDeltaU2*fDeltaV1);

			CVec3 vU,vV,vN=normalize(cross(vA,vB));


			if(div!=0.0)
			{
				//	area(u1*v2-u2*v1)/2
				float fAreaMul2=fabsf(fDeltaU1*fDeltaV2-fDeltaU2*fDeltaV1);  // weight the tangent vectors by the UV triangles area size (fix problems with base UV assignment)

				float a = fDeltaV2/div;
				float b	= -fDeltaV1/div;
				float c = -fDeltaU2/div;
				float d	= fDeltaU1/div;

				vU=normalize(vA*a+vB*b)*fAreaMul2;
				vV=normalize(vA*c+vB*d)*fAreaMul2;
			}
			else { vU=CVec3(0,0,0);vV=CVec3(0,0,0); }

			vTriangleBase.push_back(CBase33(vU,vV,vN));
		}
	}


	// distribute the normals to the vertices
	{
		// we create a new tangent base for every vertex index that has a different normal (later we split further for mirrored use)
		// and sum the base vectors (weighted by angle and mirrored if necessary)
		for(unsigned int i=0;i<dwTriCount;i++)
		{
			unsigned int e;

			// get data from caller ---------------------------
			unsigned int dwPos[3],dwNorm[3],dwUV[3];

			inInput.GetTriangleIndices(i,dwPos,dwNorm,dwUV);
			CBase33 TriBase=vTriangleBase[i];
			CVec3 vPos[3];

			for(e=0;e<3;e++) inInput.GetPos(dwPos[e],vPos[e]);

			// for each triangle vertex
			for(e=0;e<3;e++)	
			{
				float fWeight=CalcAngleBetween( vPos[(e+2)%3]-vPos[e],vPos[(e+1)%3]-vPos[e] );			// weight by angle to fix the L-Shape problem
//				float fWeight=length(cross( vPos[(e+2)%3]-vPos[e],vPos[(e+1)%3]-vPos[e] ));			// weight by area, that does not fix the L-Shape problem 100%

				if(fWeight<=0.0f)
					fWeight=0.0001f;

				AddNormal2Base(mBaseMap,dwPos[e],dwNorm[e],TriBase.n*fWeight);
			}
		}
	}

	// distribute the uv vectors to the vertices
	{
		// we create a new tangent base for every vertex index that has a different normal
		// if the base vectors does'nt fit we split as well
		for(unsigned int i=0;i<dwTriCount;i++)
		{
			unsigned int e;

			// get data from caller ---------------------------
			unsigned int dwPos[3],dwNorm[3],dwUV[3];

			CTriBaseIndex Indx;
			inInput.GetTriangleIndices(i,dwPos,dwNorm,dwUV);
			CBase33 TriBase=vTriangleBase[i];
			CVec3 vPos[3];

			for(e=0;e<3;e++) inInput.GetPos(dwPos[e],vPos[e]);

			// for each triangle vertex
			for(e=0;e<3;e++)	
			{
				float fWeight=CalcAngleBetween( vPos[(e+2)%3]-vPos[e],vPos[(e+1)%3]-vPos[e] );			// weight by angle to fix the L-Shape problem
//				float fWeight=length(cross( vPos[(e+2)%3]-vPos[e],vPos[(e+1)%3]-vPos[e] ));			// weight by area, that does not fix the L-Shape problem 100%

				Indx.p[e]=AddUV2Base(mBaseMap,dwPos[e],dwNorm[e],TriBase.u*fWeight,TriBase.v*fWeight,normalize(TriBase.n));
			}

			m_TriBaseAssigment.push_back(Indx);
		}
	}


	// orthogonalize the base vectors per vertex -------------------------------------------
	{
		std::vector<CBase33>::iterator it;
		
		for(it=m_BaseVectors.begin();it!=m_BaseVectors.end();++it)
		{
			CBase33 &ref=(*it);

			// rotate u and v in n plane
			{
				// (N is dominating, U and V equal weighted)
				CVec3 vUout,vVout,vNout;

				vNout=normalize(ref.n);

				vUout = ref.u - vNout * (vNout*ref.u);						// project u in n plane
				vVout = ref.v - vNout * (vNout*ref.v);						// project v in n plane

				ref.u=normalize(vUout);ref.v=normalize(vVout);ref.n=vNout;
				
				assert(ref.u.x>=-1 && ref.u.x<=1);
				assert(ref.u.y>=-1 && ref.u.y<=1);
				assert(ref.u.z>=-1 && ref.u.z<=1);
				assert(ref.v.x>=-1 && ref.v.x<=1);
				assert(ref.v.y>=-1 && ref.v.y<=1);
				assert(ref.v.z>=-1 && ref.v.z<=1);
				assert(ref.n.x>=-1 && ref.n.x<=1);
				assert(ref.n.y>=-1 && ref.n.y<=1);
				assert(ref.n.z>=-1 && ref.n.z<=1);
			}
		}
	}
}



template <class InputProxy>
unsigned int CTangentSpaceCalculation<InputProxy>::AddUV2Base( std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder> &inMap,
	const unsigned int indwPosNo, const unsigned int indwNormNo, const CVec3 &inU, const CVec3 &inV, const CVec3 &inNormN )
{
	// no mesh is perfect
//	assert(IsNormalized(inNormN));

	CBaseIndex Indx;

	Indx.m_dwPosNo=indwPosNo;
	Indx.m_dwNormNo=indwNormNo;

	std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder>::iterator iFind,iFindEnd;
		
	iFind = inMap.lower_bound(Indx);

	assert(iFind!=inMap.end());

	CVec3 vNormal=m_BaseVectors[(*iFind).second].n;

	iFindEnd = inMap.upper_bound(Indx);

	unsigned int dwBaseUVIndex=0xffffffff;													// init with not found

	bool bParity=(cross(inU,inV)*inNormN>0.0f);

	for(;iFind!=iFindEnd;++iFind)
	{
		CBase33 &refFound=m_BaseVectors[(*iFind).second];

		if(!IsZero(refFound.u)) 
		{
			bool bParityRef=(cross(refFound.u,refFound.v)*refFound.n>0.0f);
			bool bParityCheck=(bParityRef==bParity);

			if(!bParityCheck)continue;

//			bool bHalfAngleCheck=normalize(inU+inV) * normalize(refFound.u+refFound.v) > 0.0f;


			CVec3 vRotHalf=Rotate(normalize(refFound.n),inNormN,normalize(refFound.u+refFound.v));

			bool bHalfAngleCheck=normalize(inU+inV) * vRotHalf > 0.0f;
// //			bool bHalfAngleCheck=normalize(normalize(inU)+normalize(inV)) * normalize(normalize(refFound.u)+normalize(refFound.v)) > 0.0f;

			if(!bHalfAngleCheck)continue;
		}

		dwBaseUVIndex=(*iFind).second;break;
	}

	if(dwBaseUVIndex==0xffffffff)														// not found
	{	
		// otherwise create a new base

		CBase33 Base( CVec3(0,0,0), CVec3(0,0,0), vNormal );

		dwBaseUVIndex = (unsigned int)m_BaseVectors.size();

		inMap.insert( std::pair<CBaseIndex,unsigned int>(Indx,dwBaseUVIndex) );
		m_BaseVectors.push_back(Base);
	}

	CBase33 &refBaseUV=m_BaseVectors[dwBaseUVIndex];

	refBaseUV.u=refBaseUV.u+inU;
	refBaseUV.v=refBaseUV.v+inV;

	//no mesh is perfect 
	if(inU.x!=0.0f || inU.y!=0.0f || inU.z!=0.0f)
		assert(refBaseUV.u.x!=0.0f || refBaseUV.u.y!=0.0f || refBaseUV.u.z!=0.0f);
	// no mesh is perfect
	if(inV.x!=0.0f || inV.y!=0.0f || inV.z!=0.0f)
		assert(refBaseUV.v.x!=0.0f || refBaseUV.v.y!=0.0f || refBaseUV.v.z!=0.0f);

	return(dwBaseUVIndex);
}




template <class InputProxy>
void CTangentSpaceCalculation<InputProxy>::AddNormal2Base( std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder> &inMap,
																												 const unsigned int indwPosNo, const unsigned int indwNormNo, const CVec3 &inNormal  )
{
	CBaseIndex Indx;

	Indx.m_dwPosNo=indwPosNo;
	Indx.m_dwNormNo=indwNormNo;

	std::multimap<CBaseIndex,unsigned int,CBaseIndexOrder>::iterator iFind = inMap.find(Indx);

	unsigned int dwBaseNIndex;

	if(iFind!=inMap.end())																// found
	{
		// resuse the existing one

		dwBaseNIndex=(*iFind).second;
	}
	else
	{
		// otherwise create a new base

		CBase33 Base( CVec3(0,0,0), CVec3(0,0,0), CVec3(0,0,0) );

		dwBaseNIndex = (unsigned int)m_BaseVectors.size();
		inMap.insert( std::pair<CBaseIndex,unsigned int>(Indx,dwBaseNIndex) );
		m_BaseVectors.push_back(Base);
	}

	CBase33 &refBaseN=m_BaseVectors[dwBaseNIndex];

	refBaseN.n=refBaseN.n+inNormal;
}



template <class InputProxy>
void CTangentSpaceCalculation<InputProxy>::GetBase( const unsigned int indwPos, float outU[3], float outV[3], float outN[3] )
{
	CBase33 &base=m_BaseVectors[indwPos];

	outU[0]=base.u.x;
	outV[0]=base.v.x;
	outN[0]=base.n.x;
	outU[1]=base.u.y;
	outV[1]=base.v.y;
	outN[1]=base.n.y;
	outU[2]=base.u.z;
	outV[2]=base.v.z;
	outN[2]=base.n.z;
}


template <class InputProxy>
void CTangentSpaceCalculation<InputProxy>::GetTriangleBaseIndices( const unsigned int indwTriNo, unsigned int outdwBase[3] )
{
	assert(indwTriNo<m_TriBaseAssigment.size());
	CTriBaseIndex &indx=m_TriBaseAssigment[indwTriNo];

	for(unsigned int i=0;i<3;i++) outdwBase[i]=indx.p[i];
}


template <class InputProxy> 
size_t CTangentSpaceCalculation<InputProxy>::GetBaseCount( void )
{
	return(m_BaseVectors.size());
}
