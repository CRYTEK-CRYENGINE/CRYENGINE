// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShadowUtils.cpp :

   Revision history:
* Created by Tiago Sousa
   =============================================================================*/
#include "StdAfx.h"

#if CRY_PLATFORM_ORBIS
	#define real complex_real
#endif

#pragma warning(disable:4167) // 'sqrt' : only available as an intrinsic function
#include <complex>
#include <CryThreading/IJobManager_JobDelegator.h>

#define WATER_UPDATE_THREAD_NAME "WaterUpdate"

#define g_nGridSize              64
#define g_nGridLogSize           6

static float gaussian_y2;
static int gaussian_use_last = 0;

#define m_nGridSize    (int)g_nGridSize
#define m_nLogGridSize (int)g_nGridLogSize // log2(64)
#define m_fG           (9.81f)

// potential todo list:
//	- vectorizing/use intrinsics
//	-	support for N sized grids
//	- support for gpu update
//	- tiled grid updates ?

struct SWaterUpdateThreadInfo
{
	int   nFrameID;
	float fTime;
	bool  bOnlyHeight;
};

void WaterAsyncUpdate(CWaterSim*, int, float, bool, void*);
DECLARE_JOB("WaterUpdate", TWaterUpdateJob, WaterAsyncUpdate);

class CRY_ALIGN(128) CWaterSim
{
	typedef std::complex<float> complexF;

public:

	CWaterSim()
		: m_nFrameID(0)
		  , m_nWorkerThreadID(0)
		  , m_nFillThreadID(0)
		  , m_bQuit(false)
		  , m_fA(1.0f)
		  , m_fWind(0.0f)
		  , m_fWorldSizeX(1.0f)
		  , m_fWorldSizeY(1.0f)
		  , m_fWorldSizeXInv(1.0f)
		  , m_fWorldSizeYInv(1.0f)
		  , m_fMaxWaveSize(200.0f)
		  , m_fChoppyWaveScale(400.0f)
	{
		ZeroArray(m_pThreadInfo);
		ZeroArray(m_JobInfo);

		uint32 nSize = m_nGridSize * m_nGridSize * sizeof(complexF);
		memset(&m_pFourierAmps[0], 0, nSize);
		memset(&m_pHeightField[0], 0, nSize);
		memset(&m_pDisplaceFieldX[0], 0, nSize);
		memset(&m_pDisplaceFieldY[0], 0, nSize);

		nSize = m_nGridSize * m_nGridSize * sizeof(Vec4);
		memset(m_pDisplaceGrid[0], 0, nSize);
		memset(m_pDisplaceGrid[1], 0, nSize);
		memset(m_pLUTK, 0, nSize);

		m_nWorkerThreadID = m_nFillThreadID = 0;
		m_bQuit = false;
	}

