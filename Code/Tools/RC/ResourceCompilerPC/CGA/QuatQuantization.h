// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef _Quat_QUANTIZATION_
#define _Quat_QUANTIZATION_



//namespace QuaternionCompression
//{

#define POS_BIT		0x01
#define ROT_BIT		0x02
#define SCL_BIT		0x04

//! using 3 floats without compression
#define ROT_FLOAT	0x08
//! using 3 short int without compression
#define ROT_SHORT	0x10



#define MAX_SHORTINT	32767
#define MAX_SHORTINTf	32767.0f

#define MAX_POLAR16b	10430
#define MAX_POLAR16bf	10430.0f


#define MAX_10BIT		723
#define MAX_10BITf		723.0f
#define RANGE_10BIT		0.707106781186f

#define MAX_15BIT		23170
#define MAX_15BITf		23170.0f
#define RANGE_15BIT		0.707106781186f

#define MAX_20BIT		741454
#define MAX_20BITf		741454.0f
#define RANGE_20BIT		0.707106781186f

#define MAX_21BIT		1482909
#define MAX_21BITf		1482909.0f
#define RANGE_21BIT		0.707106781186f


//#define MAX_10BIT		511
//#define MAX_10BITf		511.0f
//#define RANGE_10BIT		1.0f



enum ECompressionFormat
{
	eNoCompress = 0,
	eNoCompressQuat = 1,
	eNoCompressVec3 = 2,
	eShotInt3Quat = 3,
	eSmallTreeDWORDQuat = 4,
	eSmallTree48BitQuat = 5,
	eSmallTree64BitQuat = 6,
	ePolarQuat = 7,
	eSmallTree64BitExtQuat = 8,
	eAutomaticQuat = 9
};


// much convenient class for quaternion storing
struct Float4Storage
{
	union{
		struct 
		{
			float x, y, z, w;
		};

		float comp[4];
	};

	Float4Storage() : x(0), y(0), z(0), w(0)
	{

	};

	Float4Storage(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w)
	{

	};

	explicit Float4Storage(const Quat& q) : x(q.v.x), y(q.v.y), z(q.v.z), w(q.w)
	{

	};

	Float4Storage operator-()
	{
		return Float4Storage(-x, -y, -z, -w);
	}

	float& operator[](unsigned int i)
	{
		return comp[i];
	};

	const float operator [] (unsigned int i) const 
	{
		return comp[i];
	};

	operator Quat() const
	{
		Quat res;
		res.v.x = x;
		res.v.y = y;
		res.v.z = z;
		res.w = w;

		return res;
	};

	void Normalize()
	{
		float dist = sqrtf(x*x + y*y + z*z + w*w);
		x = x / dist;
		y = y / dist;
		z = z / dist;
		w = w / dist;

	};

};




struct NoCompressQuat 
{
	Float4Storage val;

	void SwapBytes()
	{
		SwapEndianness(val.x);
		SwapEndianness(val.y);
		SwapEndianness(val.z);
		SwapEndianness(val.w);
	}

	void ToInternalType(const Quat& q)
	{
		val = Float4Storage(q);
	}

	Quat ToExternalType() const
	{
		return val;
	}

	static uint32 GetFormat()
	{
		return eNoCompressQuat;
	}
};

//struct ShotInt3Quat
//{
//	short int m_x;
//	short int m_y;
//	short int m_z;
//	short int m_w;

//	void FromQuat(Quat& q)
//	{
//		Quat tmp = q;
//		tmp.Normalize();

//		if (tmp.w < 0.0f)
//		{
//			tmp.x *= -1.0f;
//			tmp.y *= -1.0f;
//			tmp.z *= -1.0f;
//			tmp.w *= -1.0f;
//		}

//		m_x = static_cast<short int>(floor(tmp.x * MAX_SHORTINT + 0.5f));
//		m_y = static_cast<short int>(floor(tmp.y * MAX_SHORTINT + 0.5f));
//		m_z = static_cast<short int>(floor(tmp.z * MAX_SHORTINT + 0.5f));
//		m_w = static_cast<short int>(floor(tmp.w * MAX_SHORTINT + 0.5f));

//		


//	}

//	Quat ToQuat()
//	{
//		Quat res;

