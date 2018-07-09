// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QuatQuantization.h"
#include "Controller.h"

enum EKeyTimesFormat 
{
	eF32 = 0,
	eUINT16,
	eByte,
	obsolete_eF32StartStop,
	obsolete_eUINT16StartStop,
	obsolete_eByteStartStop,
	eBitset
};


class IPositionController;
class IRotationController;
class IKeyTimesInformation;
class ITrackPositionStorage;
class ITrackRotationStorage;

namespace ControllerHelper
{
	ITrackPositionStorage* ConstructPositionController(ECompressionFormat format);
	ITrackRotationStorage* ConstructRotationController(ECompressionFormat format);
	ITrackPositionStorage* ConstructScaleController(ECompressionFormat format);
	IKeyTimesInformation* ConstructKeyTimesController(uint32);
}

class IKeyTimesInformation : public _reference_target_t
{

public:
	virtual ~IKeyTimesInformation() {}

	// return key value in f32 format
	virtual f32 GetKeyValueFloat(uint32 key) const  = 0;

	virtual void SetKeyValueFloat(uint32 key, float time)  = 0;

	// return key number for normalized_time
	virtual uint32	GetKey( f32 normalized_time, f32& difference_time) = 0;

	virtual uint32 GetNumKeys() const = 0;

	virtual uint32 GetDataCount() const = 0;

	virtual void AddKeyTime(f32 val) = 0;

	virtual void ReserveKeyTime(uint32 count) = 0;

	virtual void ResizeKeyTime(uint32 count) = 0;

	virtual uint32 GetFormat() = 0;

	virtual char * GetData() = 0;

	virtual uint32 GetDataRawSize() = 0;

	virtual void Clear() = 0;

	virtual void SwapBytes() = 0;
};


template <class TKeyTime, EKeyTimesFormat FormatID>
class CKeyTimesInformation  : public IKeyTimesInformation
{
public:

	CKeyTimesInformation() : m_lastTime(-FLT_MAX) {}

	// return key value
	TKeyTime GetKeyValue(uint32 key) const { return m_arrKeys[key]; };

	// return key value in f32 format
	f32 GetKeyValueFloat(uint32 key) const { return m_arrKeys[key]; }

	void SetKeyValueFloat(uint32 key, float f) {m_arrKeys[key] = (TKeyTime)f; }

	// return key number for normalized_time
	uint32	GetKey( f32 realtime, f32& difference_time)
	{
		if (realtime == m_lastTime)
		{
			difference_time =  m_LastDifferenceTime;
			return m_lastKey;
		}

		m_lastTime = realtime;


		uint32 numKey = m_arrKeys.size();

		TKeyTime keytime_start = m_arrKeys[0];
		TKeyTime keytime_end = m_arrKeys[numKey-1];

		f32 test_end = keytime_end;
		if( realtime < keytime_start )
			test_end += realtime;

		if( realtime < keytime_start )
		{
			m_lastKey = 0;
			return 0;
		}

		if( realtime >= keytime_end )
		{
			m_lastKey = numKey;
			return numKey;
		}


		int nPos  = numKey>>1;
		int nStep = numKey>>2;

		// use binary search
		while(nStep)
		{
			if(realtime < m_arrKeys[nPos])
				nPos = nPos - nStep;
			else
				if(realtime > m_arrKeys[nPos])
					nPos = nPos + nStep;
				else 
					break;

			nStep = nStep>>1;
		}

		// fine-tuning needed since time is not linear
		while(realtime > m_arrKeys[nPos])
			nPos++;

		while(realtime < m_arrKeys[nPos-1])
			nPos--;

		m_lastKey = nPos;

		m_LastDifferenceTime = difference_time = (f32)(realtime-(f32)m_arrKeys[nPos-1]) / ((f32)m_arrKeys[nPos] - (f32)m_arrKeys[nPos-1]);

		return nPos;
	}

	uint32 GetNumKeys() const {return m_arrKeys.size(); };

	virtual uint32 GetDataCount() const {return m_arrKeys.size(); };

	virtual void Clear()
	{
		m_arrKeys.clear();
	}

