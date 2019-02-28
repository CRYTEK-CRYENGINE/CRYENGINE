
#if defined(OFFLINE_COMPUTATION)

#pragma once

#pragma warning (disable : 4127) 

#include "RasterTable.h"									// CRasterTable
#include <PRT/PRTTypes.h>
#include <CryMath/Cry_Math.h>
#include <limits>

//#define SUPPORT_COPY_CONSTRUCTOR	//since referencing is used, dont support copy constructor(union becomes illegal)

#undef min	//just to make sure
#undef max	//just to make sure

template <class T, class T2> class _Matrix34C;
template <class T> class _Vector2dC;

namespace NSH
{
	enum EReturnSinkValue;
}

template < class T > class _Vector3dC
{
public:
    T p[3];

	// default constructor
						_Vector3dC			();
	// constructor
						_Vector3dC			( T x, T y, T z );
						_Vector3dC			( const _Vector3dC<T> &x );
						_Vector3dC			( const _Vector2dC<T> &x );
						_Vector3dC			( const Vec3 &x ) { p[0]=(T)x.x;p[1]=(T)x.y;p[2]=(T)x.z; }
						_Vector3dC			( const T &x );

    void				Set					( T x, T y, T z );

    void				Max					( const _Vector3dC<T> &c );	
    void				Min					( const _Vector3dC<T> &c );

const _Vector3dC<T> 	operator-			() const;

	  _Vector3dC<T>		operator-			( const _Vector3dC<T> &other ) const;
	  _Vector3dC<T>		operator-=			( const _Vector3dC<T> &other );
	  _Vector3dC<T>		operator+			( const _Vector3dC<T> &other ) const;
	  _Vector3dC<T>		operator+=			( const _Vector3dC<T> &other );
    T					operator*			( const _Vector3dC<T> &other ) const;
	  _Vector3dC<T>		operator*			( const T fak ) const;
	  _Vector3dC<T>		operator*=			( const T fak );
	  _Vector3dC<T>		operator/=			( const T fak );
	bool				operator==			( const _Vector3dC<T> &outher ) const;

    T					Length				() const;
    double				LengthQuad			() const;
    void				Normalize			();	

	// a=thumb, b=zeigefinger, b=mittelfinger mit rechter Hand
friend _Vector3dC<T> 	CrossProd			( const _Vector3dC<T> &a, const _Vector3dC<T> &b );	// Kreuzprodukt

friend _Vector3dC<T>	operator*			( const T t, const _Vector3dC<T> &a );
};

template <class T> 
_Vector3dC<T>::_Vector3dC()
{}

template <class T> 
_Vector3dC<T>::_Vector3dC( T x, T y, T z )
{
    p[0]=x;p[1]=y;p[2]=z;
}

template <class T> 
_Vector3dC<T>::_Vector3dC( const T &x )
{
	assert(x==0);		// sonst macht das glaub ich keinen Sinn
    p[0]=x;p[1]=x;p[2]=x;
}

template <class T> 
_Vector3dC<T>::_Vector3dC( const _Vector3dC<T> &x )
{
    p[0]=x.p[0];p[1]=x.p[1];p[2]=x.p[2];
}

template <class T> 
_Vector3dC<T>::_Vector3dC( const _Vector2dC<T> &x )
{
    p[0]=x.p[0];p[1]=x.p[1];p[2]=0;
}

template <class T> 
void _Vector3dC<T>::Set( T x, T y, T z )
{
    p[0]=x;p[1]=y;p[2]=z;
}

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator-( const _Vector3dC<T> &b ) const
{
	return _Vector3dC<T>(p[0]-b.p[0],p[1]-b.p[1],p[2]-b.p[2] );
};

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator-=( const _Vector3dC<T> &b )
{
	p[0]-=b.p[0];p[1]-=b.p[1];p[2]-=b.p[2];

	return *this;
}

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator+( const _Vector3dC<T> &b ) const
{
	return _Vector3dC<T>(p[0]+b.p[0],p[1]+b.p[1],p[2]+b.p[2] );
}

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator+=( const _Vector3dC<T> &b )
{
	p[0]+=b.p[0];p[1]+=b.p[1];p[2]+=b.p[2];

	return *this;
}

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator*( const T f ) const
{
	return _Vector3dC<T>(p[0]*f,p[1]*f,p[2]*f );
}

template <class T> 
T _Vector3dC<T>::operator*( const _Vector3dC<T> &b ) const
{
	return(p[0]*b.p[0]+p[1]*b.p[1]+p[2]*b.p[2]);
}

