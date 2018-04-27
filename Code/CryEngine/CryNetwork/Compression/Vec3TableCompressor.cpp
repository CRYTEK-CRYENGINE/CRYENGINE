// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "SegmentedCompressionSpace.h"
#include "Quantizer.h"
#include "BoolCompress.h"
#include "Protocol/Serialize.h"

#if USE_MEMENTO_PREDICTORS

	#define INTEGER_ERROR_ENCODE_MATH
	#define INTEGER_ERROR_ENCODE_EXTRA_BITS 14

namespace
{
struct SEncodeResult
{
	Vec3i error;
	Vec3i quantized;
	bool  compressible;
};

template<class TPredictor>
class CVec3TablePolicy
{
public:
	CVec3TablePolicy() : m_hadMemento(-1) {}

	ILINE bool Load(XmlNodeRef node, const string& filename)
	{
		if (XmlNodeRef table = node->findChild("Table"))
		{
			if (!m_space.Load(table->getAttr("file")))
				return false;
		}
		else
			return false;

		if (!m_predictor.Load(node, m_space.GetBitCount()))
			return false;

		return true;
	}

	ILINE bool ReadMemento(CByteInputStream& in) const
	{
		m_hadMemento = 1;
		m_compressible.ReadMemento(in);
		return m_predictor.ReadMemento(in);
	}

	ILINE bool WriteMemento(CByteOutputStream& out) const
	{
		m_compressible.WriteMemento(out);
		return m_predictor.WriteMemento(out);
	}

	ILINE void NoMemento() const
	{
		m_hadMemento = 2;
		m_compressible.NoMemento();
		m_predictor.NoMemento();
	}

	#if USE_ARITHSTREAM
	ILINE bool ReadValue(CCommInputStream& in, Vec3& value, CArithModel* pModel, uint32 age) const
	{
		if (m_compressible.ReadValue(in))
		{
			Vec3i data;
			m_space.Decode(in, &data[0], 3);
			if (!m_predictor.Decode(data, age, value))
			{
				NetWarning("CVec3TablePolicy::ReadValue: failed decoding; m_hadMemento=%d", m_hadMemento);
				return false;
			}
		}
		else
		{
			Vec3i quantized;
			for (int i = 0; i < 3; i++)
				quantized[i] = in.ReadBits(m_space.GetBitCount());
			value = m_predictor.DecodeQuantized(quantized);
		}
		m_hadMemento = 3;
		NetLogPacketDebug("CVec3TablePolicy::ReadValue (%f, %f, %f) NumBits %d (%f)", value.x, value.y, value.z, m_space.GetBitCount(), in.GetBitSize());
		return true;
	}
	ILINE bool WriteValue(CCommOutputStream& out, const Vec3& value, CArithModel* pModel, uint32 age) const
	{
		SEncodeResult er = m_predictor.Encode(value, age);
		if (er.compressible)
			er.compressible &= m_space.CanEncode(&er.error[0], 3);
		m_compressible.WriteValue(out, er.compressible);
		if (er.compressible)
			m_space.Encode(out, &er.error[0], 3);
		else
		{
			for (int i = 0; i < 3; i++)
				out.WriteBits(er.quantized[i], m_space.GetBitCount());
		}
		m_hadMemento = 4;

		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("Vec3TablePolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("Vec3TablePolicy: not implemented for generic types");
		return false;
	}
	#else
	ILINE bool ReadValue(CNetInputSerializeImpl* in, Vec3& value, uint32 age) const
	{
		Vec3i quantized;

		for (int i = 0; i < 3; i++)
		{
			quantized[i] = in->ReadBits(m_space.GetBitCount());
		}

		value = m_predictor.DecodeQuantized(quantized);
		NetLogPacketDebug("CVec3TablePolicy::ReadValue (%f, %f, %f) NumBits %d (%f)", value.x, value.y, value.z, m_space.GetBitCount(), in.GetBitSize());

		m_hadMemento = 3;

		return true;
	}

	ILINE bool WriteValue(CNetOutputSerializeImpl* out, const Vec3& value, uint32 age) const
	{
		SEncodeResult er = m_predictor.Encode(value, age);

		for (int i = 0; i < 3; i++)
		{
			out->WriteBits(er.quantized[i], m_space.GetBitCount());
		}

		m_hadMemento = 4;

		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("Vec3TablePolicy: not implemented for generic types");
		return false;
	}

	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("Vec3TablePolicy: not implemented for generic types");
		return false;
	}
	#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CVec3TablePolicy");

		pSizer->Add(*this);
		m_space.GetMemoryStatistics(pSizer);
	}
	#if NET_PROFILE_ENABLE
	int GetBitCount(Vec3 value)
	{
		return m_compressible.GetBitCount() + (m_space.GetBitCount() * 3);
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
	#endif
private:
	CBoolCompress              m_compressible;
	TPredictor                 m_predictor;
	CSegmentedCompressionSpace m_space;
	mutable int                m_hadMemento;
};

class CDirectionalPredictor
{
public:
	ILINE bool Load(XmlNodeRef node, int bits)
	{
		if (!m_quantizer.Load(node, "unknown", "Params", eFQM_RoundLeft, bits))
			return false;

		return true;
	}