	void AddKeyTime(f32 val)
	{
		m_arrKeys.push_back((TKeyTime)(val));
	}

	void ReserveKeyTime(uint32 count)
	{

		m_arrKeys.reserve(count);
	}

	void ResizeKeyTime(uint32 count)
	{

		m_arrKeys.resize(count);
	}

	uint32 GetFormat()
	{
		return FormatID;
	}

	char * GetData()
	{
		return (char*) m_arrKeys.begin();
	}

	uint32 GetDataRawSize()
	{
		return (uint32)((char*)m_arrKeys.end() - (char*)m_arrKeys.begin());
	}

	virtual void SwapBytes()
	{
		SwapEndianness(&m_arrKeys[0], m_arrKeys.size());
	}

private:
	DynArray<TKeyTime> m_arrKeys;
	f32 m_lastTime;
	f32 m_LastDifferenceTime;
	uint32 m_lastKey;
};

typedef _smart_ptr<IKeyTimesInformation> KeyTimesInformationPtr;

typedef CKeyTimesInformation<f32, eF32> F32KeyTimesInformation;
typedef CKeyTimesInformation<uint16, eUINT16> UINT16KeyTimesInformation;
typedef CKeyTimesInformation<uint8, eByte> ByteKeyTimesInformation;


class ITrackPositionStorage : public _reference_target_t
{
public:
	virtual void AddValue(const Vec3& val) = 0;
	virtual void SetValueForKey(uint32 key, const Vec3& val) = 0;
	virtual char * GetData() = 0;
	virtual uint32 GetDataRawSize() const = 0;
	virtual void Resize(uint32 count) = 0;
	virtual void Reserve(uint32 count) = 0;
	virtual uint32 GetFormat() const = 0;
	virtual Vec3 GetValue(uint32 key) const = 0;
	virtual uint32 GetNumKeys() const = 0;
	virtual uint32 GetDataCount() const = 0;
	virtual void SwapBytes() = 0;

};

typedef _smart_ptr<ITrackPositionStorage> TrackPositionStoragePtr;


class ITrackRotationStorage : public _reference_target_t
{
public:
	virtual void AddValue(const Quat& val) = 0;
	virtual void SetValueForKey(uint32 key, const Quat& val) = 0;
	virtual char * GetData() = 0;
	virtual uint32 GetDataRawSize() const = 0;
	virtual void Resize(uint32 count) = 0;
	virtual void Reserve(uint32 count) = 0;
	virtual uint32 GetFormat() const = 0;
	virtual Quat GetValue(uint32 key) const = 0;
	virtual uint32 GetNumKeys() const = 0;
	virtual uint32 GetDataCount() const = 0;
	virtual void SwapBytes() = 0;

};

typedef _smart_ptr<ITrackPositionStorage> TrackPositionStoragePtr;
typedef _smart_ptr<ITrackRotationStorage> TrackRotationStoragePtr;


template<class _Type, class _Storage, class _Base>
class CTrackDataStorage : public _Base
{
public:

	virtual void Resize(uint32 count) override
	{
		m_arrKeys.resize(count);
	}

	virtual void Reserve(uint32 count) override
	{
		m_arrKeys.reserve(count);
	}

	virtual void AddValue(const _Type& val) override
	{
		_Storage v;
		v.ToInternalType(val);
		m_arrKeys.push_back(v);
	}

	virtual char* GetData() override
	{
		return (char*) m_arrKeys.begin();
	}

	virtual uint32 GetNumKeys() const override
	{
		return m_arrKeys.size();
	}

	virtual uint32 GetDataCount() const override
	{
		return m_arrKeys.size();
	}

	virtual uint32 GetDataRawSize() const override
	{
		return (uint32)((char*)m_arrKeys.end() - (char*)m_arrKeys.begin());
	}

	virtual _Type GetValue(uint32 key) const override
	{
		return m_arrKeys[key].ToExternalType();
	}

	virtual void SetValueForKey(uint32 key, const _Type& val) override
	{
		uint32 numKeys = m_arrKeys.size();

		if (key >= numKeys)
		{
			return; 
		}
		_Storage v;
		v.ToInternalType(val);

		m_arrKeys[key] = v;
	}