//		res.x = static_cast<float>((float)m_x /MAX_SHORTINTf);
//		res.y = static_cast<float>((float)m_y /MAX_SHORTINTf);
//		res.z = static_cast<float>((float)m_z /MAX_SHORTINTf);
//		//res.w = static_cast<float>((float)m_w /MAX_SHORTINTf);

//		float tmp = 1.0f - (res.x*res.x + res.y*res.y + res.z*res.z);
//		float test = sqrt(tmp);
//		res.w = sqrtf(1.0f - (res.x*res.x + res.y*res.y + res.z*res.z));

//		res.Normalize();

//		return res;
//	}
//};


struct NoCompressVec3
{

	float x,y,z;

	NoCompressVec3()
		: x(0)
		, y(0)
		, z(0)
	{
	}

	void ToInternalType(const Vec3& v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}

	Vec3 ToExternalType() const
	{
		Vec3 res(x,y,z);
		return res;
	}

	static uint32 GetFormat()
	{
		return eNoCompressVec3;
	}

	void SwapBytes()
	{
		SwapEndianness(x);
		SwapEndianness(y);
		SwapEndianness(z);
	}

};


//struct ShotInt4Quat : public BaseCompressedQuat
//{
//	short int m_x;
//	short int m_y;
//	short int m_z;
//	short int m_w;
//
//	void FromQuat(Quat& q)
//	{
//		Quat tmp = q;
//		tmp.Normalize();
//
//		if (tmp.w < 0.0f)
//		{
//			tmp = -tmp;
//		}
//
//		m_x = static_cast<short int>(floor(tmp.v.x * MAX_SHORTINT + 0.5f));
//		m_y = static_cast<short int>(floor(tmp.v.y * MAX_SHORTINT + 0.5f));
//		m_z = static_cast<short int>(floor(tmp.v.z * MAX_SHORTINT + 0.5f));
//		m_w = static_cast<short int>(floor(tmp.w * MAX_SHORTINT + 0.5f));
//	}
//
//	Quat ToQuat()
//	{
//		Quat res;
//
//		res.v.x = static_cast<float>((float)m_x /MAX_SHORTINTf);
//		res.v.y = static_cast<float>((float)m_y /MAX_SHORTINTf);
//		res.v.z = static_cast<float>((float)m_z /MAX_SHORTINTf);
//		res.w = static_cast<float>((float)m_w /MAX_SHORTINTf);
//
//		res.Normalize();
//		return res;
//	}
//
//};
//


struct ShotInt3Quat
{
	short int m_x;
	short int m_y;
	short int m_z;

	ShotInt3Quat()
		: m_x(0)
		, m_y(0)
		, m_z(0)
	{
	}

	void SwapBytes()
	{
		SwapEndianness(m_x);
		SwapEndianness(m_y);
		SwapEndianness(m_z);
	}

	void ToInternalType(const Quat& q)
	{
		Quat tmp = q;
		tmp.Normalize();

		if (tmp.w < 0.0f)
		{
			tmp.v.x *= -1.0f;
			tmp.v.y *= -1.0f;
			tmp.v.z *= -1.0f;
			tmp.w *= -1.0f;
		}

		m_x = static_cast<short int>(floor(tmp.v.x * MAX_SHORTINT + 0.5f));
		m_y = static_cast<short int>(floor(tmp.v.y * MAX_SHORTINT + 0.5f));
		m_z = static_cast<short int>(floor(tmp.v.z * MAX_SHORTINT + 0.5f));
	}

	Quat ToExternalType() const
	{
		Quat res;

		res.v.x = static_cast<float>((float)m_x /MAX_SHORTINTf);
		res.v.y = static_cast<float>((float)m_y /MAX_SHORTINTf);
		res.v.z = static_cast<float>((float)m_z /MAX_SHORTINTf);

		res.w = sqrtf(1.0f - (res.v.x*res.v.x + res.v.y*res.v.y + res.v.z*res.v.z));

		//			res.Normalize();

		return res;
	}

	static uint32 GetFormat()
	{
		return eShotInt3Quat;
	}


};


struct SmallTreeDWORDQuat
{

	unsigned int m_Val;

