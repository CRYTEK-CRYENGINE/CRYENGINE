// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <assert.h>
#include <vector>           // STL vector
#include <CryCore/Platform/platform.h>       // uint32
#include <CryMath/Cry_Math.h>       // uint32
#include "Util.h"           // getMin()

enum EImageFilteringMode
{
	eifm2DBorder = 0,
	eifmCubemapFilter = 1,
};

namespace
{
	enum ECubeFace
	{
		ecfPosX = 0,
		ecfNegX = 1,
		ecfPosY = 2,
		ecfNegY = 3,
		ecfPosZ = 4,
		ecfNegZ = 5,
		ecfUnknown = -1,
	};

	struct JumpEntry
	{
		ECubeFace face;
		int rot;
	};

	static const JumpEntry XJmpTable[] =
	{
		{ecfNegZ, 0}, {ecfPosZ, 2},     // ecfPosXa
		{ecfPosZ, 0}, {ecfNegZ, 2},     // ecfNegXa
		{ecfPosX, 1}, {ecfNegX, 3},     // ecfPosYa
		{ecfPosX, 3}, {ecfNegX, 1},     // ecfNegYa
		{ecfPosX, 0}, {ecfNegX, 0},     // ecfPosZa
		{ecfPosX, 2}, {ecfNegX, 2}      // ecfNegZa
	};

	static const JumpEntry YJmpTable[] =
	{
		{ecfPosY, 3}, {ecfNegY, 1},     // ecfPosXa
		{ecfPosY, 1}, {ecfNegY, 3},     // ecfNegXa
		{ecfNegZ, 2}, {ecfPosZ, 0},     // ecfPosYa
		{ecfPosZ, 0}, {ecfNegZ, 2},     // ecfNegYa
		{ecfNegY, 0}, {ecfPosY, 2},     // ecfPosZa
		{ecfNegY, 2}, {ecfPosY, 0}      // ecfNegZa
	};
}

//! memory block used as bitmap
//! if you might need mipmaps please consider using ImageObject instead
template <class RasterElement>
class CSimpleBitmap
{
public:

	CSimpleBitmap() 
		: m_dwWidth(0)
		, m_dwHeight(0)
	{
	}

	~CSimpleBitmap()
	{
	}

	// copy constructor
	explicit CSimpleBitmap(const CSimpleBitmap<RasterElement>& rhs)
		: m_dwWidth(0)
		, m_dwHeight(0)
	{
		*this = rhs;       // call assignment operator
	}

	// assignment operator
	CSimpleBitmap<RasterElement> &operator=(const CSimpleBitmap<RasterElement>& rhs)
	{
		if (&rhs != this)
		{
			m_data = rhs.m_data;
			m_dwWidth = rhs.m_dwWidth;
			m_dwHeight = rhs.m_dwHeight;
		}
		return *this;
	}

	//! free all the memory resources
	void FreeData()
	{
		m_data = std::vector<RasterElement>();
		m_dwWidth = 0;
		m_dwHeight = 0;
	}

	//! /return true=success, false=failed because of low memory
	bool SetSize(const uint32 indwWidth, const uint32 indwHeight)
	{
		if (m_dwWidth * m_dwHeight != indwWidth * indwHeight)
		{
			FreeData();
			m_data.resize(indwWidth * indwHeight);
			m_dwWidth = indwWidth;
			m_dwHeight = indwHeight;
		}
		return true;
	}

private:
	ECubeFace JumpX(const ECubeFace srcFace, const bool isdXPos, int* rotCoords) const
	{
		int index = (int)srcFace*2+(isdXPos?0:1);
		assert(index<sizeof(XJmpTable));
		const JumpEntry& jmp = XJmpTable[index];
		(*rotCoords)+=jmp.rot;
		return jmp.face;
	}

	ECubeFace JumpY(const ECubeFace srcFace, const bool isdYPos, int* rotCoords) const
	{
		int index = (int)srcFace*2+(isdYPos?0:1);
		assert(index<sizeof(YJmpTable));
		const JumpEntry& jmp = YJmpTable[index];
		(*rotCoords)+=jmp.rot;
		return jmp.face;
	}

	// table that shows to which face we jump
	ECubeFace JumpTable(const ECubeFace srcFace, int isdXPos, int isdYPos, int* rotCoords) const
	{
		if(isdXPos!=0)	// recursive jump until dx==0
		{
			int newSwap = 0;
			ECubeFace newFace = JumpX(srcFace, isdXPos>0, &newSwap);
			(*rotCoords) += newSwap;
			isdXPos-=((isdXPos>0)?1:-1);
			RotateCoord(&isdXPos, &isdYPos, newSwap);
			return JumpTable(newFace, isdXPos, isdYPos, rotCoords);
		}
		if(isdYPos!=0)	// recursive jump until dy==0
		{
			int newSwap = 0;
			ECubeFace newFace = JumpY(srcFace, isdYPos>0, &newSwap);
			(*rotCoords) += newSwap;
			isdYPos -= ((isdYPos>0)?1:-1);
			RotateCoord(&isdXPos, &isdYPos, newSwap);
			return JumpTable(newFace, isdXPos, isdYPos, rotCoords);
		}
		assert(isdXPos==0&&isdYPos==0);
		return srcFace;
	}