template <class T> 
bool _Vector3dC<T>::operator==( const _Vector3dC<T> &other ) const
{
	if(p[0]!=other.p[0])return(false);
	if(p[1]!=other.p[1])return(false);
	if(p[2]!=other.p[2])return(false);

	return(true);
}

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator*=( const T f )
{
	p[0]*=f;p[1]*=f;p[2]*=f;

	return(*this);
}

template <class T> 
_Vector3dC<T> _Vector3dC<T>::operator/=( const T f )
{
	T fInv=1/f;

	p[0]*=fInv;p[1]*=fInv;p[2]*=fInv;

	return(*this);
}

template <class T> 
_Vector3dC<T> CrossProd( const _Vector3dC<T> &a, const _Vector3dC<T> &b )
{
	_Vector3dC<T> ret;

    ret.p[0]=a.p[1]*b.p[2] - a.p[2]*b.p[1];
    ret.p[1]=a.p[2]*b.p[0] - a.p[0]*b.p[2];
    ret.p[2]=a.p[0]*b.p[1] - a.p[1]*b.p[0];

	return(ret);
}

template <class T> 
void _Vector3dC<T>::Normalize()
{
    T len=1.0f/Length();

    p[0]*=len;p[1]*=len;p[2]*=len;
}

template <class T> 
T _Vector3dC<T>::Length() const
{
    return( (T)sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]) );
}

// unary negation
template <class T> 
const _Vector3dC<T> _Vector3dC<T>::operator-() const
{
	return(_Vector3dC<T>(-p[0],-p[1],-p[2]));
}

typedef _Vector3dC<double> TVector3D;

//! /param inT
//! /return 0/1/2 plane axis that was hit
inline int GetBiggestValue(double inT[3])
{
	// get the biggest value

	if (inT[0] > inT[1])
	{
		if (inT[2] > inT[0])
			return(2);
		else
			return(0);
	}
	else
	{
		if (inT[2] > inT[1])
			return(2);
		else
			return(1);
	}
}

template <class T>
class CPossibleIntersectionSink
{
public:
	//! return one element from the bucket
	//! /param inObject 
	//! /param rInoutfRayMaxDist in and out value for max. shooting distance or FLT_MAX, if you found a interesection you may update this because further intersections are no longer interesting
	//! /return true=do further processing, false=stop I found what I need (e.g. any intersection for shadow testing)
	const NSH::EReturnSinkValue ReturnElement( const T &crInObject, float &rInoutfRayMaxDist);

	//! 
	void EndReturningBucket()
	{}
};
 
template <class T, bool bUseXRaster = true>
class CRasterCube	
{
	typedef CRasterCube<T,bUseXRaster> TRasterCubeTempl;
public:
	INSTALL_CLASS_NEW(TRasterCubeTempl)
	
	CRasterCube();		//!< default constructor
#if defined(SUPPORT_COPY_CONSTRUCTOR)
	CRasterCube(const CRasterCube& crCopyFrom);//!< copy constructor
#endif
	//! destructor
	virtual ~CRasterCube();

	//! alloc raster, after that Phase 1 has started
	//! /param invMin minimum values for the bounding box
	//! /param invMax maximum values for the bounding box
	//! /param inElementCount element count for the data structure size estimation
	//! /param infMagnifier scale factor for internal data structures (scales ~ quadratic in memory consumptions)
	//! /return true=success, false otherwise
	bool Init( const TVector3D invMin, const TVector3D invMax, uint32 inElementCount, float infMagnifier=1.0f );

	//! free data	
	void DeInit();

	//! Has to be called in Phase1, after that Phase 2 has started
	//! /param inbDebug debug output is generated
	//! /return true=success, false otherwise
	bool PreProcess( const bool inbDebug );

	//! call this per triangle after Init and before PreProcess
	//! after PreProcess call it again for every triangle
	//! /param invVertex
	//! /param inpElement
	void PutInTriangle( const Vec3 invVertex[3], const T &inElement );

	//!
	//! /param outdwSize
	//! /return memory consumption in bytes O(1)
	uint32 CalcMemoryConsumption( uint32 outdwSize[3] );