	SmallTreeDWORDQuat()
		: m_Val(0)
	{
	}

	void SwapBytes()
	{
		SwapEndianness(m_Val);
	}

	void ToInternalType(const Quat& q)
	{

		Float4Storage tmp(q);
		tmp.Normalize();

		m_Val = 0;
		int index = 0;
		float max = fabs(tmp.comp[0]);
		for (int i = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i+1]))
			{
				index = i+1;
				max = fabs(tmp.comp[i+1]);
			}
		}

		// don't store the sign of biggest value
		if (tmp.comp[index] < 0.0f)
		{
			tmp = -tmp;
		}

		int c = 0;

		// don't store maximum value - delete it;
		float drange = RANGE_10BIT;
		for (int i = 0; i < 4; ++i)
		{
			if (i != index)
			{
				//pack
				short int packed = static_cast<short int>(floor((tmp.comp[i] + drange) * MAX_10BITf /* / 2.0f / drange*/ + 0.5f));// & 0x3FF;

				//assert(packed <= 0x3FF);

				unsigned int packdw(packed);
				m_Val |= packdw << c;
				c += 10;
			}
		}

		unsigned int inddw(index);

		m_Val |= inddw << c;

	}

	Quat ToExternalType() const
	{
		Float4Storage res;

		int index = m_Val >> 30;

		int c = 0;
		float sqrsumm = 0.0f;

		float drange = RANGE_10BIT;

		for (int i = 0; i < 4; ++i)
		{
			if (i != index)
			{
				//pack


				short int packed = static_cast<short int>((m_Val >> c) & 0x3FF);
				res.comp[i] = static_cast<float>((float)packed /* 2.0f * drange */ / MAX_10BITf ) - drange;

				sqrsumm += res.comp[i]* res.comp[i];

				c += 10;
			}
		}

		res.comp[index] = sqrtf(1.0f - sqrsumm);

		//			res.Normalize();

		return res;
	}

	static uint32 GetFormat()
	{
		return eSmallTreeDWORDQuat;
	}



};


struct SmallTree48BitQuat
{
	//short int m_x;
	//short int m_y;
	//short int m_z;
	typedef int64 LONGDWORD;




public:
	unsigned short int m_1;
	unsigned short int m_2;
	unsigned short int m_3;

	SmallTree48BitQuat()
		: m_1(0)
		, m_2(0)
		, m_3(0)
	{
	}

	void SwapBytes()
	{
		SwapEndianness(m_1);
		SwapEndianness(m_2);
		SwapEndianness(m_3);
	}

	void ToInternalType(const Quat& q)
	{

		LONGDWORD m_Val;
		Float4Storage tmp(q);
		tmp.Normalize();

		m_Val = 0;
		int index = 0;
		float max = fabs(tmp.comp[0]);
		for (int i = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i+1]))
			{
				index = i+1;
				max = fabs(tmp.comp[i+1]);
			}
		}

		// don't store the sign of biggest value
		if (tmp.comp[index] < 0.0f)
		{
			tmp = -tmp;
		}

		int c = 0;

		// don't store maximum value - delete it;
		float drange = RANGE_15BIT;
		for (int i = 0; i < 4; ++i)
		{
			if (i != index)
			{
				//pack
				short int packed = static_cast<short int>(floor((tmp.comp[i] + drange) * MAX_15BITf /* / 2.0f / drange*/ + 0.5f));// & 0x3FF;

				//assert(packed <= 0x7FFF);

				LONGDWORD packdw(packed);
				m_Val |= packdw << c;
				c += 15;
			}
		}

		LONGDWORD inddw(index);

		m_Val |= inddw << 46;

		//save m_xxx

		m_1 = static_cast<unsigned short int>(m_Val & 0xFFFF);
		m_2 = static_cast<unsigned short int>(( m_Val >> 16) & 0xFFFF);
		m_3 =static_cast<unsigned short int>(( m_Val >> 32) & 0xFFFF);


	}

	Quat ToExternalType() const
	{
		Float4Storage res;
		LONGDWORD m_Val;


		//create m_Val
		LONGDWORD m2(m_2), m3(m_3);
		m_Val = m_1 + (m2 << 16) + (m3 << 32);


		int index = static_cast<int>(m_Val >> 46);

		int c = 0;
		float sqrsumm = 0.0f;

		float drange = RANGE_15BIT;

		int mask = 0x7FFF;
		for (int i = 0; i < 4; ++i)
		{
			if (i != index)
			{
				//pack


				unsigned short int packed = static_cast<unsigned short int>((m_Val >> c) & mask);
				res.comp[i] = static_cast<float>((float)packed /* 2.0f * drange */ / MAX_15BITf ) - drange;

				sqrsumm += res.comp[i]* res.comp[i];

				c += 15;
			}
		}

		res.comp[index] = sqrtf(1.0f - sqrsumm);

		//			res.Normalize();

		return res;
	}

	static uint32 GetFormat()
	{
		return eSmallTree48BitQuat;
	}


};


