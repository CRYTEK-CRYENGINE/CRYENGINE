// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define POS_BIT 0x01
#define ROT_BIT 0x02
#define SCL_BIT 0x04

//! using 3 floats without compression
#define ROT_FLOAT     0x08
//! using 3 short int without compression
#define ROT_SHORT     0x10

#define MAX_SHORTINT  32767
#define MAX_SHORTINTf 32767.0f

#define MAX_POLAR16b  10430
#define MAX_POLAR16bf 10430.0f

#define MAX_10BIT     723
#define MAX_10BITf    723.0f
#define RANGE_10BIT   0.707106781186f

#define MAX_15BIT     23170
#define MAX_15BITf    23170.0f
#define RANGE_15BIT   0.707106781186f

#define MAX_20BIT     741454
#define MAX_20BITf    741454.0f
#define RANGE_20BIT   0.707106781186f

#define MAX_21BIT     1482909
#define MAX_21BITf    1482909.0f
#define RANGE_21BIT   0.707106781186f

//#define MAX_10BIT		511
//#define MAX_10BITf		511.0f
//#define RANGE_10BIT		1.0f

// don't add values which incrace the maximum enum value over 255 because this will break IControllerOptNonVirtual!!!!
enum ECompressionInformation
{
	eNoCompress            = 0,
	eNoCompressQuat        = 1,
	eNoCompressVec3        = 2,
	eShotInt3Quat          = 3,
	eSmallTreeDWORDQuat    = 4,
	eSmallTree48BitQuat    = 5,
	eSmallTree64BitQuat    = 6,
	ePolarQuat             = 7,
	eSmallTree64BitExtQuat = 8,
	eAutomaticQuat         = 9,
	eNoController          = 10
};

// much convenient class for quaternion storing
struct Float4Storage
{
	union
	{
		struct
		{
			float x, y, z, w;
		};

		float comp[4];
	};

	enum Spec
	{
		eZero
	};

	Float4Storage(Spec)
	{

	};

	Float4Storage() : x(0), y(0), z(0), w(0)
	{

	};

	Float4Storage(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w)
	{

	};

	Float4Storage(const Quat& q) : x(q.v.x), y(q.v.y), z(q.v.z), w(q.w)
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

	const float operator[](unsigned int i) const
	{
		return comp[i];
	};

	//operator Quat()
	//{
	//	//Quat res;
	//	//res.v.x = x;
	//	//res.v.y = y;
	//	//res.v.z = z;
	//	//res.w = w;

	//	return Quat(w,x,y,z);
	//};

	void Normalize()
	{
		float dist = sqrtf(x * x + y * y + z * z + w * w);
		x = x / dist;
		y = y / dist;
		z = z / dist;
		w = w / dist;

	};

};

struct NoCompressQuat
{
	Float4Storage val;

	inline void   ToInternalType(const Quat& q)
	{
		val = q;
	}

	inline void ToExternalType(Quat& q) const
	{
		//return val;
		q.v.x = val.x;
		q.v.y = val.y;
		q.v.z = val.z;
		q.w = val.w;
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

	float       x, y, z;

	inline void ToInternalType(const Vec3& v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}

	//float loadUnaligned(float* __unaligned ptr)
	//{
	//	return *ptr;
	//}

	inline void ToExternalType(Vec3& res) const
	{
		res.x = x;//loadUnaligned(&x);
		res.y = y;//loadUnaligned(&y);
		res.z = z;//loadUnaligned(&z);
	}

	static uint32 GetFormat()
	{
		return eNoCompressVec3;
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

	void      ToInternalType(Quat& q)
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

	void ToExternalType(Quat& q) const
	{
		//		Quat res;

		q.v.x = static_cast<float>((float)m_x / MAX_SHORTINTf);
		q.v.y = static_cast<float>((float)m_y / MAX_SHORTINTf);
		q.v.z = static_cast<float>((float)m_z / MAX_SHORTINTf);

		q.w = sqrtf(1.0f - (q.v.x * q.v.x + q.v.y * q.v.y + q.v.z * q.v.z));

		//			res.Normalize();
	}

	static uint32 GetFormat()
	{
		return eShotInt3Quat;
	}

};

struct SmallTreeDWORDQuat
{

	unsigned int m_Val;

	void         ToInternalType(Quat& q)
	{

		Float4Storage tmp = q;
		tmp.Normalize();

		m_Val = 0;
		int index = 0;
		float max = fabs(tmp.comp[0]);
		for (int i = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i + 1]))
			{
				index = i + 1;
				max = fabs(tmp.comp[i + 1]);
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

				//					assert(packed < 0x3FF);
				/*				if (packed > 0x3FF)
				   {
				   int a = 0;
				   }
				 */
				unsigned int packdw(packed);
				m_Val |= packdw << c;
				c += 10;
			}
		}