	virtual uint32 GetFormat() const override
	{
		return _Storage::GetFormat();
	}

	virtual void SwapBytes() override
	{
		for (uint32 i =0; i < m_arrKeys.size(); ++i)
			m_arrKeys[i].SwapBytes();

	}

	DynArray<_Storage> m_arrKeys;
};

typedef CTrackDataStorage<Vec3, NoCompressVec3, ITrackPositionStorage> NoCompressPosition;
typedef _smart_ptr<NoCompressPosition> NoCompressPositionPtr;

typedef CTrackDataStorage<Quat, NoCompressQuat, ITrackRotationStorage> NoCompressRotation;
typedef _smart_ptr<NoCompressRotation> NoCompressRotationPtr;

typedef CTrackDataStorage<Quat, SmallTree48BitQuat, ITrackRotationStorage> SmallTree48BitQuatRotation;
typedef _smart_ptr<SmallTree48BitQuatRotation> SmallTree48BitQuatRotationPtr;

typedef CTrackDataStorage<Quat, SmallTree64BitQuat, ITrackRotationStorage> SmallTree64BitQuatRotation;
typedef _smart_ptr<SmallTree64BitQuatRotation> SmallTree64BitQuatRotationPtr;

typedef CTrackDataStorage<Quat, SmallTree64BitExtQuat, ITrackRotationStorage> SmallTree64BitExtQuatRotation;
typedef _smart_ptr<SmallTree64BitExtQuatRotation> SmallTree64BitExtQuatRotationPtr;


class ITrackInformation  : public _reference_target_t
{
public:
	uint32 GetNumKeys() const
	{
		return m_pKeyTimes->GetNumKeys();
	}

	f32 GetTimeFromKey(uint32 key) const
	{
		return m_pKeyTimes->GetKeyValueFloat(key);
	}

	void SetKeyTimesInformation(KeyTimesInformationPtr& ptr)
	{
		m_pKeyTimes = ptr;
	}

	KeyTimesInformationPtr& GetKeyTimesInformation()
	{
		return m_pKeyTimes;
	}
protected:
	KeyTimesInformationPtr m_pKeyTimes;

};

class IPositionController : public ITrackInformation // TODO: Should be renamed to Vec3 controller.
{
public:
	virtual ~IPositionController() {}

	virtual Vec3 DecodeKey(f32 normalized_time) = 0;

	virtual size_t SizeOfThis() const
	{
		return sizeof(this) + m_pData->GetDataRawSize();
	}

	virtual Vec3 GetValueFromKey(uint32 key) const
	{
		uint32 numKeys = GetNumKeys();

		if (key >= numKeys)
		{
			key = numKeys - 1;
		}

		return m_pData->GetValue(key);
	}

	virtual void SetPositionStorage(TrackPositionStoragePtr& ptr)
	{
		m_pData = ptr;
	}

	virtual TrackPositionStoragePtr& GetPositionStorage()
	{
		return m_pData;
	}

	virtual uint32 GetFormat() const
	{
		return m_pData->GetFormat();
	}

	virtual uint32 GetDataCount() const
	{
		return m_pData->GetNumKeys();
	}

	virtual void SwapBytes()
	{
		m_pData->SwapBytes();
	}

protected:
	TrackPositionStoragePtr m_pData;
};

class IRotationController  : public ITrackInformation // TODO: Should be renamed to Quat controller.
{
public:

	virtual ~IRotationController() {};

	virtual Quat DecodeKey(f32 normalized_time) = 0;

	virtual size_t SizeOfThis() const
	{
		return sizeof(this) + m_pData->GetDataRawSize();
	}

	virtual Quat GetValueFromKey(uint32 key)	const
	{
		uint32 numKeys = GetNumKeys();

		if (key >= numKeys)
		{
			key = numKeys - 1;
		}

		return m_pData->GetValue(key);
	}

	virtual void SetRotationStorage(TrackRotationStoragePtr& ptr)
	{
		m_pData = ptr;
	}
	virtual TrackRotationStoragePtr& GetRotationStorage()
	{
		return m_pData;
	}

	virtual uint32 GetFormat() const
	{
		return m_pData->GetFormat();
	}