	// this works because m_pExtraMemoryPool is sorted in ascending order
	// returns true if inSink.ReturnElement returns false(meaning stop processing), does not bother if bBreakAfterFirstHit = false
	template <class Sink, bool bBreakAfterFirstHit>
	const bool GatherElementsAt( int iniPos[3], Sink &inSink, float& rfRet )const
	{
		rfRet = std::numeric_limits<float>::max();

		const uint32 *pElX = 0;
		if(bUseXRaster)
		{
			pElX=m_XRaster.GetElementsAt(iniPos[1],iniPos[2]);	// YZ
			if(!pElX)return(false);
		}

		const uint32 *pElY=m_YRaster.GetElementsAt(iniPos[2],iniPos[0]);	// ZX
		if(!pElY)return(false);
		const uint32 *pElZ=m_ZRaster.GetElementsAt(iniPos[0],iniPos[1]);	// XY
		if(!pElZ)return(false);

		if(bUseXRaster)
			assert(*pElX);
		assert(*pElY);
		assert(*pElZ);

		// get the intersection of all three lists
		for(;;)
		{ 
			// increase pointer to the biggest values
			if(bUseXRaster)
			{
				if(*pElY>*pElX)
				{
					if(*pElZ>*pElY){ pElZ++;if(*pElZ==0)break; }
					else { pElY++;if(*pElY==0)break; }
				}
				else
				{
					if(*pElZ>*pElX){ pElZ++;if(*pElZ==0)break; }
					else
					{
						if(*pElX==*pElY && *pElY==*pElZ)
						{
							if(bBreakAfterFirstHit)
							{
								if(NSH::RETURN_SINK_FAIL != inSink.ReturnElement(m_vElements[(*pElX++)-1],rfRet))
								{
									inSink.EndReturningBucket();
									return true;
								}
							}
							else
							if(NSH::RETURN_SINK_HIT_AND_EARLY_OUT == inSink.ReturnElement(m_vElements[(*pElX++)-1],rfRet))
							{
								inSink.EndReturningBucket();
								return true;
							}
							if(*pElX==0)break;
							pElY++;if(*pElY==0)break;
							pElZ++;if(*pElZ==0)break;
						}
						else { pElX++;if(*pElX==0)break; }
					}
				}
			}
			else//bUseXRaster = false
			{
				if(*pElZ > *pElY)
				{ 
					pElZ++;
					if(*pElZ==0)
						break; 
				}
				else 
				{ 
					if(*pElZ < *pElY)
					{
						pElY++;
						if(*pElY==0)
							break; 
					}
					else	//*pElY == *pElZ
					{	
						if(bBreakAfterFirstHit)
						{
							if(NSH::RETURN_SINK_FAIL != inSink.ReturnElement(m_vElements[(*pElY++)-1],rfRet))
							{
								inSink.EndReturningBucket();
								return true;
							}
						}
						else
						if(NSH::RETURN_SINK_HIT_AND_EARLY_OUT == inSink.ReturnElement(m_vElements[(*pElY++)-1],rfRet))
						{
							inSink.EndReturningBucket();
							return true;
						}
						if(*pElY==0)
							break;
						pElZ++;
						if(*pElZ==0)
							break;
					}
				}
			}
		}

		inSink.EndReturningBucket();

		return(false); 
	}