		unsigned int inddw(index);

		m_Val |= inddw << c;

	}

	void ToExternalType(Quat& q) const
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
				res.comp[i] = static_cast<float>((float)packed /* 2.0f * drange */ / MAX_10BITf) - drange;

				sqrsumm += res.comp[i] * res.comp[i];

				c += 10;
			}
		}

		res.comp[index] = sqrtf(1.0f - sqrsumm);

		q.v.x = res.x;
		q.v.y = res.y;
		q.v.z = res.z;
		q.w = res.w;

		//		return res;
	}

	static uint32 GetFormat()
	{
		return eSmallTreeDWORDQuat;
	}

};

struct SmallTree48BitQuat
{
	typedef int64 LONGDWORD;

	unsigned short int m_1;
	unsigned short int m_2;
	unsigned short int m_3;

#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
	static __m128 div;// = _mm_load1_ps(&max);
	static __m128 ran;// = _mm_load1_ps(&range);
#endif

	static int Init()
	{
#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
		const float max = MAX_15BITf;//, MAX_21BITf, MAX_21BITf, MAX_21BITf};
		const float range = RANGE_15BIT;//, RANGE_21BIT, RANGE_21BIT, RANGE_21BIT};

		float invmax = 1.0f / max;
		div = _mm_load1_ps(&invmax);
		ran = _mm_load1_ps(&range);
#endif
		return 1;
	}