	virtual uint32 GetDataCount() const
	{
		return m_pData->GetNumKeys();
	}

	virtual void SwapBytes()
	{
		m_pData->SwapBytes();
	}

protected:
	TrackRotationStoragePtr m_pData;
};

typedef _smart_ptr<IRotationController> RotationControllerPtr;
typedef _smart_ptr<IPositionController> PositionControllerPtr;


struct Vec3Lerp
{	
	static inline Vec3 Blend(const Vec3& val1, const Vec3& val2, f32 t)
	{
		Vec3 nrvo;
		nrvo.SetLerp(val1, val2, t);
		return nrvo;
	}
};

struct QuatLerp
{
	static inline Quat Blend(const Quat& val1, const Quat& val2, f32 t)
	{
		Quat nrvo;
		nrvo.SetNlerp(val1, val2, t);
		return nrvo;
	}
};

template<class _Type, class _Interpolator, class _Base>
class CAdvancedTrackInformation : public _Base
{
public:

	CAdvancedTrackInformation() 
	{
	}

	virtual _Type DecodeKey(f32 normalized_time) override
	{
		f32 t;
		uint32 key = m_pKeyTimes->GetKey(normalized_time, t);

		if (key == 0)
		{
			return GetValueFromKey(0);
		}
		else if (key >= GetNumKeys())
		{
			return GetValueFromKey(key - 1);
		}
		else
		{
			const _Type p1 = GetValueFromKey(key - 1);
			const _Type p2 = GetValueFromKey(key);
			return _Interpolator::Blend(p1, p2, t);
		}	
	}
};

typedef CAdvancedTrackInformation<Quat, QuatLerp, IRotationController> RotationTrackInformation;
typedef _smart_ptr<RotationTrackInformation> RotationTrackInformationPtr;

typedef CAdvancedTrackInformation<Vec3, Vec3Lerp, IPositionController> PositionTrackInformation;
typedef _smart_ptr<PositionTrackInformation> PositionTrackInformationPtr;


class CController : public IController
{
public:

	virtual const CController* GetCController() const { return this; };

	uint32 numKeys() const
	{
		// now its hack, because num keys might be different
		if (m_pRotationController)
			return m_pRotationController->GetNumKeys();

		if (m_pPositionController)
			return m_pPositionController->GetNumKeys();

		return 0;
	}

	uint32 GetID () const {return m_nControllerId;}

	Status4 GetOPS(f32 normalized_time, Quat& quat, Vec3& pos, Diag33& scale)
	{
		Status4 res;
		res.o = GetO( normalized_time, quat) ? 1 : 0;
		res.p = GetP( normalized_time, pos) ? 1 : 0;
		res.s = GetS( normalized_time, scale) ? 1: 0;

		return res; 
	}

	Status4 GetOP(f32 normalized_time, Quat& quat, Vec3& pos)
	{
		Status4 res;
		res.o = GetO( normalized_time, quat) ? 1 : 0;
		res.p = GetP( normalized_time, pos) ? 1 : 0;

		return res; 
	}

	uint32 GetO(f32 normalized_time, Quat& quat)
	{
		if (m_pRotationController)
		{
			quat = m_pRotationController->DecodeKey(normalized_time);
			return STATUS_O;
		}

		return 0;
	}

	uint32 GetP(f32 normalized_time, Vec3& pos)
	{
		if (m_pPositionController)
		{
			pos = m_pPositionController->DecodeKey(normalized_time);
			return STATUS_P;
		}

		return 0;
	}

	uint32 GetS(f32 normalized_time, Diag33& pos)
	{
		if (m_pScaleController)
		{
			const Vec3 scale = m_pScaleController->DecodeKey(normalized_time);

			pos.x = scale.x;
			pos.y = scale.y;
			pos.z = scale.z;

			return STATUS_S;
		}
		return 0;
	}

	virtual int32 GetO_numKey() override
	{
		if (m_pRotationController)
			return m_pRotationController->GetNumKeys();
		return -1;
	}

	virtual int32 GetP_numKey() override
	{
		if (m_pPositionController)
			return m_pPositionController->GetNumKeys();
		return -1;
	}