	// raytracing, gather unsorted hits (sorted could give faster access to the first hit)
	// call ClearListBefore and GetHitList afterwards
	// if bBreakAfterFirstHit=true it returns immediately if GatherElementsAt returns true (this is when inSink.ReturnElement returns false(meaning stop processing))
	template <class Sink, bool bBreakAfterFirstHit>
	void GatherRayHitsDirection( const TVector3D _invStart, const TVector3D _invDir, Sink &inSink, const float infRayMax = std::numeric_limits<float>::max()) const
	{
		TVector3D invStart,vDirRelative;				// scaled to the internal representation

		// transform the ray into the RasterCubeSpace
		for(int i=0;i<3;i++)
		{
			double fScale=(((double)(m_iSize[i]))/((double)m_vMax.p[i]-(double)m_vMin.p[i]));
			invStart.p[i]=(_invStart.p[i]-m_vMin.p[i])*fScale; vDirRelative.p[i]=_invDir.p[i]*fScale;
		}

		TVector3D vDir=vDirRelative;

		int iPos[3]={ (int)floor(invStart.p[0]),(int)floor(invStart.p[1]),(int)floor(invStart.p[2]) };	// floor done

		// integer direction
		int iDir[3]={ -1,-1,-1 };

		TVector3D vPosInCube( fmod(fabs(invStart.p[0]),1),fmod(fabs(invStart.p[1]),1),fmod(fabs(invStart.p[2]),1) );

		if(vDir.p[0]>0){ iDir[0]=1;vDir.p[0]=-vDir.p[0];vPosInCube.p[0]=1.0f-vPosInCube.p[0]; }
		if(vDir.p[1]>0){ iDir[1]=1;vDir.p[1]=-vDir.p[1];vPosInCube.p[1]=1.0f-vPosInCube.p[1]; }
		if(vDir.p[2]>0){ iDir[2]=1;vDir.p[2]=-vDir.p[2];vPosInCube.p[2]=1.0f-vPosInCube.p[2]; }

		// make sure it's in the range 0..1
		if(vPosInCube.p[0]<=0)vPosInCube.p[0]++;
		if(vPosInCube.p[1]<=0)vPosInCube.p[1]++;
		if(vPosInCube.p[2]<=0)vPosInCube.p[2]++;

		assert(vPosInCube.p[0]>0);
		assert(vPosInCube.p[1]>0);
		assert(vPosInCube.p[2]>0);
		assert(vPosInCube.p[0]<=1.0f);
		assert(vPosInCube.p[1]<=1.0f);
		assert(vPosInCube.p[2]<=1.0f);


		// calculate T and StepT
		double fT[3],fTStep[3];

		if(vDir.p[0]!=0.0)
			{ fTStep[0]=1.0/vDir.p[0];fT[0]=vPosInCube.p[0]*fTStep[0]; } 
		 else 
			{ fTStep[0]=0.0;fT[0] = std::numeric_limits<float>::min(); }
		
		if(vDir.p[1]!=0.0)
			{ fTStep[1]=1.0/vDir.p[1];fT[1]=vPosInCube.p[1]*fTStep[1]; } 
		 else 
			{ fTStep[1]=0.0;fT[1] = std::numeric_limits<float>::min(); }
		
		if(vDir.p[2]!=0.0)
			{ fTStep[2]=1.0/vDir.p[2];fT[2]=vPosInCube.p[2]*fTStep[2]; } 
		 else 
			{ fTStep[2]=0.0;fT[2] = std::numeric_limits<float>::min(); }

		float fRayMax=infRayMax;

		int iEndBorder[3];

		CalcNewBorder(invStart,vDirRelative,fRayMax,iDir,iEndBorder);

		float fNewRayMax;
		for(;;)
		{
			// get elements if we are in the block
			if((uint32)iPos[0]<(uint32)m_iSize[0])
				if((uint32)iPos[1]<(uint32)m_iSize[1])
					if((uint32)iPos[2]<(uint32)m_iSize[2])
					{
						if(GatherElementsAt<Sink, bBreakAfterFirstHit>(iPos,inSink, fNewRayMax))
							return;//one valid hit found, stop here
						// new Border
						if(fNewRayMax<fRayMax)
						{
							fRayMax=fNewRayMax;

							CalcNewBorder(invStart,vDirRelative,fRayMax,iDir,iEndBorder);
						}
					}

			// advance position
			int iAxis=GetBiggestValue(fT);

			fT[iAxis]+=fTStep[iAxis];
			iPos[iAxis]+=iDir[iAxis];

			// stop on border hit
			if(iDir[0]>0){ if(iPos[0]>=iEndBorder[0])break; }
				else { if(iPos[0]<=iEndBorder[0])break; }
			if(iDir[1]>0){ if(iPos[1]>=iEndBorder[1])break; }
				else { if(iPos[1]<=iEndBorder[1])break; }
			if(iDir[2]>0){ if(iPos[2]>=iEndBorder[2])break; }
				else { if(iPos[2]<=iEndBorder[2])break; }
		}
	}

private:

	TVector3D												m_vMin;						//!< bounding box min from Init() parameters)
	TVector3D												m_vMax;						//!< bounding box max from Init() parameters)
	int															m_iSize[3];				//!< integer size of the internal raster images
	std::vector<T, CSHAllocator<T> > m_vElements;			//!< index to the elements that are put in

	CRasterTable<uint32>						m_XRaster;				//!< YZ internal raster images - index-1 to m_vElements because 0 is used for termination
	CRasterTable<uint32>						m_YRaster;				//!< ZX internal raster images - index-1 to m_vElements because 0 is used for termination
	CRasterTable<uint32>						m_ZRaster;				//!< XY internal raster images - index-1 to m_vElements because 0 is used for termination

#if !defined(SUPPORT_COPY_CONSTRUCTOR)
	CRasterCube(const CRasterCube& crCopyFrom);//!< copy constructor
#endif