	void ToInternalType(const Quat& q)
	{

		LONGDWORD m_Val;
		Float4Storage tmp = q;
		tmp.Normalize();

		m_Val = 0;
		int index = 0;
		float max = fabs(tmp.comp[0]);
		for (int i = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i + 1]))
			{
				index = i + 1;
				max = fabs(tmp.comp[i + 1]);
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

				LONGDWORD packdw(packed);
				m_Val |= packdw << c;
				c += 15;
			}
		}

		LONGDWORD inddw(index);

		m_Val |= inddw << 46;

		//save m_xxx
		m_1 = static_cast<unsigned short int>(m_Val & 0xFFFF);
		m_2 = static_cast<unsigned short int>((m_Val >> 16) & 0xFFFF);
		m_3 = static_cast<unsigned short int>((m_Val >> 32) & 0xFFFF);

	}

	void inline ToExternalType(Quat& q) const
	{
#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)

		float* m_Res = &q.v.x;
	#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		#define TEMPLATE_PROCESSING(index, offset0, offset1, offset2)              \
		  m_Res[offset0] = (float)((uint16)(m_1 & 0x7FFF));                        \
		  m_Res[offset1] = (float)((uint16)(((m_1 >> 15) + (m_2 << 1)) & 0x7FFF)); \
		  m_Res[offset2] = (float)((uint16)(((m_2 >> 14) + (m_3 << 2)) & 0x7FFF)); \
		  __m128 src = _mm_load_ps(m_Res);                                         \
		  __m128 a = _mm_mul_ps(src, div);                                         \
		  __m128 b = _mm_sub_ps(a, ran);                                           \
		  _mm_store_ps(m_Res, b);                                                  \
		  union {__m128 q; float fl[4]; } sq;                                      \
		  sq.q = _mm_mul_ps(b, b);                                                 \
		  m_Res[index] = sqrt(1 - sq.fl[offset0] - sq.fl[offset1] - sq.fl[offset2]);
	#else
		#define TEMPLATE_PROCESSING(index, offset0, offset1, offset2)              \
		  m_Res[offset0] = (float)((uint16)(m_1 & 0x7FFF));                        \
		  m_Res[offset1] = (float)((uint16)(((m_1 >> 15) + (m_2 << 1)) & 0x7FFF)); \
		  m_Res[offset2] = (float)((uint16)(((m_2 >> 14) + (m_3 << 2)) & 0x7FFF)); \
		  __m128 src = _mm_load_ps(m_Res);                                         \
		  __m128 a = _mm_mul_ps(src, div);                                         \
		  __m128 b = _mm_sub_ps(a, ran);                                           \
		  _mm_store_ps(m_Res, b);                                                  \
		  __m128 sq = _mm_mul_ps(b, b);                                            \
		  m_Res[index] = sqrt(1 - sq.m128_f32[offset0] - sq.m128_f32[offset1] - sq.m128_f32[offset2]);
	#endif
		//#define TEMPLATE_PROCESSING(index, offset0, offset1, offset2) \
		//    //			float packed = (float)((uint16)(m_1 & 0x7FFF)); \
		//    //			m_Res[offset0] = (packed  / MAX_15BITf ) - RANGE_15BIT; \
		//    //			packed = (float)((uint16)(((m_1 >> 15) + (m_2 << 1)) & 0x7FFF)); \
		//    //			m_Res[offset1] = (packed  / MAX_15BITf ) - RANGE_15BIT; \
		//    //		  packed = (float)((uint16)(((m_2 >> 14) + (m_3 << 2)) & 0x7FFF)); \
		//    //			m_Res[offset2] = (packed  / MAX_15BITf ) - RANGE_15BIT; \
		//    //			m_Res[index] = sqrtf(1.0f - m_Res[offset0]*m_Res[offset0] - m_Res[offset1]*m_Res[offset1] - m_Res[offset2]*m_Res[offset2]);

		//float squares[4];
		//_mm_store_ps(&squares[0], sq);

		//		int offset[3];

		if (m_3 > 0xBFFF) // old 3
		{
			m_Res[3] = 0.0f;
			TEMPLATE_PROCESSING(3, 0, 1, 2);

			//offset[0] = 1;
			//offset[1] = 2;
			//offset[2] = 3;
		}
		else if (m_3 > 0x7999) // old 2
		{
			m_Res[2] = 0.0f;
			TEMPLATE_PROCESSING(2, 0, 1, 3);

			//offset[0] = 0;
			//offset[1] = 2;
			//offset[2] = 3;
		}
		else if (m_3 > 0x3999) // old 1
		{
			m_Res[1] = 0.0f;
			TEMPLATE_PROCESSING(1, 0, 2, 3);

			//offset[0] = 0;
			//offset[1] = 1;
			//offset[2] = 3;
		}
		else     // old 0
		{
			m_Res[0] = 0.0f;
			TEMPLATE_PROCESSING(0, 1, 2, 3);

			//offset[0] = 0;
			//offset[1] = 1;
			//offset[2] = 2;
		}

		//m_Res[offset[0]] = (float)((uint16)(m_1 & 0x7FFF));
		//m_Res[offset[1]] = (float)((uint16)(((m_1 >> 15) + (m_2 << 1)) & 0x7FFF));
		//m_Res[offset[2]] = (float)((uint16)(((m_2 >> 14) + (m_3 << 2)) & 0x7FFF));

		//__m128 src = _mm_load_ps(m_Res);
		////__m128 div = _mm_load1_ps(&max);
		////__m128 ran = _mm_load1_ps(&range);
		//__m128 a = _mm_mul_ps(src, div);
		//__m128 b = _mm_sub_ps(a, ran);
		////
		//__m128 sq = _mm_mul_ps(b,b);

		//// values
		//_mm_store_ps(m_Res, b);
		//float squares[4];
		//_mm_store_ps(&squares[0], sq);

		//m_Res[index] = sqrtf(1 - squares[offset[0]] - squares[offset[1]] - squares[offset[2]]);

#else
		int index = m_3;
		index = index >> 14;

		int offset[3];

		if (index == 0)
		{
			offset[0] = 1;
			offset[1] = 2;
			offset[2] = 3;
		}
		else if (index == 1)
		{
			offset[0] = 0;
			offset[1] = 2;
			offset[2] = 3;
		}
		else if (index == 2)
		{
			offset[0] = 0;
			offset[1] = 1;
			offset[2] = 3;

		}
		else
		{
			offset[0] = 0;
			offset[1] = 1;
			offset[2] = 2;
		}

		CRY_ALIGN(16) Float4Storage m_Res(Float4Storage::eZero);
		//		LONGDWORD m_Val;
		//	LONGDWORD m2(m_2), m3(m_3);
		//		m_Val = m_1 + (m2 << 16) + (m3 << 32);

		int c = 0;
		float sqrsumm = 0.0f;

		//				int mask = 0x7FFF;

		int i = offset[0];
		float packed = (float)((uint16)(m_1 & 0x7FFF));
		m_Res.comp[i] = (packed / MAX_15BITf) - RANGE_15BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];
		c += 15;

		i = offset[1];
		packed = (float)((uint16)(((m_1 >> 15) + (m_2 << 1)) & 0x7FFF));
		m_Res.comp[i] = (packed / MAX_15BITf) - RANGE_15BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];
		c += 15;

		i = offset[2];
		packed = (float)((uint16)(((m_2 >> 14) + (m_3 << 2)) & 0x7FFF));
		m_Res.comp[i] = (packed / MAX_15BITf) - RANGE_15BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];

		m_Res.comp[index] = sqrtf(1.0f - sqrsumm);

		q.v.x = m_Res.x;
		q.v.y = m_Res.y;
		q.v.z = m_Res.z;
		q.w = m_Res.w;

