// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <stdio.h>
#include <assert.h>										// assert()
#include <math.h>											// floorf()
#include "SummedAreaFilterKernel.h"		// CSummedAreaFilterKernel

CSummedAreaFilterKernel::CSummedAreaFilterKernel()
{
	m_eFilterType=eEmpty;
	m_fCorrectionFactor=0.0f;
}

// http://www.sixsigma.de/english/sixsigma/6s_e_gauss.htm
bool CSummedAreaFilterKernel::CreateFromGauss( const unsigned long indwSize )
{
	assert(indwSize>2);

	int iInit=0;

	if(!Alloc(indwSize,indwSize,&iInit))
		return false;

	for(unsigned long y=0;y<indwSize;y++)
		for(unsigned long x=0;x<indwSize;x++)
		{
			float fX=(float)(x)-(float)(indwSize)*0.5f;
			float fY=(float)(y)-(float)(indwSize)*0.5f;

			double r1=sqrt(fX*fX+fY*fY)/(indwSize*0.5-2.0);

			if(r1>1)m_pData[y*indwSize+x]=0;
			else
			{
				double fSigma=1.0/3.0;		// we aim for 6*sigma = 99,99996 of all values

				double fWeight = exp(-r1*r1/(2*fSigma*fSigma));

				fWeight-=(1.0-0.9999996);

				m_pData[y*indwSize+x]=(int)(255.0f*fWeight);
			}
		}

	m_eFilterType=eGaussBlur;
	_SumUpTableAndNormalize();
	return true;
}


// create summed area table
void CSummedAreaFilterKernel::_SumUpTableAndNormalize()
{
	for(unsigned long y=0;y<m_dwHeight;y++)
	{
		int iFromLeft=0;

		for(unsigned long x=0;x<m_dwWidth;x++)
		{
			iFromLeft+=m_pData[y*m_dwWidth+x];

			if(y!=0)m_pData[y*m_dwWidth+x]=iFromLeft+m_pData[(y-1)*m_dwWidth+x];
				else m_pData[y*m_dwWidth+x]=iFromLeft;
		}
	}

	m_fCorrectionFactor=1.0f/((float)m_pData[m_dwHeight*m_dwWidth-1]);
}


// windows size 16x16 = radius 8
bool CSummedAreaFilterKernel::CreateFromSincCalc( const unsigned long indwSize )
{
	assert(indwSize>2);

	int iInit=0;

	if(!Alloc(indwSize,indwSize,&iInit))
		return false;

	for(unsigned long y=0;y<indwSize;y++)
		for(unsigned long x=0;x<indwSize;x++)
		{
			float fX=(float)(x-indwSize*0.5f);
			float fY=(float)(y-indwSize*0.5f);

			float r1=sqrtf(fX*fX+fY*fY)/(indwSize*0.5f-2.0f);

			if(r1>1.0f)m_pData[y*indwSize+x]=0;
			else
			{
				r1*=3.1415926535897932384626433832795f;

				float r2=r1*8.0f;

				// http://home.no.net/dmaurer/~dersch/interpolator/interpolator.html
				// weight = [ sin(x*¼) / (x*¼) ] * [ sin(x*¼ / 8) / (x*¼/8) ]

				// http://www.binbooks.com/books/photo/i/l/57186AF8DE
				// sinc(x) = sin(pi * x) / (pi * x)
				// L8interp(x) = sinc(x) * sinc(x/8)  if  abs(x) <= 8
				//             = 0                    if  abs(x) > 8

				float fWeight=(sinf(r1)*sinf(r2)) / (r1*r2);

				m_pData[y*indwSize+x]=(int)(255.0f*fWeight);
			}
		}

	m_eFilterType=eSinc;
	_SumUpTableAndNormalize();

	return true;
}





bool CSummedAreaFilterKernel::CreateFromRAWFile( const char *filename, const unsigned long indwSize, const int iniMidValue )
{
	assert(iniMidValue>=0 && iniMidValue<255);

	int iInit=0;

	if(!Alloc(indwSize,indwSize,&iInit))return false;

	FILE *in=fopen(filename,"rb");			if(!in)return false;

	for(unsigned long y=0;y<m_dwHeight;y++)
	for(unsigned long x=0;x<m_dwWidth;x++)
	{
		unsigned char val;

		if(fread(&val,1,1,in)!=1){ fclose(in);return false; }

		m_pData[y*m_dwWidth+x]=(int)val-iniMidValue;
	}

	fclose(in);

	m_eFilterType=eRAW;
	_SumUpTableAndNormalize();

	return true;
}


std::string CSummedAreaFilterKernel::GetInfoString( void ) const
{
	std::string sRet="FilterKernel(";

	switch(m_eFilterType)
	{
		case eEmpty:				sRet+="Empty"; break;
		case eSinc:					sRet+="Sinc16x16"; break;
		case eRAW:					sRet+="RAW"; break;
		case eGaussBlur:		sRet+="GaussBlur"; break;
		case eGaussSharp:		sRet+="GaussSharp"; break;
		default: assert(0);
	}

	sRet+=")";

	return(sRet);
}