	void RotateCoord(int* x, int* y, int mode) const
	{
		if(mode!=0)
		{
			if(mode == 2)	// 180 degrees
			{
				(*x) = -(*x);
				(*y) = -(*y);
			}
			else
			{
				if(mode==1)	// 90 dergees
				{
					int tmp = (*y);
					(*y) = (*x);
					(*x) = -tmp;
				}
				else		// 270 degrees
				{
					assert(mode==3);
					int tmp = (*y);
					(*y) = -(*x);
					(*x) = tmp;
				}
			}
		}
	}

public:
	//! works only within the Bitmap for filter kernels
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	//! /param outValue
	//! /return pointer to the raster element value if position was in the bitmap, NULL otherwise
	const RasterElement* GetForFiltering_2D( const int inX, const int inY ) const
	{
		return Get(inX, inY);
	}

	//! works only within the Bitmap for filter kernels
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	//! /param outValue
	//! /return pointer to the raster element value if position was in the bitmap, NULL otherwise
	const RasterElement* GetForFiltering_Cubemap( const int inX, const int inY, const int srcX, const int srcY ) const
	{
		if (m_data.empty())
		{
			return false;
		}

		assert(m_dwWidth == m_dwHeight*6);
		assert(srcX>=0&&srcX<m_dwWidth);
		assert(srcY>=0&&srcY<m_dwHeight);

		const int sideSize = m_dwHeight;

		ECubeFace srcFace = (ECubeFace)(srcX/sideSize);

		if(inX>=0&&inX<m_dwWidth && inY>=0&&inY<m_dwHeight)	// if we're inside the cubemap
		{
			ECubeFace destFace = (ECubeFace)(inX/sideSize);
			if(destFace==srcFace)	// we have the same face as src texel
			{
				return &m_data[inY*m_dwWidth+inX];
			}
		}
		const int halfSideSize = Util::getMax(1, sideSize/2);

		// ternary logic
		const int isdXPositive = int(floorf((float)inX/sideSize)-floorf((float)srcX/sideSize));
		const int isdYPositive = int(floorf((float)inY/sideSize)-floorf((float)srcY/sideSize));
		//if(isdXPositive==0&&isdYPositive<0&&srcFace==ecfPosY)
		//{
		//	int tmp = 0;
		//}
		assert(isdXPositive!=0 || isdYPositive!=0);
		int rotCoords = 0;	// quadrants to rotate coords
		ECubeFace destFace = JumpTable(srcFace, isdXPositive, isdYPositive,&rotCoords);
		rotCoords = ((rotCoords%4)+4)%4;
		int destX = inX-srcFace*sideSize;
		int destY = inY;

		// rotate coords
		destX -= halfSideSize;	// center coords
		destY -= halfSideSize;
		RotateCoord(&destX, &destY, rotCoords);
		destX += halfSideSize;	// shift back
		destY += halfSideSize;

		destX = ((destX+sideSize)%sideSize+sideSize)%sideSize;	// tile in the face
		destY = ((destY+sideSize)%sideSize+sideSize)%sideSize;

		destX = Util::getMin(destX, sideSize-1);
		destY = Util::getMin(destY, sideSize-1);
		destX += sideSize*destFace;

		assert(destX<m_dwWidth);
		assert((ECubeFace)(destX/sideSize)==destFace);

		return &m_data[destY*m_dwWidth+destX];
	}

	const RasterElement* GetForFiltering( const Vec3& inDir ) const
	{
		Vec3 vcAbsDir(fabsf(inDir.x), fabsf(inDir.y), fabsf(inDir.z));
		ECubeFace face;
		Vec2 texCoord;
		if(vcAbsDir.x > vcAbsDir.y && vcAbsDir.x > vcAbsDir.z)
		{
			if(inDir.x > 0)
			{
				face = ecfPosX;
			}
			else
			{
				face = ecfNegX;
			}
			texCoord = Vec2(inDir.y, inDir.z) / vcAbsDir.x;
		}
		else if(vcAbsDir.y > vcAbsDir.x && vcAbsDir.y > vcAbsDir.z)
		{
			if(inDir.y > 0)
			{
				face = ecfPosY;
			}
			else
			{
				face = ecfNegY;
			}
			texCoord = Vec2(inDir.x, inDir.z) / vcAbsDir.y;
		}
		else
		{
			assert(vcAbsDir.z >= vcAbsDir.x && vcAbsDir.z >= vcAbsDir.y);
			if(inDir.z > 0)
			{
				face = ecfPosZ;
			}
			else
			{
				face = ecfNegZ;
			}
			texCoord = Vec2(inDir.x, inDir.y) / vcAbsDir.z;
		}

		texCoord = texCoord * .5f + Vec2(.5f, .5f);
		assert(texCoord.x <= 1.f && texCoord.x >= 0);
		assert(texCoord.y <= 1.f && texCoord.y >= 0);

		Vec2i texelPos(texCoord.x*(m_dwHeight-1), texCoord.y*(m_dwHeight-1));

		texelPos.x += face * m_dwHeight;	// plus face
		return &m_data[texelPos.y*m_dwWidth+texelPos.x];
	}