struct SmallTree64BitQuat
{
	//short int m_x;
	//short int m_y;
	//short int m_z;
	typedef int64 LONGDWORD;




public:
	unsigned short int m_1;
	unsigned short int m_2;
	unsigned short int m_3;
	unsigned short int m_4;

	SmallTree64BitQuat()
		: m_1(0)
		, m_2(0)
		, m_3(0)
		, m_4(0)
	{
	}


	void SwapBytes()
	{
		SwapEndianness(m_1);
		SwapEndianness(m_2);
		SwapEndianness(m_3);
		SwapEndianness(m_4);
	}

	void ToInternalType(const Quat& q)
	{

		LONGDWORD m_Val;

		Float4Storage tmp(q);
		tmp.Normalize();

		m_Val = 0;
		int offset[3];

		int index = 0;
		float max = fabs(tmp.comp[0]);

		for (int i = 0, o = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i+1]))
			{
				offset[o++] = index;
				index = i+1;
				max = fabs(tmp.comp[i+1]);
			}
			else
			{
				offset[o++] = i;
			}

		}

		// don't store the sign of biggest value
		if (tmp.comp[index] < 0.0f)
		{
			tmp = -tmp;
		}

		int c = 0;


		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i!= index)
			{
					offset[c++] = i;
			}
		}


		// don't store maximum value - delete it;




		LONGDWORD packdw = (LONGDWORD)(floor((tmp.comp[offset[0]] + RANGE_20BIT) * MAX_20BITf  + 0.5f));


		m_Val |= packdw << c;
		c += 20;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[1]] + RANGE_20BIT) * MAX_20BITf  + 0.5f));


		m_Val |= packdw << c;
		c += 20;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[2]] + RANGE_20BIT) * MAX_20BITf  + 0.5f));


		m_Val |= packdw << c;




		//float drange = RANGE_20BIT;
		//for (int i = 0, ; i < 4; ++i)
		//{
		//	if (i != index)
		//	{
		//		//pack
		//		int packed = static_cast<int>(floor((tmp.comp[i] + drange) * MAX_20BITf  + 0.5f));
		//		//					assert(packed < 0x3FF);


		//		if (packed > 0xFFFFF)
		//		{
		//			int a = 0;
		//		}



		//		LONGDWORD packdw(packed);
		//		m_Val |= packdw << c;
		//		c += 20;
		//	}
		//}

		LONGDWORD inddw(index);

		m_Val |= inddw << 62;

		//save m_xxx

		m_1 = static_cast<unsigned short int>(m_Val & 0xFFFF);
		m_2 = static_cast<unsigned short int>(( m_Val >> 16) & 0xFFFF);
		m_3 = static_cast<unsigned short int>(( m_Val >> 32) & 0xFFFF);
		m_4 = static_cast<unsigned short int>(( m_Val >> 48) & 0xFFFF);


	}

	Quat ToExternalType() const
	{
		Float4Storage res;


		LONGDWORD m_Val;
		//create m_Val
		LONGDWORD m2(m_2), m3(m_3), m4(m_4);
		m_Val = m_1 + (m2 << 16) + (m3 << 32) + (m4 << 48);


		unsigned short int index = static_cast<unsigned short int>((m_Val >> 62) & 3);

		int offset[3];

		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i!= index)
			{
				offset[c++] = i;
			}
		}

		//if (index < 0 || index > 3)
		//{
		//	int f = 0;
		//}

		int c = 0;
		float sqrsumm = 0.0f;

		int mask = 0xFFFFF;

		int i = offset[0];
		int packed = static_cast<int>((m_Val >> c) & mask);
		res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		sqrsumm += res.comp[i]* res.comp[i];
		c += 20;

		i = offset[1];
		packed = static_cast<int>((m_Val >> c) & mask);
		res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		sqrsumm += res.comp[i]* res.comp[i];
		c += 20;

		mask = 0xFFFFF;
		i = offset[2];
		packed = static_cast<int>((m_Val >> c) & mask);
		res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		sqrsumm += res.comp[i]* res.comp[i];



		//int mask = 0xFFFFF;
		//for (int i = 0; i < 4; ++i)
		//{
		//	if (i != index)
		//	{
		//		//pack


		//		int packed = static_cast<int>((m_Val >> c) & mask);
		//		res.comp[i] = static_cast<float>((float)packed /* 2.0f * drange */ / MAX_20BITf ) - drange;

		//		sqrsumm += res.comp[i]* res.comp[i];

		//		c += 20;
		//	}
		//}

		res.comp[index] = sqrtf(1.0f - sqrsumm);

		return res;
	}

	static uint32 GetFormat()
	{
		return eSmallTree64BitQuat;
	}


};