	void CalcNewBorder( TVector3D &invStart, TVector3D &invDirRelative, float infRayMax, int iniDir[3], int outiNewBorder[3] )const
	{
		if(infRayMax == std::numeric_limits<float>::max())
		{
			if(iniDir[0]>0)outiNewBorder[0]=m_iSize[0]; else outiNewBorder[0]=-1;
			if(iniDir[1]>0)outiNewBorder[1]=m_iSize[1]; else outiNewBorder[1]=-1;
			if(iniDir[2]>0)outiNewBorder[2]=m_iSize[2]; else outiNewBorder[2]=-1;
			return;
		}

		TVector3D vNewEnd=invStart + invDirRelative*infRayMax;

		for(int i=0;i<3;i++)
		{
			outiNewBorder[i]=(int)floor(vNewEnd.p[i])+iniDir[i];

			if(outiNewBorder[i]<-1)outiNewBorder[i]=-1;

			if(outiNewBorder[i]>m_iSize[i])outiNewBorder[i]=m_iSize[i];
		}
	}
};

// constructor
template <class T, bool bUseXRaster>
CRasterCube<T, bUseXRaster>::CRasterCube()
{
	DeInit();
}

#if defined(SUPPORT_COPY_CONSTRUCTOR)
	template <class T, bool bUseXRaster>
	CRasterCube<T, bUseXRaster>::CRasterCube(const CRasterCube<T, bUseXRaster>& crCopyFrom) : 
		m_vMin(crCopyFrom.m_vMin), m_vMax(crCopyFrom.m_vMax), m_vElements(crCopyFrom.m_vElements), m_YRaster(crCopyFrom.m_YRaster), m_ZRaster(crCopyFrom.m_ZRaster)
	{
		m_iSize[0] = crCopyFrom.m_iSize[0];
		m_iSize[1] = crCopyFrom.m_iSize[1];
		m_iSize[2] = crCopyFrom.m_iSize[2];
		if(bUseXRaster)
			m_XRaster = crCopyFrom.m_XRaster;
	}
#endif

// destructor
template <class T, bool bUseXRaster>
CRasterCube<T, bUseXRaster>::~CRasterCube()
{
	DeInit();
}


// free data	
template <class T, bool bUseXRaster>
void CRasterCube<T, bUseXRaster>::DeInit()
{
	if(bUseXRaster)
		m_XRaster.DeInit();
	m_YRaster.DeInit();
	m_ZRaster.DeInit();

	m_iSize[0]=m_iSize[1]=m_iSize[2]=0;
}

// alloc raster, after that Phase 1 has started
template <class T, bool bUseXRaster>
bool CRasterCube<T, bUseXRaster>::Init( const TVector3D invMin, const TVector3D invMax, uint32 inElementCount, float infMagnifier )
{
	m_vElements.reserve(inElementCount);

	TVector3D vSize;

	m_vMin=invMin;
	m_vMax=invMax;
	vSize=m_vMax-m_vMin;

	float fAvgSize=((float)vSize.p[0]+(float)vSize.p[1]+(float)vSize.p[2])/3.0f;

	if(fAvgSize==0.0f)return(false);

	float fMassSideLength=(float)sqrt((double)inElementCount) * infMagnifier;

	m_iSize[0]=(int)(vSize.p[0]/fAvgSize * fMassSideLength)+2;
	m_iSize[1]=(int)(vSize.p[1]/fAvgSize * fMassSideLength)+2;
	m_iSize[2]=(int)(vSize.p[2]/fAvgSize * fMassSideLength)+2;

	/*
	char str[256];

	sprintf(str,"CRasterCube<T>::Init %dx%dx%d\n",m_iSize[0],m_iSize[1],m_iSize[2]);
	OutputDebugString(str);
	*/

	TVector3D vBorder=TVector3D((vSize.p[0]+1.0f)/(float)m_iSize[0],(vSize.p[1]+1.0f)/(float)m_iSize[1],(vSize.p[2]+1.0f)/(float)m_iSize[2]);

	m_vMin-=vBorder*2;
	m_vMax+=vBorder*2;

	const int iMinimumSize=3;

	if(m_iSize[0]<iMinimumSize)m_iSize[0]=iMinimumSize;
	if(m_iSize[1]<iMinimumSize)m_iSize[1]=iMinimumSize;
	if(m_iSize[2]<iMinimumSize)m_iSize[2]=iMinimumSize;
	if(bUseXRaster)
		if(!m_XRaster.Init(m_iSize[1],m_iSize[2]))	// YZ
			return(false);
	if(!m_YRaster.Init(m_iSize[2],m_iSize[0]))	// ZX
		return(false);
	if(!m_ZRaster.Init(m_iSize[0],m_iSize[1]))	// XY
		return(false);

	return(true);
}