	virtual int32 GetS_numKey() override
	{
		if (m_pScaleController)
			return m_pScaleController->GetNumKeys();
		return -1;
	}

	size_t SizeOfThis() const
	{
		size_t res(sizeof(CController));

		if (m_pPositionController)
			res += m_pPositionController->SizeOfThis();

		if (m_pScaleController)
			res += m_pScaleController->SizeOfThis();

		if (m_pRotationController)
			res += m_pRotationController->SizeOfThis();

		return  res;
	}

	CInfo GetControllerInfo() const
	{
		CInfo info;
		info.etime = 0;
		info.stime = 0;
		info.numKeys = 0;

		if (m_pRotationController)
		{
			info.numKeys = m_pRotationController->GetNumKeys();
			info.quat = m_pRotationController->GetValueFromKey(info.numKeys - 1);
			info.etime	 = (int)m_pRotationController->GetTimeFromKey(info.numKeys - 1);
			info.stime	 = (int)m_pRotationController->GetTimeFromKey(0);
		}

		if (m_pPositionController)
		{
			info.numKeys = m_pPositionController->GetNumKeys();
			info.pos = m_pPositionController->GetValueFromKey(info.numKeys - 1);
			info.etime	 = (int)m_pPositionController->GetTimeFromKey(info.numKeys - 1);
			info.stime	 = (int)m_pPositionController->GetTimeFromKey(0);
		}


		info.realkeys = (info.etime - info.stime) + 1;

		return info;
	}

	void SetID(uint32 id)
	{
		m_nControllerId = id;
	}

	void SetRotationController(RotationControllerPtr& ptr)
	{
		m_pRotationController = ptr;
	}

	void SetPositionController(PositionControllerPtr& ptr)
	{
		m_pPositionController = ptr;
	}

	void SetScaleController(PositionControllerPtr& ptr)
	{
		m_pScaleController = ptr;
	}

	const RotationControllerPtr& GetRotationController() const
	{
		return m_pRotationController;
	}

	RotationControllerPtr& GetRotationController()
	{
		return m_pRotationController;
	}

	const PositionControllerPtr& GetPositionController() const
	{
		return m_pPositionController;
	}

	PositionControllerPtr& GetPositionController()
	{
		return m_pPositionController;
	}

	PositionControllerPtr& GetScaleController()
	{
		return m_pScaleController;
	}

	const PositionControllerPtr& GetScaleController() const
	{
		return m_pScaleController;
	}

	uint32 m_nPositionKeyTimesTrackId;
	uint32 m_nRotationKeyTimesTrackId;
	uint32 m_nPositionTrackId;
	uint32 m_nRotationTrackId;

	bool m_bShared;

protected:

	uint32 m_nControllerId;
	RotationControllerPtr m_pRotationController;
	PositionControllerPtr m_pPositionController;
	PositionControllerPtr m_pScaleController;
};

TYPEDEF_AUTOPTR(CController);


inline uint16 GetFirstLowBit(uint16 word)
{
	uint16 c(0);

	if (word & 1)
		return 0;

	uint16 b;
	do 
	{
		b = (word >> 1) & 1;
		++c;
	} while(!b && c < 16);

	return c; 
}

inline uint16 GetFirstHighBit(uint16 word)
{
	uint16 c(0);

	if (word & 1)
		return 0;

	uint16 b;
	do 
	{
		b = (word << 1) & 0x8000;
		++c;
	} while(!b && c < 16);

	return c; 
}


class CKeyTimesInformationBitSet  : public IKeyTimesInformation
{
public:

	CKeyTimesInformationBitSet()
		: m_lastTime(-1)
	{
	}

	virtual void SetKeyValueFloat(uint32 key, float time) {}

	// return key value in f32 format
	f32 GetKeyValueFloat(uint32 key) const 
	{ 
		if (key == 0)
		{
			return (float)GetHeader()->m_Start;
		}
		if (key == GetNumKeys() - 1)
		{
			return (float)GetHeader()->m_End;
		}

		int keys = GetNumKeys();
		int count(0);
		for (int i = 0; i < keys; ++i)
		{
			uint16 bits = GetKeyData(i);

			for (int j = 0; j < 16; ++j)
			{
				if ((bits >> j) & 1)
				{
					++count;
					if ((count - 1) == key)
						return (float)(i * 16 + j) + GetHeader()->m_Start;
				}
			}
		}

		return 0;
	}