	//! works only within the Bitmap
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	//! /param outValue
	//! /return pointer to raster element value if position was in the bitmap, NULL otherwise
	const RasterElement* Get( const uint32 inX, const uint32 inY ) const
	{
		if (m_data.empty())
		{
			return 0;
		}
		if (inX >= m_dwWidth || inY >= m_dwHeight)
		{
			return 0;
		}
		return &m_data[inY * m_dwWidth + inX];
	}

		
	//! bilinear, works only well within 0..1
	bool GetFiltered( const float infX, const float infY, RasterElement &outValue ) const
	{
		float fIX=floorf(infX), fIY=floorf(infY);
		float fFX=infX-fIX,     fFY=infY-fIY;
		int   iXa=(int)fIX,     iYa=(int)fIY;
		int   iXb=iXa+1,        iYb=iYa+1;

		if(iXb==m_dwWidth)
			iXb=0;

		if(iYb==m_dwHeight)
			iYb=0;

		const RasterElement* p[4];

		if((p[0]=Get(iXa,iYa)) && (p[1]=Get(iXb,iYa)) && (p[2]=Get(iXa,iYb)) && (p[3]=Get(iXb,iYb)))
		{
			outValue =
				(*p[0]) * ((1.0f-fFX)*(1.0f-fFY)) +  // left top
				(*p[1]) * ((     fFX)*(1.0f-fFY)) +  // right top
				(*p[2]) * ((1.0f-fFX)*(     fFY)) +  // left bottom
				(*p[3]) * ((     fFX)*(     fFY));   // right bottom

			return true;
		}

		return false;
	}

	//! works only within the Bitmap
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	const RasterElement& GetRef(const uint32 inX, const uint32 inY) const
	{
		assert(!m_data.empty());
		assert(inX < m_dwWidth && inY < m_dwHeight);
		return m_data[inY * m_dwWidth + inX];
	}

	//! works only within the Bitmap
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	RasterElement& GetRef(const uint32 inX, const uint32 inY)
	{
		assert(!m_data.empty());
		assert(inX < m_dwWidth && inY < m_dwHeight);
		return m_data[inY * m_dwWidth + inX];
	}

	//! works even outside of the Bitmap (tiled)
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	RasterElement& GetTiledRef(const uint32 inX, const uint32 inY)
	{ 
		assert(!m_data.empty());
		const uint32 x = inX % m_dwWidth;
		const uint32 y = inY % m_dwHeight;
		return m_data[y * m_dwWidth + x];
	}

	//! works only within the Bitmap
	//! /param inX 0..m_dwWidth-1 or the method returns false
	//! /param inY 0..m_dwHeight-1 or the method returns false
	//! /param inValue
	bool Set(const uint32 inX, const uint32 inY, const RasterElement& inValue)
	{
		if (m_data.empty())
		{
			assert(!m_data.empty());
			return false;
		}

		if (inX >= m_dwWidth || inY >= m_dwHeight)
		{
			return false;
		}

		m_data[inY*m_dwWidth+inX]=inValue;

		return true;
	}

	uint32 GetWidth() const
	{
		return m_dwWidth;
	}

	uint32 GetHeight() const
	{
		return m_dwHeight;
	}

	// Returns size of one line in bytes
	size_t GetPitch() const
	{
		return m_dwWidth * sizeof(RasterElement);
	}

	uint32 GetBitmapSizeInBytes() const
	{
		return m_dwWidth * m_dwHeight * sizeof(RasterElement);
	}

	//! /return could be 0 if the pixel is outside the bitmap
	const RasterElement* GetPointer(const uint32 inX=0, const uint32 inY=0) const
	{
		if (inX >= m_dwWidth || inY >= m_dwHeight)
		{
			return 0;
		}
		return &m_data[inY*m_dwWidth+inX];
	}

	//! /return could be 0 if the pixel is outside the bitmap
	RasterElement* GetPointer(const uint32 inX=0, const uint32 inY=0)
	{
		if (inX >= m_dwWidth || inY >= m_dwHeight)
		{
			return 0;
		}
		return &m_data[inY*m_dwWidth+inX];
	}

	void Fill(const RasterElement& inValue)
	{
		const uint32 n = m_dwHeight * m_dwWidth;
		for (uint32 i = 0; i < n; ++i)
		{
			m_data[i] = inValue;
		}
	}

	bool IsValid() const
	{
		return !m_data.empty();
	}

protected: // ------------------------------------------------------

	std::vector<RasterElement> m_data; //!< [m_dwWidth * m_dwHeight]

	uint32 m_dwWidth;
	uint32 m_dwHeight;
};