#endif
		//			res.Normalize();
		//return res;
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
	//	unsigned short int m_1;
	//	unsigned short int m_2;
	//	unsigned short int m_3;
	//	unsigned short int m_4;
	unsigned int m_1; // m_2...m_1
	unsigned int m_2; // m_4...m_3

	void         ToInternalType(Quat& q)
	{

		LONGDWORD m_Val;

		Float4Storage tmp = q;
		tmp.Normalize();

		m_Val = 0;
		int offset[3];

		int index = 0;
		float max = fabs(tmp.comp[0]);

		for (int i = 0, o = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i + 1]))
			{
				offset[o++] = index;
				index = i + 1;
				max = fabs(tmp.comp[i + 1]);
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

		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i != index)
			{
				offset[c++] = i;
			}
		}

		int c = 0;

		// don't store maximum value - delete it;

		LONGDWORD packdw = (LONGDWORD)(floor((tmp.comp[offset[0]] + RANGE_20BIT) * MAX_20BITf + 0.5f));

		m_Val |= packdw << c;
		c += 20;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[1]] + RANGE_20BIT) * MAX_20BITf + 0.5f));

		m_Val |= packdw << c;
		c += 20;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[2]] + RANGE_20BIT) * MAX_20BITf + 0.5f));

		m_Val |= packdw << c;

		LONGDWORD inddw(index);

		m_Val |= inddw << 62;

		//save m_xxx

		m_1 = static_cast<unsigned int>(m_Val);
		m_2 = static_cast<unsigned int>(m_Val >> 32);

	}

	void ToExternalType(Quat& q) const
	{
		Float4Storage res;

		//LONGDWORD m_Val;
		////create m_Val
		//LONGDWORD m2(m_2), m3(m_3), m4(m_4);
		//m_Val = m_1 + (m2 << 16) + (m3 << 32) + (m4 << 48);

		unsigned short int index = static_cast<unsigned short int>((m_2 >> 30) & 3);

		int offset[3];

		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i != index)
			{
				offset[c++] = i;
			}
		}

		//int c = 0;
		//float sqrsumm = 0.0f;

		//int mask = 0xFFFFF;

		//int i = offset[0];
		//int packed = static_cast<int>((m_Val >> c) & mask);
		//res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		//sqrsumm += res.comp[i]* res.comp[i];
		//c += 20;

		//i = offset[1];
		//packed = static_cast<int>((m_Val >> c) & mask);
		//res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		//sqrsumm += res.comp[i]* res.comp[i];
		//c += 20;

		//mask = 0xFFFFF;
		//i = offset[2];
		//packed = static_cast<int>((m_Val >> c) & mask);
		//res.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		//sqrsumm += res.comp[i]* res.comp[i];

		//res.comp[index] = sqrtf(1.0f - sqrsumm);

		CRY_ALIGN(16) Float4Storage m_Res;