	// return key number for normalized_time
	uint32	GetKey( f32 realtime, f32& difference_time)
	{
		if (realtime == m_lastTime)
		{
			difference_time =  m_LastDifferenceTime;
			return m_lastKey;
		}
		m_lastTime = realtime;
		uint32 numKey = m_arrKeys.size();

		f32 keytime_start = (float)GetHeader()->m_Start;
		f32 keytime_end = (float)GetHeader()->m_End;
		f32 test_end = keytime_end;

		if( realtime < keytime_start )
			test_end += realtime;

		if( realtime < keytime_start )
		{
			m_lastKey = 0;
			return 0;
		}

		if( realtime >= keytime_end )
		{
			m_lastKey = numKey;
			return numKey;
		}

		f32 internalTime = realtime - keytime_start;
		uint16 uTime = (uint16)internalTime;
		uint16 piece = (uTime / sizeof(uint16)) >> 3;
		uint16 bit = uTime % 16;
		uint16 data = GetKeyData(piece);

		uint16 left = data & (0xFFFF >> (15 - bit));
		uint16 leftPiece(piece);
		uint16 nearestLeft=0;
		uint16 wBit;
		while ((wBit = GetFirstHighBit(left)) == 16)
		{
			--leftPiece;
			left = GetKeyData(leftPiece);
		}
		nearestLeft = leftPiece * 16 + wBit;

		uint16 right = ((data >> (bit + 1)) & 0xFFFF) << (bit + 1);
		uint16 rigthPiece(piece);
		uint16 nearestRight=0;
		while ((wBit = GetFirstLowBit(right)) == 16)
		{
			++rigthPiece;
			right = GetKeyData(rigthPiece);
		}

		nearestRight = ((rigthPiece  * sizeof(uint16)) << 3) + wBit;
		m_LastDifferenceTime = difference_time = (f32)(internalTime-(f32)nearestLeft) / ((f32)nearestRight - (f32)nearestLeft);

		uint32 nPos(0);
		for (uint16 i = 0; i < rigthPiece; ++i)
		{
			uint16 data = GetKeyData(i);
			nPos += m_bTable[data & 255] + m_bTable[data >> 8];
		}

		data = GetKeyData(rigthPiece);
		data = ((data <<  (15 - wBit)) & 0xFFFF) >> (15 - wBit);
		nPos += m_bTable[data & 255] + m_bTable[data >> 8];

		m_lastKey = nPos - 1;

		return m_lastKey;
	}

	uint32 GetNumKeys() const {return GetHeader()->m_Size; }
	virtual uint32 GetDataCount() const { return m_arrKeys.size(); }


	void AddKeyTime(f32 val)
	{
	}

	void ReserveKeyTime(uint32 count)
	{
		m_arrKeys.reserve(count);
	}

	void ResizeKeyTime(uint32 count)
	{
		m_arrKeys.resize(count);
	}

	uint32 GetFormat()
	{
		return eBitset;
	}

	char * GetData()
	{
		return (char*)m_arrKeys.begin();
	}

	uint32 GetDataRawSize()
	{
		return (uint32)((char*)m_arrKeys.end() - (char*)m_arrKeys.begin());
	}

	virtual void Clear()
	{

	}

	virtual void SwapBytes()
	{
		SwapEndianness(&m_arrKeys[0], m_arrKeys.size());
	}

protected:

	struct Header 
	{
		uint16		m_Start;
		uint16    m_End;
		uint16		m_Size;
	};

	inline Header* GetHeader() const
	{
		return (Header*)(&m_arrKeys[0]);
	};

	inline uint16 GetKeyData(int i) const
	{
		return m_arrKeys[3 + i];
	};

	DynArray<uint16> m_arrKeys;

	f32 m_lastTime;
	f32 m_LastDifferenceTime;
	uint32 m_lastKey;

private:
	static uint8 m_bTable[256];
};