#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
template<typename T> inline
T AlignToPlatform(T nSize, size_t nAlign = CRY_PLATFORM_ALIGNMENT)
{
	assert((nAlign & (nAlign - 1)) == 0);
	size_t size = ((size_t)nSize + (nAlign - 1)) & ~(nAlign - 1);
	return T(size);
}
#endif

struct SmallTree64BitExtQuat
{
	//short int m_x;
	//short int m_y;
	//short int m_z;
	typedef int64 LONGDWORD;




public:
//	LONGDWORD m_Val;
	unsigned int m_1; // m_2...m_1
	unsigned int m_2; // m_4...m_3

	SmallTree64BitExtQuat()
		: m_1(0)
		, m_2(0)
	{
	}

	void SwapBytes()
	{
		SwapEndianness(m_1);
		SwapEndianness(m_2);
	}


	void ToInternalType(const Quat& q)
	{
		

		Float4Storage tmp(q);
		tmp.Normalize();

		LONGDWORD m_Val;

		m_Val = 0;
		int offset[3];

		int index = 0;
		float max = fabs(tmp.comp[0]);

		for (int i = 0, o = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i+1]))
			{
				offset[o++] = index;
				index = i+1;
				max = fabs(tmp.comp[i+1]);
			}
			else
			{
				offset[o++] = i;
			}

		}

		// don't store the sign of biggest value
		if (tmp.comp[index] < 0.0f)
		{
			tmp = -tmp;
		}

		int c = 0;


		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i!= index)
			{
				offset[c++] = i;
			}
		}


		// don't store maximum value - delete it;

		LONGDWORD packdw = (LONGDWORD)(floor((tmp.comp[offset[0]] + RANGE_21BIT) * MAX_21BITf  + 0.5f));

		m_Val |= packdw << c;
		c += 21;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[1]] + RANGE_21BIT) * MAX_21BITf  + 0.5f));


		m_Val |= packdw << c;
		c += 21;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[2]] + RANGE_20BIT) * MAX_20BITf  + 0.5f));


		m_Val |= packdw << c;




		//float drange = RANGE_20BIT;
		//for (int i = 0, ; i < 4; ++i)
		//{
		//	if (i != index)
		//	{
		//		//pack
		//		int packed = static_cast<int>(floor((tmp.comp[i] + drange) * MAX_20BITf  + 0.5f));
		//		//					assert(packed < 0x3FF);


		//		if (packed > 0xFFFFF)
		//		{
		//			int a = 0;
		//		}



		//		LONGDWORD packdw(packed);
		//		m_Val |= packdw << c;
		//		c += 20;
		//	}
		//}

		LONGDWORD inddw(index);

		m_Val |= inddw << 62;

		//save m_xxx

		//m_1 = static_cast<unsigned short int>(m_Val & 0xFFFF);
		//m_2 = static_cast<unsigned short int>(( m_Val >> 16) & 0xFFFF);
		//m_3 = static_cast<unsigned short int>(( m_Val >> 32) & 0xFFFF);
		//m_4 = static_cast<unsigned short int>(( m_Val >> 48) & 0xFFFF);

		m_1 = static_cast<unsigned int>(m_Val & 0xFFFFFFFF);
		m_2 = static_cast<unsigned int>((m_Val >> 32) & 0xFFFFFFFF);


	}

	Quat ToExternalType() const
	{
		Float4Storage res;
		//LONGDWORD m_Val;
		////create m_Val
		//LONGDWORD m2(m_2), m3(m_3), m4(m_4);
		//m_Val = m_1 + (m2 << 16) + (m3 << 32) + (m4 << 48);

		unsigned int index = static_cast<unsigned int>((m_2 >> 30) & 3);
		int offset[3];
		for (int i = 0, c = 0; i < 4; i++)
			{
			if (i!= index)
				{
				offset[c++] = i;
				}
			} 
		//		int c = 0;
		//		float sqrsumm = 0.0f;
		//
		//		int mask = 0x1FFFFF;
		//
		//		int i = offset[0];
		//		int packed = static_cast<int>((m_Val >> c) & mask);
		//		res.comp[i] = static_cast<float>((float)packed  / MAX_21BITf ) - RANGE_21BIT;
		//		sqrsumm += res.comp[i]* res.comp[i];
		//		c += 21;
		//
		//		i = offset[1];
		//		packed = static_cast<int>((m_Val >> c) & mask);
		//		res.comp[i] = static_cast<float>((float)packed  / MAX_21BITf ) - RANGE_21BIT;
		//		sqrsumm += res.comp[i]* res.comp[i];
		//		c += 21;
		//
		//		mask = 0xFFFFF;
		//		i = offset[2];
		//		packed = static_cast<int>((m_Val >> c) & mask);
		//		res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		//		sqrsumm += res.comp[i]* res.comp[i];
		//
		//
		//		res.comp[index] = sqrtf(1.0f - sqrsumm);
		//#endif

#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
		CryStackAllocWithSize(Float4Storage, m_Res, AlignToPlatform);
		CryStackAllocWithSize(Float4Storage, max, AlignToPlatform);
		CryStackAllocWithSize(Float4Storage, range, AlignToPlatform);

		m_Res->comp[offset[0]] = (float)((int)(m_1 & 0x1FFFFF));
		m_Res->comp[offset[1]] = (float)((int)(((m_1 >> 21) + (m_2 << 11)) & 0x1FFFFF));
		m_Res->comp[offset[2]] = (float)((int)((m_2 >> 10) & 0xFFFFF));
		m_Res->comp[index] = 0.0f;  // Arbitrary value to avoid floating point under/overflow.

		//		static const float max = MAX_20BITf;//, MAX_21BITf, MAX_21BITf, MAX_21BITf};
		//		static const float range = RANGE_20BIT;//, RANGE_21BIT, RANGE_21BIT, RANGE_21BIT};
		max->comp[offset[0]] = MAX_21BITf;
		max->comp[offset[1]] = MAX_21BITf;
		max->comp[offset[2]] = MAX_20BITf;
		max->comp[index] = 1.0f;

		range->comp[offset[0]] = RANGE_21BIT;
		range->comp[offset[1]] = RANGE_21BIT;
		range->comp[offset[2]] = RANGE_20BIT;
		range->comp[index] = 1.0f;

		__m128 src = _mm_load_ps(&m_Res->comp[0]);
		__m128 div = _mm_load_ps(&max->comp[0]);
		__m128 ran = _mm_load_ps(&range->comp[0]);
		__m128 a = _mm_div_ps(src, div);
		__m128 b = _mm_sub_ps(a, ran);
		__m128 sq = _mm_mul_ps(b,b);

		// values
		_mm_store_ps(&m_Res->comp[0], b);
		float squares[4];
		_mm_store_ps(&squares[0], sq);

		m_Res->comp[index] = sqrtf(1 - squares[offset[0]] - squares[offset[1]] - squares[offset[2]]);

		return *m_Res;
#else
		Float4Storage m_Res;

		float sqrsumm = 0.0f;

		int i = offset[0];
		int packed = (/*(int)*/(m_1 & 0x1FFFFF));
		m_Res.comp[i] = ((float)packed  / MAX_21BITf ) - RANGE_21BIT;
		sqrsumm += m_Res.comp[i]* m_Res.comp[i];

		i = offset[1];
		packed = (/*(int)*/(((m_1 >> 21) + (m_2 << 11)) & 0x1FFFFF));
		m_Res.comp[i] = ((float)packed  / MAX_21BITf ) - RANGE_21BIT;
		sqrsumm += m_Res.comp[i]* m_Res.comp[i];

		i = offset[2];
		packed = (/*(int)*/((m_2 >> 10) & 0xFFFFF));
		m_Res.comp[i] = ((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		sqrsumm += m_Res.comp[i]* m_Res.comp[i];

		m_Res.comp[index] = sqrtf(1.0f - sqrsumm);

		return m_Res;
#endif
	}

	static uint32 GetFormat()
	{
		return eSmallTree64BitExtQuat;
	}


};



struct PolarCoordinates
{

	float m_Yaw;
	float m_Pitch;
	//float m_Ro;

	PolarCoordinates()
		: m_Yaw(0)
		, m_Pitch(0)
	{
	}

	void FromVec3(float x, float z, float y)
	{

		float S = sqrtf(x*x + y*y + z*z);

		if (S > 0.0f)
		{

			double dTheta = acos(z/S);


			float v2norm = sqrtf(x*x + y*y);

			//x /= v2norm;

			double dRho;

			if (v2norm  > 0.0f)
			{

				if ((x >= 0.0f))
				{
					dRho = asin(y / v2norm);
				}
				else 
				{
					dRho = gf_PI - asin(y / v2norm);// + deg2rad(270.0f);
				}
			}
			else
				dRho = 0.0f;

			m_Pitch = (float) dTheta;
			m_Yaw   = (float) dRho;
		}
		else
		{
			//				m_Ro = 0.0f;
			m_Pitch=0.0f;
			m_Yaw=0.0f;
		}

	}

	void ToVec3( float& x, float& z, float& y)
	{
		//			m_Ro = 1.0f;
		double sin_theta = sin(this->m_Pitch);
		double cos_theta = cos(this->m_Pitch);
		double sin_rho   = sin(this->m_Yaw);
		double cos_rho   = cos(this->m_Yaw);
		y = (float) (sin_theta * sin_rho);
		x = (float) (sin_theta * cos_rho);
		z = (float) cos_theta;
	}

};


struct PolarQuat
{
	short int m_yaw;
	short int m_pitch;
	short int m_w;

	PolarQuat()
		: m_yaw(0)
		, m_pitch(0)
		, m_w(0)
	{
	}

	void ToInternalType(const Quat& q)
	{
		Float4Storage tmp(q);
		tmp.Normalize();

		if (tmp.w < 0.0f)
		{
			tmp = -tmp;
		}

		PolarCoordinates polar;
		polar.FromVec3(tmp.x, tmp.y, tmp.z);

		m_yaw = static_cast<short int>(floor(polar.m_Yaw * MAX_POLAR16b + 0.5f));
		m_pitch = static_cast<short int>(floor(polar.m_Pitch * MAX_POLAR16b + 0.5f));
		m_w = static_cast<short int>(floor(tmp.w * MAX_SHORTINT + 0.5f));
	}

	Quat ToExternalType() const
	{
		Float4Storage res;

		PolarCoordinates polar;

		res.w = static_cast<float>((float)m_w /MAX_SHORTINTf);

		polar.m_Pitch = static_cast<float>((float)m_pitch /MAX_POLAR16bf);
		polar.m_Yaw = static_cast<float>((float)m_yaw /MAX_POLAR16bf);
		//polar.m_Ro = static_cast<float>((float)m_ro /MAX_SHORTINT);

		polar.ToVec3(res.x, res.y, res.z);

		float renorm =   sqrt(1 - res.w*res.w);
		res.x *= renorm;
		res.y *= renorm;
		res.z *= renorm;

		res.Normalize();
		return res;
	}

	static uint32 GetFormat()
	{
		return ePolarQuat;
	}


};



#endif