// call this per triangle after Init and before PreProcess
// after PreProcess call it again for every triangle
template <class T, bool bUseXRaster>
void CRasterCube<T, bUseXRaster>::PutInTriangle( const Vec3 invVertices[3], const T &inpElement )
{
	float fX[3],fY[3];
	double fScale[3];
	uint32 dwElementNo=(uint32)(m_vElements.size()+1);			// +1 because 0 is used for termination
	
	fScale[0]=(((double)(m_iSize[0]))/(m_vMax.p[0]-m_vMin.p[0]));
	fScale[1]=(((double)(m_iSize[1]))/(m_vMax.p[1]-m_vMin.p[1]));
	fScale[2]=(((double)(m_iSize[2]))/(m_vMax.p[2]-m_vMin.p[2]));
	if(bUseXRaster)
	{
		fX[0]=(float)((invVertices[0].y-m_vMin.p[1])*fScale[1]);fY[0]=(float)((invVertices[0].z-m_vMin.p[2])*fScale[2]);
		fX[1]=(float)((invVertices[1].y-m_vMin.p[1])*fScale[1]);fY[1]=(float)((invVertices[1].z-m_vMin.p[2])*fScale[2]);
		fX[2]=(float)((invVertices[2].y-m_vMin.p[1])*fScale[1]);fY[2]=(float)((invVertices[2].z-m_vMin.p[2])*fScale[2]);
		m_XRaster.PutInTriangle(fX,fY,dwElementNo);
	}
	fX[0]=(float)((invVertices[0].z-m_vMin.p[2])*fScale[2]);fY[0]=(float)((invVertices[0].x-m_vMin.p[0])*fScale[0]);
	fX[1]=(float)((invVertices[1].z-m_vMin.p[2])*fScale[2]);fY[1]=(float)((invVertices[1].x-m_vMin.p[0])*fScale[0]);
	fX[2]=(float)((invVertices[2].z-m_vMin.p[2])*fScale[2]);fY[2]=(float)((invVertices[2].x-m_vMin.p[0])*fScale[0]);
	m_YRaster.PutInTriangle(fX,fY,dwElementNo);

	fX[0]=(float)((invVertices[0].x-m_vMin.p[0])*fScale[0]);fY[0]=(float)((invVertices[0].y-m_vMin.p[1])*fScale[1]);
	fX[1]=(float)((invVertices[1].x-m_vMin.p[0])*fScale[0]);fY[1]=(float)((invVertices[1].y-m_vMin.p[1])*fScale[1]);
	fX[2]=(float)((invVertices[2].x-m_vMin.p[0])*fScale[0]);fY[2]=(float)((invVertices[2].y-m_vMin.p[1])*fScale[1]);
	m_ZRaster.PutInTriangle(fX,fY,dwElementNo);

	m_vElements.push_back(inpElement);
}



// Has to be called in Phase1, after that Phase 2 has started
template <class T, bool bUseXRaster>
bool CRasterCube<T, bUseXRaster>::PreProcess( const bool inbDebug )
{
	if(inbDebug)
	{
		if(bUseXRaster)
			m_XRaster.Debug("CRasterCubeDebugX.tga");
		m_YRaster.Debug("CRasterCubeDebugY.tga");
		m_ZRaster.Debug("CRasterCubeDebugZ.tga");
	}

	if(bUseXRaster)
		if(!m_XRaster.PreProcess()) return(false);
	if(!m_YRaster.PreProcess())	return(false);
	if(!m_ZRaster.PreProcess())	return(false);

	return(true);
}

// calculates memory consumption in bytes O(1)
template <class T, bool bUseXRaster>
uint32 CRasterCube<T, bUseXRaster>::CalcMemoryConsumption( uint32 outdwSize[3] )
{
	outdwSize[0]=m_iSize[0];
	outdwSize[1]=m_iSize[1];
	outdwSize[2]=m_iSize[2];
	if(bUseXRaster)
		return(m_XRaster.CalcMemoryConsumption()+m_YRaster.CalcMemoryConsumption()+m_ZRaster.CalcMemoryConsumption()+sizeof(*this));
	else
		return(m_YRaster.CalcMemoryConsumption()+m_ZRaster.CalcMemoryConsumption()+sizeof(*this));
}

#pragma warning (default : 4127)

#endif