	ILINE bool ReadMemento(CByteInputStream& in) const
	{
		m_nValues = in.GetTyped<uint8>();
		NET_ASSERT(/*m_nValues >= 0 && */ m_nValues <= 2);
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				m_quantized[i][j] = in.GetTyped<int32>();
		return true;
	}
	ILINE bool WriteMemento(CByteOutputStream& out) const
	{
		NET_ASSERT(/*m_nValues >= 0 && */ m_nValues <= 2);
		out.PutTyped<uint8>() = m_nValues;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				out.PutTyped<int32>() = m_quantized[i][j];
		return true;
	}
	ILINE void NoMemento() const
	{
		m_nValues = 0;
	}
	ILINE SEncodeResult Encode(const Vec3& value, int age) const
	{
		Vec3i encoded = Quantize(value);
		Vec3i send = encoded;
		if (m_nValues == 2)
		{
			Vec3i pred = GetPrediction();
			send = EncodeErr(encoded - pred);
			encoded = ClampVec(pred + DecodeErr(send));
		}
		SEncodeResult er;
		er.quantized = encoded;
		er.error = send;
		er.compressible = (m_nValues == 2);
		Update(encoded);
		return er;
	}
	ILINE bool Decode(Vec3i encoded, int age, Vec3& out) const
	{
		if (m_nValues != 2)
			return false;
		Vec3i pred = GetPrediction();
		encoded = ClampVec(pred + DecodeErr(encoded));
		Update(encoded);
		out = Dequantize(encoded);
		return true;
	}
	ILINE Vec3 DecodeQuantized(Vec3i encoded) const
	{
		Update(encoded);
		return Dequantize(encoded);
	}
	ILINE Vec3i ClampVec(const Vec3i& v) const
	{
		Vec3i out;
		for (int i = 0; i < 3; i++)
			out[i] = CLAMP(v[i], 0, (int)m_quantizer.GetMaxQuantizedValue());
		return out;
	}

private:
	CQuantizer    m_quantizer;

	mutable uint8 m_nValues;
	mutable Vec3i m_quantized[3];

	ILINE Vec3 Dequantize(const Vec3i& p) const
	{
		Vec3 t;
		for (int i = 0; i < 3; i++)
			t[i] = m_quantizer.Dequantize(p[i]);
		return t;
	}
	ILINE Vec3i Quantize(const Vec3& v) const
	{
		Vec3i p;
		for (int i = 0; i < 3; i++)
			p[i] = m_quantizer.Quantize(v[i]);
		return p;
	}

	ILINE Vec3i GetPrediction() const
	{
		return m_quantized[1] + m_quantized[2];
	}

	ILINE Vec3i EncodeErr(const Vec3i& err) const
	{
	#ifdef INTEGER_ERROR_ENCODE_MATH
		Vec3i dir = m_quantized[2];
		int64 approx_length = abs(dir.x) > abs(dir.y) ? abs(dir.x) : abs(dir.y);
		// make sure that the product of the encode and decode approximation is the length squared
		approx_length = ((int64(dir.x) * dir.x + int64(dir.y) * dir.y) << INTEGER_ERROR_ENCODE_EXTRA_BITS) / approx_length;

		Vec3i err2;
		err2.x = (int)(((int64(dir.x) * err.x + int64(dir.y) * err.y) << INTEGER_ERROR_ENCODE_EXTRA_BITS) / approx_length);
		err2.y = (int)(((-int64(dir.y) * err.x + int64(dir.x) * err.y) << INTEGER_ERROR_ENCODE_EXTRA_BITS) / approx_length);
		err2.z = err.z;
		return err2;
	#else
		Vec3d dir = m_quantized[2];
		double cosAng = dir.x / dir.GetLength2D();
		double sinAng = dir.y / dir.GetLength2D();
		Vec3d err2;
		err2.x = cosAng * err.x + sinAng * err.y;
		err2.y = -sinAng * err.x + cosAng * err.y;
		err2.z = err.z;
		return err2;
	#endif
	}

	ILINE Vec3i DecodeErr(const Vec3i& err) const
	{
	#ifdef INTEGER_ERROR_ENCODE_MATH
		Vec3i dir = m_quantized[2];
		int approx_length = abs(dir.x) > abs(dir.y) ? abs(dir.x) : abs(dir.y);

		Vec3i err2;
		err2.x = (int)((int64(dir.x) * err.x - int64(dir.y) * err.y) / approx_length);
		err2.y = (int)((int64(dir.y) * err.x + int64(dir.x) * err.y) / approx_length);
		err2.z = err.z;
		return err2;
	#else
		Vec3d dir = m_quantized[2];
		double cosAng = dir.x / dir.GetLength2D();
		double sinAng = dir.y / dir.GetLength2D();
		Vec3d err2;
		err2.x = cosAng * err.x - sinAng * err.y;
		err2.y = sinAng * err.x + cosAng * err.y;
		err2.z = err.z;
		return err2;
	#endif
	}