#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)

		m_Res.comp[offset[0]] = (float)((int)(m_1 & 0xFFFFF));
		m_Res.comp[offset[1]] = (float)((int)(((m_1 >> 20) + (m_2 << 12)) & 0xFFFFF));
		m_Res.comp[offset[2]] = (float)((int)((m_2 >> 8) & 0xFFFFF));

		const float max = MAX_20BITf;//, MAX_21BITf, MAX_21BITf, MAX_21BITf};
		const float range = RANGE_20BIT;//, RANGE_21BIT, RANGE_21BIT, RANGE_21BIT};

		__m128 src = _mm_load_ps(&m_Res.comp[0]);
		__m128 div = _mm_load1_ps(&max);
		__m128 ran = _mm_load1_ps(&range);
		__m128 a = _mm_div_ps(src, div);
		__m128 b = _mm_sub_ps(a, ran);
		//
		__m128 sq = _mm_mul_ps(b, b);

		// values
		_mm_store_ps(&m_Res.comp[0], b);
		CRY_ALIGN(16) Float4Storage squares;
		_mm_store_ps(&squares.comp[0], sq);

		m_Res.comp[index] = sqrtf(1 - squares[offset[0]] - squares[offset[1]] - squares[offset[2]]);

#else

		//		LONGDWORD m_Val;
		//	LONGDWORD m2(m_2), m3(m_3);
		//		m_Val = m_1 + (m2 << 16) + (m3 << 32);

		int c = 0;
		float sqrsumm = 0.0f;

		//		int mask = 0x7FFF;

		int i = offset[0];
		int packed = int(m_1 & 0xFFFFF);
		m_Res.comp[i] = ((float)packed / MAX_20BITf) - RANGE_20BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];
		c += 20;

		i = offset[1];
		packed = int(((m_1 >> 20) + (m_2 << 12)) & 0xFFFFF);
		m_Res.comp[i] = ((float)packed / MAX_20BITf) - RANGE_20BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];
		c += 20;

		i = offset[2];
		packed = int((m_2 >> 8) & 0xFFFFF);
		m_Res.comp[i] = ((float)packed / MAX_20BITf) - RANGE_20BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];

		m_Res.comp[index] = sqrtf(1.0f - sqrsumm);
#endif

		//#endif
		q.v.x = res.x;
		q.v.y = res.y;
		q.v.z = res.z;
		q.w = res.w;

		//		return res;
	}

	static uint32 GetFormat()
	{
		return eSmallTree64BitQuat;
	}

};

struct SmallTree64BitExtQuat
{
	typedef int64 LONGDWORD;
public:
	unsigned int m_1; // m_2...m_1
	unsigned int m_2; // m_4...m_3