float CSummedAreaFilterKernel::GetAreaNonAA( float infAx, float infAy, float infDx, float infDy ) const
{
	assert(m_eFilterType!=eEmpty);

	int ax=(int)floorf(infAx*127.5f+127.5f);	
	int ay=(int)floorf(infAy*127.5f+127.5f);	
	int dx=(int)floorf(infDx*127.5f+127.5f);	
	int dy=(int)floorf(infDy*127.5f+127.5f);	

	if(ax<0)ax=0;	else if(ax>255)ax=255;
	if(dx<0)dx=0;	else if(dx>255)dx=255;
	if(ay<0)ay=0;	else if(ay>255)ay=255;
	if(dy<0)dy=0;	else if(dy>255)dy=255;

	unsigned long area=m_pData[dy*m_dwWidth+dx]-m_pData[dy*m_dwWidth+ax]-m_pData[ay*m_dwWidth+dx]+m_pData[ay*m_dwWidth+ax];

	return(m_fCorrectionFactor*(float)area);
}



// optimizable
float CSummedAreaFilterKernel::GetAreaAA( float infAx, float infAy, float infDx, float infDy ) const
{
	assert(m_eFilterType!=eEmpty);

	infAx = infAx*127.5f+127.5f;	
	infAy = infAy*127.5f+127.5f;	
	infDx = infDx*127.5f+127.5f;	
	infDy = infDy*127.5f+127.5f;	

	float fSum=_GetBilinearFiltered(infDx,infDy)
						-_GetBilinearFiltered(infAx,infDy)
						-_GetBilinearFiltered(infDx,infAy)
						+_GetBilinearFiltered(infAx,infAy);

	return(fSum*m_fCorrectionFactor);
}


float CSummedAreaFilterKernel::_GetBilinearFiltered( const float infX, const float infY ) const
{
	float fIX=floorf(infX),	fIY=floorf(infY);
	float fFX=infX-fIX,			fFY=infY-fIY;
	int		iX=(int)fIX,			iY=(int)fIY;

	if(iX<0)iX=0;	else if(iX>254)iX=254;
	if(iY<0)iY=0; else if(iY>254)iY=254;

	float fArea= m_pData[    iY*m_dwWidth+iX    ] * ((1.0f-fFX)*(1.0f-fFY))		// left top
							+m_pData[    iY*m_dwWidth+iX+1  ] * ((     fFX)*(1.0f-fFY))		// right top
							+m_pData[(iY+1)*m_dwWidth+iX    ] * ((1.0f-fFX)*(     fFY))		// left bottom
							+m_pData[    iY*m_dwWidth+iX+257] * ((     fFX)*(     fFY));		// right bottom

	return(fArea);
}



bool CSummedAreaFilterKernel::CreateWeightFilter( CSimpleBitmap<float> &outFilter, const float infX, const float infY,
	const float infWeight, const float infR ) const
{
	assert(infX>=0.0f);
	assert(infX<1.0f);
	assert(infY>=0.0f);
	assert(infY<1.0f);
	assert(infWeight>=0.0f);
	assert(infR>0.0f);

	float fLeftTop=ceilf(infR);
	int iSide=2*(int)fLeftTop+1;

	float fInit=0.0f;

	if(!outFilter.Alloc(iSide,iSide,&fInit))return false;

	AddWeights(outFilter,infX+fLeftTop,infY+fLeftTop,infWeight,infR);

	return true;
}


bool CSummedAreaFilterKernel::CreateWeightFilterBlock( CSimpleBitmap<float> &outFilter, const unsigned long indwSideLength,
	const float infR ) const
{
	assert(indwSideLength>=0);
	assert(infR>0.0f);

	float fLeftTop=ceilf(infR);
	int iSide=2*(int)fLeftTop+1;

	float fInit=0.0f;

	if(!outFilter.Alloc(iSide,iSide,&fInit))return false;

	float fStep=1.0f/(float)indwSideLength;
	float fHalf=fStep*0.5f;

	float fWeight=fStep*fStep;

	for(float y=fHalf;y<1.0f;y+=fStep)
	for(float x=fHalf;x<1.0f;x+=fStep)
		AddWeights(outFilter,x+fLeftTop,y+fLeftTop,fWeight,infR);

	// check
#ifdef _DEBUG
	float fSum=0.0f;
	for(int y=0;y<iSide;y++)
	for(int x=0;x<iSide;x++)
	{
		float f;

		outFilter.Get(x,y,f);
		fSum+=f;
	}
	assert(fSum>=0.98f);
	assert(fSum<=1.02f);
#endif

	return true;
}


void CSummedAreaFilterKernel::AddWeights( CSimpleBitmap<float> &inoutFilter, const float infX, const float infY,
	const float infWeight, const float infR ) const
{
	assert(infWeight>=0.0f);
	assert(infR>0.0f);

	if(infWeight<=0.0f)return;

	float fInvR=1.0f/infR;
	float	sx=floorf(infX-infR);
	float	sy=floorf(infY-infR);

	int	iax=(int)sx;
	int	iay=(int)sy;
	int	iex=(int)ceilf(infX+infR);
	int	iey=(int)ceilf(infY+infR);

	float x,y;
	int ix,iy;

	for(iy=iay,y=(sy-infY)*fInvR;iy<=iey;iy++,y+=fInvR)
	for(ix=iax,x=(sx-infX)*fInvR;ix<=iex;ix++,x+=fInvR)
	{
		float fArea=GetAreaAA(x,y,x+fInvR,y+fInvR);							// better quality
//			float fArea=m_Filter.GetAreaNonAA(x,y,x+fInvR,y+fInvR);				// faster

//			assert(fArea<=1.0f);			// may be wrong if we use sharpening filter

		float fOldVal; 

		inoutFilter.Get(ix,iy,fOldVal);
		inoutFilter.Set(ix,iy,fOldVal + fArea*infWeight );
	}
}