	ILINE void Update(const Vec3i& encoded) const
	{
		switch (m_nValues)
		{
		case 0:
			m_quantized[0] = encoded;
			m_nValues++;
			break;
		case 1:
			m_quantized[1] = encoded;
			if (!(m_quantized[1].x == m_quantized[0].x && m_quantized[1].y == m_quantized[0].y))
			{
				m_quantized[2] = m_quantized[1] - m_quantized[0];
				m_nValues++;
			}
			break;
		case 2:
			m_quantized[0] = m_quantized[1];
			m_quantized[1] = encoded;
			if (!(m_quantized[1].x == m_quantized[0].x && m_quantized[1].y == m_quantized[0].y))
				m_quantized[2] = m_quantized[1] - m_quantized[0];
			break;
		default:
			NET_ASSERT(!"should never happen");
		}
	}
};

typedef CVec3TablePolicy<CDirectionalPredictor> TTableDirVec3;

}

	#include "CompressionManager.h"
void TestTableDirVec3()
{
	/*
	   CNetwork::Get()->GetCompressionManager().Reset(true);
	   CDefaultStreamAllocator alc;
	   CCommOutputStream stm(&alc, 1024);
	   ICompressionPolicyPtr pol = CNetwork::Get()->GetCompressionManager().GetCompressionPolicy('wrld');
	   pol->NoMemento( false );

	   std::vector<Vec3> saved;
	   static const int N = 10000;
	   for (int i=0; i<N; i++)
	   {
	    Vec3 r;
	    float ang = cry_random(0.0f, gf_PI*2);
	    float rad = expf(cry_random(0.0f, 15.0836251125665f)) * 0.0000719793139544965f - 0.0000719793139544965f;
	    r.x = cosf(ang)*rad + 1000.0f;
	    r.y = sinf(ang)*rad + 1000.0f;
	    r.z = 100.0f + cry_random(0.0f, 0.2f);
	    saved.push_back(r);
	    pol->WriteValue( stm, r, 0, 0, false );
	   }
	   stm.Flush();

	   CCommInputStream in(stm.GetPtr(),stm.GetOutputSize());
	   pol->NoMemento( false );
	   for (int i=0; i<N; i++)
	   {
	    Vec3 p;
	    pol->ReadValue( in, p, NULL, 0, false );
	    NET_ASSERT(p.GetDistance(saved[i]) < 0.01f);
	   }
	 */
}

#else

class CVec3TablePolicy
{
public:
	CVec3TablePolicy()
	{
		m_quantizerBits = 0;
	}

	ILINE bool Load(XmlNodeRef node, const string& filename)
	{
		if (!m_quantizer.Load(node, "unknown", "Params", eFQM_RoundLeft, 24))
			return false;

		m_quantizerBits = m_quantizer.GetNumBits();

		return true;
	}

	ILINE bool ReadValue(CNetInputSerializeImpl* in, Vec3& value, uint32 age) const
	{
		Vec3i quantized;

		quantized[0] = in->ReadBits(m_quantizerBits);
		quantized[1] = in->ReadBits(m_quantizerBits);
		quantized[2] = in->ReadBits(m_quantizerBits);

		value[0] = m_quantizer.Dequantize(quantized[0]);
		value[1] = m_quantizer.Dequantize(quantized[1]);
		value[2] = m_quantizer.Dequantize(quantized[2]);

		NetLogPacketDebug("CVec3TablePolicy::ReadValue (%f, %f, %f) Min %f Max %f NumBits %d (%f)", value.x, value.y, value.z, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), in->GetBitSize());

		return true;
	}

	ILINE bool WriteValue(CNetOutputSerializeImpl* out, const Vec3& value, uint32 age) const
	{
		Vec3i quantized;

		quantized[0] = m_quantizer.Quantize(value[0]);
		quantized[1] = m_quantizer.Quantize(value[1]);
		quantized[2] = m_quantizer.Quantize(value[2]);

		out->WriteBits(quantized[0], m_quantizerBits);
		out->WriteBits(quantized[1], m_quantizerBits);
		out->WriteBits(quantized[2], m_quantizerBits);

		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("Vec3TablePolicy: not implemented for generic types");
		return false;
	}

	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("Vec3TablePolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CVec3TablePolicy");

		pSizer->Add(*this);
	}
	#if NET_PROFILE_ENABLE
	int GetBitCount(Vec3 value)
	{
		return m_quantizerBits * 3;
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
	#endif
private:
	CQuantizer m_quantizer;
	uint32     m_quantizerBits;
};

typedef CVec3TablePolicy TTableDirVec3;

#endif

REGISTER_COMPRESSION_POLICY(TTableDirVec3, "TableDirVec3");