	void         ToInternalType(Quat& q)
	{

		LONGDWORD m_Val;

		Float4Storage tmp = q;
		tmp.Normalize();

		m_Val = 0;
		int offset[3];

		int index = 0;
		float max = fabs(tmp.comp[0]);

		for (int i = 0, o = 0; i < 3; ++i)
		{
			if (max < fabs(tmp.comp[i + 1]))
			{
				offset[o++] = index;
				index = i + 1;
				max = fabs(tmp.comp[i + 1]);
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

		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i != index)
			{
				offset[c++] = i;
			}
		}

		int c = 0;

		// don't store maximum value - delete it;
		LONGDWORD packdw = (LONGDWORD)(floor((tmp.comp[offset[0]] + RANGE_21BIT) * MAX_21BITf + 0.5f));

		m_Val |= packdw << c;
		c += 21;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[1]] + RANGE_21BIT) * MAX_21BITf + 0.5f));

		m_Val |= packdw << c;
		c += 21;

		packdw = (LONGDWORD)(floor((tmp.comp[offset[2]] + RANGE_20BIT) * MAX_20BITf + 0.5f));

		m_Val |= packdw << c;

		LONGDWORD inddw(index);

		m_Val |= inddw << 62;

		m_1 = static_cast<unsigned int>(m_Val);
		m_2 = static_cast<unsigned int>(m_Val >> 32);

	}

	void ToExternalType(Quat& q) const
	{
		unsigned int index = static_cast<unsigned int>((m_2 >> 30) & 3);
		int offset[3];
		for (int i = 0, c = 0; i < 4; i++)
		{
			if (i != index)
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

		CRY_ALIGN(16) Float4Storage m_Res;
#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
		CRY_ALIGN(16) Float4Storage max;
		CRY_ALIGN(16) Float4Storage range;

		m_Res.comp[offset[0]] = (float)((int)(m_1 & 0x1FFFFF));
		m_Res.comp[offset[1]] = (float)((int)(((m_1 >> 21) + (m_2 << 11)) & 0x1FFFFF));
		m_Res.comp[offset[2]] = (float)((int)((m_2 >> 10) & 0xFFFFF));

		//		static const float max = MAX_20BITf;//, MAX_21BITf, MAX_21BITf, MAX_21BITf};
		//		static const float range = RANGE_20BIT;//, RANGE_21BIT, RANGE_21BIT, RANGE_21BIT};
		max[offset[0]] = MAX_21BITf;
		max[offset[1]] = MAX_21BITf;
		max[offset[2]] = MAX_20BITf;
		max[index] = 1.0f;

		range[offset[0]] = RANGE_21BIT;
		range[offset[1]] = RANGE_21BIT;
		range[offset[2]] = RANGE_20BIT;
		range[index] = 1.0f;

		__m128 src = _mm_load_ps(&m_Res.comp[0]);
		__m128 div = _mm_load_ps(&max.comp[0]);
		__m128 ran = _mm_load_ps(&range.comp[0]);
		__m128 a = _mm_div_ps(src, div);
		__m128 b = _mm_sub_ps(a, ran);
		//
		__m128 sq = _mm_mul_ps(b, b);

		// values
		_mm_store_ps(&m_Res.comp[0], b);
		CRY_ALIGN(16) Float4Storage squares;
		_mm_store_ps(&squares.comp[0], sq);

		m_Res.comp[index] = sqrtf(1 - squares[offset[0]] - squares[offset[1]] - squares[offset[2]]);

#else

		int c = 0;
		float sqrsumm = 0.0f;

		//		int mask = 0x7FFF;

		int i = offset[0];
		int packed = (/*(int)*/ (m_1 & 0x1FFFFF));
		m_Res.comp[i] = ((float)packed / MAX_21BITf) - RANGE_21BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];
		c += 21;

		i = offset[1];
		packed = (/*(int)*/ (((m_1 >> 21) + (m_2 << 11)) & 0x1FFFFF));
		m_Res.comp[i] = ((float)packed / MAX_21BITf) - RANGE_21BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];
		c += 21;

		i = offset[2];
		packed = (/*(int)*/ ((m_2 >> 10) & 0xFFFFF));
		m_Res.comp[i] = ((float)packed / MAX_20BITf) - RANGE_20BIT;
		sqrsumm += m_Res.comp[i] * m_Res.comp[i];

		//		if (sqrsumm < 1.0f)
		m_Res.comp[index] = sqrtf(1.0f - sqrsumm);
		//	else
		//			m_Res.comp[index] = 0.0f;

		//CRY_ALIGN(16) Float4Storage m_Res1;

		//LONGDWORD m_Val;
		//LONGDWORD m2(m_2);
		//m_Val = m_1 + (m2 << 32);

		//c = 0;
		//sqrsumm = 0.0f;

		//int mask = 0x1FFFFF;

		//i = offset[0];
		//packed = static_cast<int>((m_Val >> c) & mask);
		//m_Res1.comp[i] = static_cast<float>((float)packed  / MAX_21BITf ) - RANGE_21BIT;
		//sqrsumm += m_Res1.comp[i]* m_Res1.comp[i];
		//c += 21;
		//assert(m_Res1.comp[i] == m_Res.comp[i]);

		//i = offset[1];
		//packed = static_cast<int>((m_Val >> c) & mask);
		//m_Res1.comp[i] = static_cast<float>((float)packed  / MAX_21BITf ) - RANGE_21BIT;
		//sqrsumm += m_Res1.comp[i]* m_Res1.comp[i];
		//c += 21;
		//assert(m_Res1.comp[i] == m_Res.comp[i]);
		//mask = 0xFFFFF;
		//i = offset[2];
		//packed = static_cast<int>((m_Val >> c) & mask);
		//m_Res1.comp[i] = static_cast<float>((float)packed  / MAX_20BITf ) - RANGE_20BIT;
		//sqrsumm += m_Res1.comp[i]* m_Res1.comp[i];
		//assert(m_Res1.comp[i] == m_Res.comp[i]);

		//m_Res1.comp[index] = sqrtf(1.0f - sqrsumm);
		//assert(m_Res1.comp[index] == m_Res.comp[index]);

