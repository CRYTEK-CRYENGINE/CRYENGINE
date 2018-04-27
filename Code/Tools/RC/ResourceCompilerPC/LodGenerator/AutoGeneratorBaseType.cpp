// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorBaseType.h"

namespace LODGenerator
{
	SAutoUVTri::SAutoUVTri(Vec2 &a, Vec2 &b, Vec2 &c, Vec3 &ap, Vec3 &bp, Vec3 &cp, SAutoUVVertex *va, SAutoUVVertex *vb, SAutoUVVertex *vc)
	{
		v[0]=a;
		v[1]=b;
		v[2]=c;
		pos[0]=ap;
		pos[1]=bp;
		pos[2]=cp;
		orig[0]=va;
		orig[1]=vb;
		orig[2]=vc;
	}

	SAutoUVSquare::SAutoUVSquare(int _w, int _h, float _mx, float _my, int polyStart, int polyEnd)
	{
		w=_w;
		h=_h;
		s=polyStart;
		e=polyEnd;
		mx=_mx;
		my=_my;
	}

	bool SAutoUVSquare::operator <(const SAutoUVSquare &rhs) const
	{
		int area=w*h;
		int areaRHS=rhs.w*rhs.h;
		return area>areaRHS;
	}

	void SAutoUVSquare::Rotate()
	{
		int tmp=w;
		w=h;
		h=tmp;
	}

	void SAutoUVSquare::RotateTris(std::vector<SAutoUVTri> &tris)
	{
		float newMx=(tris.begin()+s)->v[0].x;
		float newMy=(tris.begin()+s)->v[0].y;
		for (std::vector<SAutoUVTri>::iterator tit=tris.begin()+s, tend=tris.begin()+e; tit!=tend; ++tit)
		{
			tit->v[0]=tit->v[0].rot90cw();
			tit->v[1]=tit->v[1].rot90cw();
			tit->v[2]=tit->v[2].rot90cw();
			if (tit->v[0].y<newMy)
				newMy=tit->v[0].y;
			if (tit->v[1].y<newMy)
				newMy=tit->v[1].y;
			if (tit->v[2].y<newMy)
				newMy=tit->v[2].y;
			if (tit->v[0].x<newMx)
				newMx=tit->v[0].x;
			if (tit->v[1].x<newMx)
				newMx=tit->v[1].x;
			if (tit->v[2].x<newMx)
				newMx=tit->v[2].x;
		}
		mx=newMx;
		my=newMy;
	}

	SAutoUVExpandingRasteriser::SAutoUVExpandingRasteriser(float resolution)
	{
		m_min[0]=m_min[1]=m_max[0]=m_max[1]=0;
		m_resolution=1.0f/resolution;
		m_pixels=NULL;
		m_bInitialised=false;
	}

	SAutoUVExpandingRasteriser::~SAutoUVExpandingRasteriser()
	{
		if (m_pixels)
			free(m_pixels);
	}

	void SAutoUVExpandingRasteriser::ExpandToFitVertices(int bbMin[2], int bbMax[2])
	{
		if (bbMin[0]<m_min[0] || bbMin[1]<m_min[1] || bbMax[0]>m_max[0] || bbMax[1]>m_max[1])
		{
			int newMin[2], newMax[2];
			if (!m_bInitialised)
			{
				m_min[0]=m_max[0]=bbMin[0];
				m_min[1]=m_max[1]=bbMin[1];
				m_bInitialised=true;
			}
			newMin[0]=min(bbMin[0],m_min[0]);
			newMin[1]=min(bbMin[1],m_min[1]);
			newMax[0]=max(bbMax[0],m_max[0]);
			newMax[1]=max(bbMax[1],m_max[1]);
			int newSize=(newMax[0]-newMin[0])*(newMax[1]-newMin[1]);
			char *newPixels=(char*)malloc(newSize);
			memset(newPixels, 0, newSize);
			for (int y=0; y<newMax[1]-newMin[1]; y++)
			{
				if (y>=m_min[1]-newMin[1] && y<m_max[1]-newMin[1])
					memcpy(&newPixels[y*(newMax[0]-newMin[0])+m_min[0]-newMin[0]], &m_pixels[(y-(m_min[1]-newMin[1]))*(m_max[0]-m_min[0])], m_max[0]-m_min[0]);
			}
			if (m_pixels)
				free(m_pixels);
			m_pixels=newPixels;
			m_min[0]=newMin[0];
			m_min[1]=newMin[1];
			m_max[0]=newMax[0];
			m_max[1]=newMax[1];
		}
	}

	bool SAutoUVExpandingRasteriser::RayTest(Vec2 &a, Vec2 &b, Vec2 c, int x, int y)
	{
		Vec2 p(x/m_resolution, y/m_resolution);
		if ((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x)<0.0f)
			return false;
		if ((c.x-b.x)*(p.y-b.y)-(c.y-b.y)*(p.x-b.x)<0.0f)
			return false;
		if ((a.x-c.x)*(p.y-c.y)-(a.y-c.y)*(p.x-c.x)<0.0f)
			return false;
		return true;				
	}

	bool SAutoUVExpandingRasteriser::RasteriseTriangle(Vec2 &a, Vec2 &b, Vec2 &c, bool bWrite)
	{
		int bbMin[2];
		int bbMax[2];

		bbMin[0]=(int)floorf(a.x*m_resolution);
		bbMin[1]=(int)floorf(a.y*m_resolution);
		bbMax[0]=(int)ceilf(a.x*m_resolution);
		bbMax[1]=(int)ceilf(a.y*m_resolution);
		for (int i=0; i<2; i++)
		{
			Vec2 *v=(i==0)?&b:&c;
			bbMin[0]=min(bbMin[0], (int)floorf(v->x*m_resolution));
			bbMin[1]=min(bbMin[1], (int)floorf(v->y*m_resolution));
			bbMax[0]=max(bbMax[0], (int)ceilf(v->x*m_resolution));
			bbMax[1]=max(bbMax[1], (int)ceilf(v->y*m_resolution));
		}
		if (bWrite)
		{
			ExpandToFitVertices(bbMin, bbMax);
		}
		bbMin[0]=max(m_min[0], bbMin[0]);
		bbMin[1]=max(m_min[1], bbMin[1]);
		bbMax[0]=min(m_max[0], bbMax[0]);
		bbMax[1]=min(m_max[1], bbMax[1]);
		char ret=0;
		for (int y=bbMin[1]; y<bbMax[1]; y++)
		{
			for (int x=bbMin[0]; x<bbMax[0]; x++)
			{
				if (RayTest(a,b,c,x,y))
				{
					char *pix=&m_pixels[(y-m_min[1])*(m_max[0]-m_min[0])+x-m_min[0]];
					ret|=*pix;
					if (bWrite)
						*pix=1;
					else if (ret)
						return true;
				}
			}
		}
		return ret?true:false;
	}

}