	virtual ~CWaterSim()
	{
		Release();
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	virtual void Create(float fA, float fWind, float fWorldSizeX, float fWorldSizeY)       // Create/Initialize water simulation
	{
		m_fA = fA;
		m_fWind = 0.0f;   //fWind; assume constant direction
		m_fWorldSizeX = fWorldSizeX;
		m_fWorldSizeY = fWorldSizeY;
		m_fWorldSizeXInv = 1.f / m_fWorldSizeX;
		m_fWorldSizeYInv = 1.f / m_fWorldSizeY;
		InitTableK();
		InitFourierAmps();
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	virtual void Release()
	{
		uint32 nSize = m_nGridSize * m_nGridSize * sizeof(std::complex<float> );
		memset(&m_pFourierAmps[0], 0, nSize);
		memset(&m_pHeightField[0], 0, nSize);
		memset(&m_pDisplaceFieldX[0], 0, nSize);
		memset(&m_pDisplaceFieldY[0], 0, nSize);

		nSize = m_nGridSize * m_nGridSize * sizeof(Vec4);
		memset(m_pDisplaceGrid, 0, nSize);
		memset(m_pLUTK, 0, nSize);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	virtual void SaveToDisk(const char* pszFileName)
	{
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// Returns (-1)^n
	int powNeg1(int n)
	{
		int pow[2] = { 1, -1 };
		return pow[n & 1];
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// gaussian random number, using box muller technique
	float frand_gaussian(float m = 0.0f, float s = 1.0f)  // m- mean, s - standard deviation
	{
		float x1, x2, w, y1;
		if (gaussian_use_last)     // use value from previous call
		{
			y1 = gaussian_y2;
			gaussian_use_last = 0;
		}
		else
		{
			do
			{
				x1 = cry_random(-1.0f, 1.0f);
				x2 = cry_random(-1.0f, 1.0f);

				w = x1 * x1 + x2 * x2;
			}
			while (w >= 1.0f);

			w = sqrt_fast_tpl((-2.0f * log_tpl(w)) / w);
			y1 = x1 * w;
			gaussian_y2 = x2 * w;

			gaussian_use_last = 1;
		}

		return (m + y1 * s);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////u///////////////////////////////////////////////////////////////////

	inline int GetGridOffset(const int& x, const int& y) const
	{
		return y * m_nGridSize + x;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	int GetMirrowedGridOffset(const int& x, const int& y) const
	{
		return GetGridOffset((m_nGridSize - x) & (m_nGridSize - 1), (m_nGridSize - y) & (m_nGridSize - 1));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	int GetGridOffsetWrapped(const int& x, const int& y) const
	{
		return GetGridOffset(x & (m_nGridSize - 1), y & (m_nGridSize - 1));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	void ComputeFFT1D(int nDir, float* pReal, float* pImag)
	{
		// reference: "2 Dimensional FFT" - by Paul Bourke
		long nn, i, i1, j, k, i2, l, l1, l2;
		float c1, c2, fReal, fImag, t1, t2, u1, u2, z;

		nn = m_nGridSize;
		float fRecipNN = 1.0f / (float) nn;

		// Do the bit reversal
		i2 = nn >> 1;
		j = 0;
		for (i = 0; i < nn - 1; ++i)
		{
			if (i < j)
			{
				fReal = pReal[i];
				fImag = pImag[i];
				pReal[i] = pReal[j];
				pImag[i] = pImag[j];
				pReal[j] = fReal;
				pImag[j] = fImag;
			}

			k = i2;
			while (k <= j)
			{
				j -= k;
				k >>= 1;
			}

			j += k;
		}

		// FFT computation
		c1 = -1.0f;
		c2 = 0.0f;
		l2 = 1;
		for (l = 0; l < m_nLogGridSize; ++l)
		{
			l1 = l2;
			l2 <<= 1;
			u1 = 1.0;
			u2 = 0.0;
			for (j = 0; j < l1; ++j)
			{
				for (i = j; i < nn; i += l2)
				{
					i1 = i + l1;
					t1 = u1 * pReal[i1] - u2 * pImag[i1];
					t2 = u1 * pImag[i1] + u2 * pReal[i1];
					pReal[i1] = pReal[i] - t1;
					pImag[i1] = pImag[i] - t2;
					pReal[i] += t1;
					pImag[i] += t2;
				}

				z = u1 * c1 - u2 * c2;
				u2 = u1 * c2 + u2 * c1;
				u1 = z;
			}

			c2 = sqrt_fast_tpl((1.0f - c1) * 0.5f);

			if (nDir == 1)
			{
				c2 = -c2;
			}

			c1 = sqrt_fast_tpl((1.0f + c1) * 0.5f);
		}

		// Scaling for forward transform
		if (nDir == 1)
		{
			for (i = 0; i < nn; ++i)
			{
				pReal[i] *= fRecipNN;
				pImag[i] *= fRecipNN;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	void ComputeFFT2D(int nDir, complexF* const __restrict c)
	{
		// reference: "2 Dimensional FFT" - by Paul Bourke
		float pReal[m_nGridSize];
		float pImag[m_nGridSize];

		// Transform the rows
		for (int y(0); y < m_nGridSize; ++y)
		{
			for (int x(0); x < m_nGridSize; ++x)
			{
				int nGridOffset = GetGridOffset(x, y);
				const complexF& curC = c[nGridOffset];
				pReal[x] = curC.real();
				pImag[x] = curC.imag();
			}

			ComputeFFT1D(nDir, pReal, pImag);

			for (int x(0); x < m_nGridSize; ++x)
			{
				int nGridOffset = GetGridOffset(x, y);
				c[nGridOffset] = complexF(pReal[x], pImag[x]);
			}
		}

		// Transform the columns
		for (int x(0); x < m_nGridSize; ++x)
		{
			for (int y(0); y < m_nGridSize; ++y)
			{
				int nGridOffset = GetGridOffset(x, y);
				const complexF& curC = c[nGridOffset];
				pReal[y] = curC.real();
				pImag[y] = curC.imag();
			}

			ComputeFFT1D(nDir, pReal, pImag);

			for (int y(0); y < m_nGridSize; ++y)
			{
				int nGridOffset = GetGridOffset(x, y);
				c[nGridOffset] = complexF(pReal[y], pImag[y]);
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	float GetIndexToWorldX(int x)
	{
		static const float PI2ByWorldSizeX = (2.0f * PI) / m_fWorldSizeX;
		return (float(x) - ((float)m_nGridSize / 2.0f)) * PI2ByWorldSizeX;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	float GetIndexToWorldY(int y)
	{
		static const float PI2ByWorldSizeY = (2.0f * PI) / m_fWorldSizeY;
		return (float(y) - ((float)m_nGridSize / 2.0f)) * PI2ByWorldSizeY;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	float GetTermAngularFreq(float k)
	{
		// reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.2)
		return sqrt_fast_tpl(k * m_fG);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// a - constant, pK - wave dir (2d), pW - wind dir (2d)
	float ComputePhillipsSpec(const Vec2& pK, const Vec2& pW) const
	{
		float fK2 = pK.GetLength2();
		if (fK2 == 0)
		{
			return 0.0f;
		}

		float fW2 = pW.GetLength2();
		float fL = fW2 / m_fG;
		float fL2 = sqr(fL);

		// reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.3)
		float fPhillips = m_fA * (exp_tpl(-1.0f / (fK2 * fL2)) / sqr(fK2)) * (sqr(pK.Dot(pW)) / fK2 * fW2);

		assert(fPhillips >= 0);

		// Return Phillips spectrum
		return fPhillips;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	void InitTableK()
	{
		for (int y(0); y < m_nGridSize; ++y)
		{
			float fKy = GetIndexToWorldY(y);
			for (int x(0); x < m_nGridSize; ++x)
			{
				float fKx = GetIndexToWorldX(x);
				float fKLen = sqrt_tpl(fKx * fKx + fKy * fKy);
				float fAngularFreq = GetTermAngularFreq(fKLen);
				m_pLUTK[GetGridOffset(x, y)] = Vec4(fKx, fKy, fKLen, fAngularFreq);
			}
		}
	}

	// Initialize Fourier amplitudes table
	void InitFourierAmps()
	{
		const float recipSqrt2 = 1.0f / sqrt_fast_tpl(2.0f);

		Vec2 pW(cosf(m_fWind), sinf(m_fWind));
		pW = -pW;     // meh - match with regular water animation
		//pW *= m_fWindScale;

		for (int y(0); y < m_nGridSize; ++y)
		{
			//float fKy=GetIndexToWorldY(y);
			for (int x(0); x < m_nGridSize; ++x)
			{
				complexF e(frand_gaussian(), frand_gaussian());
				int nGridOffset = GetGridOffset(x, y);

				Vec4 pLookupK = m_pLUTK[nGridOffset];
				Vec2 pK = Vec2(pLookupK.x, pLookupK.y);

				//float fKx=GetIndexToWorldX(x);
				//Vec2 pK(fKx, fKy);

				// reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.4)
				e *= recipSqrt2 * sqrt_fast_tpl(ComputePhillipsSpec(pK, pW));
				m_pFourierAmps[nGridOffset] = e;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// Update simulation
	void Update(SWaterUpdateThreadInfo& pThreadInfo)
	{
		int nFrameID = pThreadInfo.nFrameID;
		float fTime = pThreadInfo.fTime;
		bool bOnlyHeight = pThreadInfo.bOnlyHeight;

		PROFILE_FRAME(CWaterSim::Update);

		// Optimization: only half grid is required to update, since we can use conjugate to the other half

		float fHalfY = (m_nGridSize >> 1) + 1;
		for (int y(0); y < fHalfY; ++y)
		{
			for (int x(0); x < m_nGridSize; ++x)
			{
				int offset = GetGridOffset(x, y);
				int offsetMirrowed = GetMirrowedGridOffset(x, y);

				Vec4 pK = m_pLUTK[offset];

				float fKLen = pK.z;                //pK.GetLength();
				float fAngularFreq = pK.w * fTime; //GetTermAngularFreq(fKLen)

				float fAngularFreqSin = 0, fAngularFreqCos = 0;
#if CRY_PLATFORM_ORBIS // Workaround for bug in Orbis maths library which means sinf becomes orders of magnitude slower for big numbers
				fAngularFreq = fAngularFreq * (1.0f / g_PI2);
				fAngularFreq = (fAngularFreq - floorf(fAngularFreq)) * g_PI2;
#endif
				sincos_tpl((f32)fAngularFreq, (f32*) &fAngularFreqSin, (f32*)&fAngularFreqCos);

				complexF ep(fAngularFreqCos, fAngularFreqSin);
				complexF em = conj(ep);

				// reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.4)
				complexF currWaveField = m_pFourierAmps[offset] * ep + conj(m_pFourierAmps[offsetMirrowed]) * em;

				m_pHeightField[offset] = currWaveField;
				if (!bOnlyHeight && fKLen)
				{
					m_pDisplaceFieldX[offset] = currWaveField * ((fKLen == 0) ? complexF(0) : complexF(0, (-pK.x - pK.y) / fKLen));
					//m_pDisplaceFieldX[offset] =  complexF( (fimag * pK.x + freal * pK.y)/fKLen, (-freal * pK.x + fimag * pK.y)/fKLen );
					m_pDisplaceFieldY[offset] = currWaveField * ((fKLen == 0) ? complexF(0) : complexF(0, -pK.y / fKLen));
				}
				else
				{
					m_pDisplaceFieldX[offset] = 0;
					m_pDisplaceFieldY[offset] = 0;
				}

				// Set upper half using conjugate
				if (y != fHalfY - 1)
				{
					m_pHeightField[offsetMirrowed] = conj(m_pHeightField[offset]);
					if (!bOnlyHeight)
					{
						m_pDisplaceFieldX[offsetMirrowed] = conj(m_pDisplaceFieldX[offset]);
						m_pDisplaceFieldY[offsetMirrowed] = conj(m_pDisplaceFieldY[offset]);
					}
				}
			}
		}

		ComputeFFT2D(-1, m_pHeightField);
		if (!bOnlyHeight)
		{
			ComputeFFT2D(-1, m_pDisplaceFieldX);
			ComputeFFT2D(-1, m_pDisplaceFieldY);
		}

		for (int y(0); y < m_nGridSize; ++y)
		{
			for (int x(0); x < m_nGridSize; ++x)
			{
				int offset = GetGridOffset(x, y);
				int currPowNeg1 = powNeg1(x + y);

				m_pHeightField[offset] *= currPowNeg1 * m_fMaxWaveSize;
				if (!bOnlyHeight)
				{
					m_pDisplaceFieldX[offset] *= currPowNeg1 * m_fChoppyWaveScale;
					m_pDisplaceFieldY[offset] *= currPowNeg1 * m_fChoppyWaveScale;
				}

				m_pDisplaceGrid[m_nWorkerThreadID][offset] = Vec4(m_pDisplaceFieldX[offset].real(),
				                                                  m_pDisplaceFieldY[offset].real(),
				                                                  //m_pDisplaceFieldX[offset].real(), //imag(),
				                                                  -m_pHeightField[offset].real(),
				                                                  0.0f);     // store encoded normal ?
			}
		}
	}

	void Update(int nFrameID, float fTime, bool bOnlyHeight)
	{
		CRenderer* rd = gRenDev;

		m_nFillThreadID = m_nWorkerThreadID = 0;

		SWaterUpdateThreadInfo& pThreadInfo = m_pThreadInfo[m_nFillThreadID];
		pThreadInfo.nFrameID = nFrameID;
		pThreadInfo.fTime = fTime;
		pThreadInfo.bOnlyHeight = bOnlyHeight;

		Update(pThreadInfo);

	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	Vec3 GetPositionAt(int x, int y) const
	{
		Vec4 pPos = m_pDisplaceGrid[m_nFillThreadID][GetGridOffsetWrapped(x, y)];
		return Vec3(pPos.x, pPos.y, pPos.z);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	float GetHeightAt(int x, int y) const
	{
		return m_pDisplaceGrid[m_nFillThreadID][GetGridOffsetWrapped(x, y)].z;
	}

	Vec4* GetDisplaceGrid()
	{
		return m_pDisplaceGrid[m_nFillThreadID];
	}

	static CWaterSim* GetInstance()
	{
		static CWaterSim pInstance;
		return &pInstance;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	void SpawnUpdateJob(int nFrameID, float fTime, bool bOnlyHeight, void* pRawPtr)
	{
		if (nFrameID != m_nFrameID)
			m_nFrameID = nFrameID;
		else
			return;

		WaitForJob();
		TWaterUpdateJob job(this, nFrameID, fTime, bOnlyHeight, pRawPtr);
		job.RegisterJobState(&m_JobState);
		job.Run();
	}

	void WaitForJob()
	{
		gEnv->GetJobManager()->WaitForJob(m_JobState);
	}

protected:

	// Double buffered displacement grid
	CRY_ALIGN(128) Vec4 m_pDisplaceGrid[2][g_nGridSize * g_nGridSize];
	// Simulation constants
	CRY_ALIGN(128) Vec4 m_pLUTK[g_nGridSize * g_nGridSize];                // pre-computed K table
	CRY_ALIGN(128) complexF m_pFourierAmps[m_nGridSize * m_nGridSize];     // Fourier amplitudes at time 0 (aka. H0)
	CRY_ALIGN(128) complexF m_pHeightField[m_nGridSize * m_nGridSize];     // Current Fourier amplitudes height field
	CRY_ALIGN(128) complexF m_pDisplaceFieldX[m_nGridSize * m_nGridSize];  // Current Fourier amplitudes displace field
	CRY_ALIGN(128) complexF m_pDisplaceFieldY[m_nGridSize * m_nGridSize];  // Current Fourier amplitudes displace field

	SWaterUpdateThreadInfo m_pThreadInfo[2];

	int m_nFrameID;
	uint32 m_nWorkerThreadID;
	uint32 m_nFillThreadID;
	CryEvent m_Event;
	bool m_bQuit;

	float m_fA;          // constant value
	float m_fWind;       // window direction angle
	float m_fWorldSizeX; // world dimensions
	float m_fWorldSizeY; // world dimensions

	float m_fWorldSizeXInv; // 1.f / world dimensions
	float m_fWorldSizeYInv; // 1.f / world dimensions

	float m_fMaxWaveSize;
	float m_fChoppyWaveScale;

	JobManager::SJobState m_JobState;
	SWaterUpdateThreadInfo m_JobInfo[2];
};

void WaterAsyncUpdate(CWaterSim* pWaterSim, int nFrameID, float fTime, bool bOnlyHeight, void* pRawPtr)
{
	if (pWaterSim == NULL)
		return;

	pWaterSim->Update(nFrameID, fTime, bOnlyHeight);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::Create(float fA, float fWind, float fWorldSizeX, float fWorldSizeY)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Water Sim");
	Release();
	m_pWaterSim = CryAlignedNew<CWaterSim>(); //::GetInstance();
	m_pWaterSim->Create(fA, fWind, fWorldSizeX, fWorldSizeY);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::Release()
{
	if (m_pWaterSim)
		m_pWaterSim->WaitForJob();
	CryAlignedDelete(m_pWaterSim);
	//m_pWaterSim = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::SaveToDisk(const char* pszFileName)
{
	assert(m_pWaterSim);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
void CWater::Update(int nFrameID, float fTime, bool bOnlyHeight, void* pRawPtr)
{
	if (m_pWaterSim)
		m_pWaterSim->SpawnUpdateJob(nFrameID, fTime, bOnlyHeight, pRawPtr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

Vec3 CWater::GetPositionAt(int x, int y) const
{
	if (m_pWaterSim)
		return m_pWaterSim->GetPositionAt(x, y);

	return Vec3(ZERO);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

float CWater::GetHeightAt(int x, int y) const
{
	if (m_pWaterSim)
		return m_pWaterSim->GetHeightAt(x, y);

	return 0.0f;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

Vec4* CWater::GetDisplaceGrid() const
{
	if (m_pWaterSim)
		return m_pWaterSim->GetDisplaceGrid();
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

const int CWater::GetGridSize() const
{
	return g_nGridSize;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_pWaterSim);
}