#endif

		q.v.x = m_Res.x;
		q.v.y = m_Res.y;
		q.v.z = m_Res.z;
		q.w = m_Res.w;

		//return res;
	}

	static uint32 GetFormat()
	{
		return eSmallTree64BitExtQuat;
	}

};

//#define PI (3.1415926535897932384626433832795028841971693993751f)
#define deg2rad(d) (((d) * PI) / 180.0f)
#define rad2deg(r) (((r) * 180.0f) / PI)

struct PolarCoordinates
{

	float m_Yaw;
	float m_Pitch;
	//float m_Ro;

	void FromVec3(float x, float z, float y)
	{

		float S = sqrtf(x * x + y * y + z * z);

		if (S > 0.0f)
		{
			double dTheta = acos_tpl(z / S);

			float v2norm = sqrtf(x * x + y * y);

			//x /= v2norm;

			double dRho;

			if (v2norm > 0.0f)
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

			this->m_Pitch = (float) dTheta;
			this->m_Yaw = (float) dRho;
		}
		else
		{
			//				m_Ro = 0.0f;
			m_Pitch = 0.0f;
			m_Yaw = 0.0f;
		}

	}

	void ToVec3(float& x, float& z, float& y)
	{
		//			m_Ro = 1.0f;
		double sin_theta = sin(this->m_Pitch);
		double cos_theta = cos(this->m_Pitch);
		double sin_rho = sin(this->m_Yaw);
		double cos_rho = cos(this->m_Yaw);
		y = (float) (sin_theta * sin_rho);
		x = (float) (sin_theta * cos_rho);
		z = (float) cos_theta;
	}

};
