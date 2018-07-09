// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryMemory/CrySizer.h>
#include <CrySerialization/Enum.h>

// For serialization
class XmlNodeRef;

//! \deprecated {These flags are deprecated. New code should prefer the spline::Flags structure.}
enum ESplineKeyTangentType
{
	SPLINE_KEY_TANGENT_NONE   = 0,
	SPLINE_KEY_TANGENT_CUSTOM = 1,
	SPLINE_KEY_TANGENT_ZERO   = 2,
	SPLINE_KEY_TANGENT_STEP   = 3,
	SPLINE_KEY_TANGENT_LINEAR = 4
};

#define SPLINE_KEY_TANGENT_IN_SHIFT    (0)
#define SPLINE_KEY_TANGENT_IN_MASK     (0x07)                                // 0000111
#define SPLINE_KEY_TANGENT_OUT_SHIFT   (3)
#define SPLINE_KEY_TANGENT_OUT_MASK    (0x07 << (SPLINE_KEY_TANGENT_OUT_SHIFT)) // 0111000
#define SPLINE_KEY_TANGENT_UNIFY_SHIFT (6)
#define SPLINE_KEY_TANGENT_UNIFY_MASK  (0x01 << (SPLINE_KEY_TANGENT_UNIFY_SHIFT)) // 1000000

#define SPLINE_KEY_TANGENT_ALL_MASK    (SPLINE_KEY_TANGENT_IN_MASK | SPLINE_KEY_TANGENT_OUT_MASK | SPLINE_KEY_TANGENT_UNIFY_MASK)
#define SPLINE_KEY_TANGENT_UNIFIED     ((SPLINE_KEY_TANGENT_CUSTOM << SPLINE_KEY_TANGENT_IN_SHIFT) \
  | (SPLINE_KEY_TANGENT_CUSTOM << SPLINE_KEY_TANGENT_OUT_SHIFT)                                    \
  | (0x01 << SPLINE_KEY_TANGENT_UNIFY_SHIFT))
#define SPLINE_KEY_TANGENT_BROKEN      ((SPLINE_KEY_TANGENT_CUSTOM << SPLINE_KEY_TANGENT_IN_SHIFT) \
  | (SPLINE_KEY_TANGENT_CUSTOM << SPLINE_KEY_TANGENT_OUT_SHIFT)                                    \
  | (0x00 << SPLINE_KEY_TANGENT_UNIFY_SHIFT))

enum ESplineKeyFlags
{
	ESPLINE_KEY_UI_SELECTED_SHIFT               = 16,
	ESPLINE_KEY_UI_SELECTED_MAX_DIMENSION_COUNT = 4,   //!< Should be power of 2 (see ESPLINE_KEY_UI_SELECTED_MASK).
	ESPLINE_KEY_UI_SELECTED_MASK                = ((1 << ESPLINE_KEY_UI_SELECTED_MAX_DIMENSION_COUNT) - 1) << ESPLINE_KEY_UI_SELECTED_SHIFT
};

//! Return value closest to 0 if same sign, or 0 if opposite.
template<class T>
inline T minmag(T const& a, T const& b)
{
	if (a * b <= T(0.f))
		return T(0.f);
	else if (a < T(0.f))
		return max(a, b);
	else
		return min(a, b);
}

template<class T>
inline Vec3_tpl<T> minmag(Vec3_tpl<T> const& a, Vec3_tpl<T> const& b)
{
	return Vec3_tpl<T>(minmag(a.x, b.x), minmag(a.y, b.y), minmag(a.z, b.z));
}

//! Interface returned by backup methods of ISplineInterpolator.
struct ISplineBackup
{
	// <interfuscator:shuffle>
	virtual ~ISplineBackup() {}
	virtual void                     AddRef() = 0;
	virtual void                     Release() = 0;
	virtual struct ISplineEvaluator* GetSpline() = 0;
	// </interfuscator:shuffle>
};

namespace spline
{
// Special function that makes parameter zero.
template<class T> void Zero(T& val) { ZeroStruct(val); }

/****************************************************************************
**                            Key classes																	 **
****************************************************************************/
SERIALIZATION_ENUM_DECLARE(ETangentType, : uint8,
                           Smooth,
                           Custom,
                           Zero,
                           Step,
                           Linear
                           )

struct Flags          //!< Tangent flags.
{
	ETangentType inTangentType;
	ETangentType outTangentType;
	bool         tangentsUnified;
	uint8        selectedMask;

	//! Convert bitfields to/and from flag int, for code and serialization compatibility
	Flags(int i = 0)
	{
		inTangentType = ETangentType((i & SPLINE_KEY_TANGENT_IN_MASK) >> SPLINE_KEY_TANGENT_IN_SHIFT);
		outTangentType = ETangentType((i & SPLINE_KEY_TANGENT_OUT_MASK) >> SPLINE_KEY_TANGENT_OUT_SHIFT);
		tangentsUnified = (i & SPLINE_KEY_TANGENT_UNIFY_MASK) != 0;
		selectedMask = (i & ESPLINE_KEY_UI_SELECTED_MASK) >> ESPLINE_KEY_UI_SELECTED_SHIFT;
	}
	operator int() const
	{
		return ((int)inTangentType << SPLINE_KEY_TANGENT_IN_SHIFT)
		       | ((int)outTangentType << SPLINE_KEY_TANGENT_OUT_SHIFT)
		       | (tangentsUnified << SPLINE_KEY_TANGENT_UNIFY_SHIFT)
		       | (selectedMask << ESPLINE_KEY_UI_SELECTED_SHIFT);
	}
};

template<class T>
struct  SplineKey
{
	typedef T value_type;

	float      time;      //!< Key time.
	Flags      flags;     //!< Key flags.
	value_type value;     //!< Key value.
	value_type ds;        //!< Incoming tangent.
	value_type dd;        //!< Outgoing tangent.

	SplineKey() { ZeroStruct(*this); }

	bool operator==(const SplineKey<T>& k2) const { return time == k2.time; };
	bool operator!=(const SplineKey<T>& k2) const { return time != k2.time; };
	bool operator<(const SplineKey<T>& k2) const  { return time < k2.time; };
	bool operator>(const SplineKey<T>& k2) const  { return time > k2.time; };

	// Interpolate value between two keys.
	void interpolate(const SplineKey& key2, float tr, T& val) const
	{
		const float t = tr, u = 1.f - t, tt = t * t, uu = u * u, tu = u * t;
		const float cv0 = u * (1 + tu - tt),
		            cv1 = t * (1 + tu - uu),
		            cs0 = u * tu,
		            cs1 = -t * tu;
		val = value * cv0 + key2.value * cv1 + dd * cs0 + key2.ds * cs1;
	}

	static int tangent_passes() { return 1; }

	// Compute the in and out tangents if needed. 
	// Default implementation changes tangents only when flags = Zero, Step, or Linear.
	void compute_tangents(const SplineKey* prev = nullptr, const SplineKey* next = nullptr, int pass = 0, int num_keys = 0)
	{
		if (pass == 0)
		{
			// Set initial tangents based on flags
			if (!prev)
			{
				Zero(ds);
				flags.inTangentType = ETangentType::Smooth;
			}
			else
			{
				switch (flags.inTangentType)
				{
				case ETangentType::Step:
				case ETangentType::Zero:
					Zero(ds);
					break;
				case ETangentType::Linear:
					ds = value - prev->value;
					break;
				}
			}

			if (!next)
			{
				Zero(dd);
				flags.outTangentType = ETangentType::Smooth;
			}
			else
			{
				switch (flags.outTangentType)
				{
				case ETangentType::Step:
				case ETangentType::Zero:
					Zero(dd);
					break;
				case ETangentType::Linear:
					dd = next->value - value;
					break;
				}
			}
		}
	}
};

// Serialization separators
struct Formatting
{
	char key, field, component;

	Formatting(cstr in)
		: key(in[0]), field(in[1]), component(in[2])
	{
		assert(*in);
	}
};

struct Float4SplineKey : SplineKey<float[4]>
{
	static const int DIM = 4;
	typedef float value_type[DIM];

	void ToString(string& str, Formatting format) const;
	bool FromString(char* str, Formatting format);

	// Version which extracts a key from a string and updates the pointer
	bool ParseFromString(char*& str, Formatting format);

protected:

	static void ToString(const value_type& val, string& str, Formatting format);
	static bool FromString(value_type& val, char*& str, Formatting format);
};
}

//! Basic spline interface for structure, interpolation, and conversion.
struct ISplineEvaluator
{
	typedef float                   ElemType;
	typedef ElemType                ValueType[4];
	typedef spline::Float4SplineKey KeyType;

	static void ZeroValue(ValueType& value) { value[0] = 0; value[1] = 0; value[2] = 0; value[3] = 0; }

	// <interfuscator:shuffle>
	virtual ~ISplineEvaluator() {}

	// Basic spline structure and evaluation
	virtual int  GetNumDimensions() const { return 1; }
	virtual int  GetKeyCount() const = 0;
	virtual void GetKey(int i, KeyType& key) const = 0;
	virtual void Interpolate(float time, ValueType& value) = 0;

	// Conversion to other splines
	virtual ISplineBackup* Backup();
	virtual void           Restore(ISplineBackup* pBackup);

	virtual void           FromSpline(const ISplineEvaluator& source) = 0;
	virtual void           ToSpline(ISplineEvaluator& dest) const { dest.FromSpline(*this); }

	// String conversion
	// Default implementations utilize Formatting, may be overridden
	typedef spline::Formatting Formatting;

	virtual Formatting GetFormatting() const;
	virtual cstr       ToString() const; // Default implementation returns temporary string pointer, using static buffer
	virtual bool       FromString(cstr str);
	virtual bool       Serialize(yasli::Archive& ar, const char* name = "", const char* label = "");
	virtual void       SerializeSpline(XmlNodeRef& node, bool bLoading);
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////
// Aggregate multiple splines on the same domain.
// This is different than a multi-dimensional single spline.
//////////////////////////////////////////////////////////////////////////
struct IMultiSplineEvaluator
{
	virtual int               GetNumSplines() const = 0;
	virtual ISplineEvaluator* GetSpline(int i) = 0;
	virtual bool              Serialize(yasli::Archive& ar, const char* name = "", const char* label = "") = 0;
};

//! General interpolation and editing interface.
struct ISplineInterpolator : ISplineEvaluator
{
	// <interfuscator:shuffle>

	//! Insert a new key, returns index of the key.
	virtual int   InsertKey(const KeyType& key) = 0;
	virtual int   InsertKey(float time, ValueType value) = 0;
	virtual void  RemoveKey(int key) = 0;
	virtual void  Clear() = 0;

	virtual void  FindKeysInRange(float startTime, float endTime, int& firstFoundKey, int& numFoundKeys) = 0;
	virtual void  RemoveKeysInRange(float startTime, float endTime) = 0;

	virtual void  SetKeyTime(int key, float time) = 0;
	virtual float GetKeyTime(int key) = 0;
	virtual void  SetKeyValue(int key, ValueType value) = 0;
	virtual bool  GetKeyValue(int key, ValueType& value) = 0;

	virtual void  SetKeyInTangent(int key, ValueType tin) = 0;
	virtual void  SetKeyOutTangent(int key, ValueType tout) = 0;
	virtual void  SetKeyTangents(int key, ValueType tin, ValueType tout) = 0;
	virtual bool  GetKeyTangents(int key, ValueType& tin, ValueType& tout) = 0;

	//! Changes key flags.
	//! \see ESplineKeyFlags.
	virtual void SetKeyFlags(int key, int flags) = 0;

	//! Retrieve key flags.
	//! \see ESplineKeyFlags.
	virtual int GetKeyFlags(int key) = 0;
	// </interfuscator:shuffle>

	//////////////////////////////////////////////////////////////////////////
	// Helper functions.
	inline bool IsKeySelectedAtAnyDimension(const int key)
	{
		const int flags = GetKeyFlags(key);
		const int dimensionCount = GetNumDimensions();
		const int mask = ((1 << dimensionCount) - 1) << ESPLINE_KEY_UI_SELECTED_SHIFT;
		return (flags & mask) != 0;
	}

	inline bool IsKeySelectedAtDimension(const int key, const int dimension)
	{
		const int flags = GetKeyFlags(key);
		const int mask = 1 << (ESPLINE_KEY_UI_SELECTED_SHIFT + dimension);
		return (flags & mask) != 0;
	}

	void SelectKeyAllDimensions(int key, bool select)
	{
		const int flags = GetKeyFlags(key);
		if (select)
		{
			const int dimensionCount = GetNumDimensions();
			const int mask = ((1 << dimensionCount) - 1) << ESPLINE_KEY_UI_SELECTED_SHIFT;
			SetKeyFlags(key, (flags & (~ESPLINE_KEY_UI_SELECTED_MASK)) | mask);
		}
		else
		{
			SetKeyFlags(key, flags & (~ESPLINE_KEY_UI_SELECTED_MASK));
		}
	}

	void SelectKeyAtDimension(int key, int dimension, bool select)
	{
		const int flags = GetKeyFlags(key);
		const int mask = 1 << (ESPLINE_KEY_UI_SELECTED_SHIFT + dimension);
		SetKeyFlags(key, (select ? (flags | mask) : (flags & (~mask))));
	}

	inline int  InsertKeyFloat(float time, float val)      { ValueType v = { val, 0, 0, 0 }; return InsertKey(time, v); }
	inline int  InsertKeyFloat3(float time, float* vals)   { ValueType v = { vals[0], vals[1], vals[2], 0 }; return InsertKey(time, v); }
	inline bool GetKeyValueFloat(int key, float& value)    { ValueType v = { value }; bool b = GetKeyValue(key, v); value = v[0]; return b; }
	inline void SetKeyValueFloat(int key, float value)     { ValueType v = { value, 0, 0, 0 }; SetKeyValue(key, v); }
	inline void SetKeyValueFloat3(int key, float* vals)    { ValueType v = { vals[0], vals[1], vals[2], 0 }; SetKeyValue(key, v); }
	inline void InterpolateFloat(float time, float& val)   { ValueType v = { val }; Interpolate(time, v); val = v[0]; }
	inline void InterpolateFloat3(float time, float* vals) { ValueType v = { vals[0], vals[1], vals[2] }; Interpolate(time, v); vals[0] = v[0]; vals[1] = v[1]; vals[2] = v[2]; }

	//! Return Key closest to the specified time.
	inline int FindKey(float fTime, float fEpsilon = 0.01f)
	{
		int nKey = -1;
		// Find key.
		for (int k = 0; k < GetKeyCount(); k++)
		{
			if (fabs(GetKeyTime(k) - fTime) < fEpsilon)
			{
				nKey = k;
				break;
			}
		}
		return nKey;
	}

	//! Force an update.
	void Update() { ValueType val; Interpolate(0.f, val); }
};

namespace spline
{
//! General Spline class
template<class TKey>
class TSpline
{
public:
	typedef TKey key_type;
	using_type(TKey, value_type);

	//! Out of range types.
	enum
	{
		ORT_CONSTANT        = 0x0001,   //!< Constant track.
		ORT_CYCLE           = 0x0002,   //!< Cycle track.
		ORT_LOOP            = 0x0003,   //!< Loop track.
		ORT_OSCILLATE       = 0x0004,   //!< Oscillate track.
		ORT_LINEAR          = 0x0005,   //!< Linear track.
		ORT_RELATIVE_REPEAT = 0x0007    //!< Relative repeat track.
	};
	//! Spline flags.
	enum
	{
		MODIFIED  = 0x0001,     //!< Track modified.
		MUST_SORT = 0x0002,     //!< Track modified and must be sorted.
	};

	// Methods.
	inline TSpline()
	{
		m_flags = MODIFIED;
		m_ORT = 0;
		m_curr = 0;
		m_rangeStart = 0;
		m_rangeEnd = 0;
	}

	ILINE void  flag_set(int flag)               { m_flags |= flag; };
	ILINE void  flag_clr(int flag)               { m_flags &= ~flag; };
	ILINE int   flag(int flag)                   { return m_flags & flag; };

	ILINE void  ORT(int ort)                     { m_ORT = ort; };
	ILINE int   ORT() const                      { return m_ORT; };
	ILINE int   isORT(int o) const               { return (m_ORT == o); };

	ILINE void  SetRange(float start, float end) { m_rangeStart = start; m_rangeEnd = end; };
	ILINE float GetRangeStart() const            { return m_rangeStart; };
	ILINE float GetRangeEnd() const              { return m_rangeEnd; };

	// Keys access methods.
	ILINE void              reserve_keys(int n)            { m_keys.reserve(n); }; //!< Reserve memory for more keys.
	ILINE void              clear()                        { m_keys.clear(); };
	ILINE void              resize(int num)                { m_keys.resize(num); };        //!< Set new key count.
	ILINE bool              empty() const                  { return m_keys.empty(); };     //!< Check if curve empty (no keys).
	ILINE int               num_keys() const               { return (int)m_keys.size(); }; //!< Return number of keys in curve.

	ILINE key_type&         key(int n)                     { return m_keys[n]; };       //!< Return n key.
	ILINE float&            time(int n)                    { return m_keys[n].time; };  //!< Shortcut to key n time.
	ILINE value_type&       value(int n)                   { return m_keys[n].value; }; //!< Shortcut to key n value.
	ILINE value_type&       ds(int n)                      { return m_keys[n].ds; };    //!< Shortcut to key n incoming tangent.
	ILINE value_type&       dd(int n)                      { return m_keys[n].dd; };    //!< Shortcut to key n outgoing tangent.
	ILINE Flags&            flags(int n)                   { return m_keys[n].flags; }; //!< Shortcut to key n flags.

	ILINE key_type const&   key(int n) const               { return m_keys[n]; };       //!< Return n key.
	ILINE float             time(int n) const              { return m_keys[n].time; };  //!< Shortcut to key n time.
	ILINE value_type const& value(int n) const             { return m_keys[n].value; }; //!< Shortcut to key n value.
	ILINE value_type const& ds(int n) const                { return m_keys[n].ds; };    //!< Shortcut to key n incoming tangent.
	ILINE value_type const& dd(int n) const                { return m_keys[n].dd; };    //!< Shortcut to key n outgoing tangent.
	ILINE int               flags(int n) const             { return m_keys[n].flags; }; //!< Shortcut to key n flags.

	ILINE ETangentType      GetInTangentType(int n) const  { return m_keys[n].flags.inTangentType; }
	ILINE ETangentType      GetOutTangentType(int n) const { return m_keys[n].flags.outTangentType; }

	ILINE void              erase(int key)                 { m_keys.erase(m_keys.begin() + key); };
	ILINE bool              closed()                       { return (ORT() == ORT_LOOP); } //!< Return True if curve closed.

	ILINE void              SetModified(bool bOn, bool bSort = false)
	{
		if (bOn) m_flags |= MODIFIED; else m_flags &= ~(MODIFIED);
		if (bSort)
			m_flags |= MUST_SORT;
		m_curr = 0;
	}

	void sort_keys()
	{
		std::sort(m_keys.begin(), m_keys.end());
		m_flags &= ~MUST_SORT;
	}
	void comp_deriv()
	{
		key_type first, last;
		for (int pass = 0; pass < key_type::tangent_passes(); ++pass)
		{
			for (int i = 0; i < num_keys(); ++i)
			{
				const key_type* prev = nullptr;
				if (i > 0)
					prev = &m_keys[i - 1];
				else if (closed())
				{
					prev = &first;
					first = m_keys.back();
					first.time -= m_rangeEnd - m_rangeStart;
				}

				const key_type* next = nullptr;
				if (i < num_keys() - 1)
					next = &m_keys[i + 1];
				else if (closed())
				{
					next = &last;
					last = m_keys.front();
					last.time += m_rangeEnd - m_rangeStart;
				}

				m_keys[i].compute_tangents(prev, next, pass, num_keys());
			}
		}
	}

	int insert_key(const key_type& k)
	{
		int i;
		for (i = num_keys(); i > 0; i--)
		{
			if (m_keys[i - 1].time <= k.time)
				break;
		}

		m_keys.insert(i, k);
		return i;
	};

	inline void update()
	{
		if (m_flags)
		{
			if (m_flags & MUST_SORT)
				sort_keys();
			comp_deriv();
			m_flags = 0;
		}
	}

	inline bool is_updated() const
	{
		return m_flags == 0;
	}

	//! Interpolate the value along the spline.
	inline bool interpolate(float t, value_type& val)
	{
		update();

		if (empty())
		{
			return false;
		}

		if (t < time(0))
		{
			val = value(0);
			return true;
		}

		adjust_time(t);

		int curr = seek_key(t);
		if (curr < num_keys() - 1)
		{
			assert(t >= time(curr));
			if (GetOutTangentType(curr) == ETangentType::Step)
				val = value(curr + 1);
			else if (GetInTangentType(curr + 1) == ETangentType::Step)
				val = value(curr);
			else
			{
				float tr = (t - time(curr)) / (time(curr + 1) - time(curr));
				m_keys[curr].interpolate(m_keys[curr + 1], tr, val);
			}
		}
		else
		{
			val = value(num_keys() - 1);
		}
		return true;
	}

	inline value_type interpolate(float t)
	{
		value_type value;
		Zero(value);
		interpolate(t, value);
		return value;
	}

	size_t mem_size() const
	{
		return m_keys.capacity() * sizeof(m_keys[0]);
	}

	size_t sizeofThis() const
	{
		return sizeof(*this) + mem_size();
	}

	void swap(TSpline& b)
	{
		using std::swap;

		m_keys.swap(b.m_keys);
		swap(m_flags, b.m_flags);
		swap(m_ORT, b.m_ORT);
		swap(m_curr, b.m_curr);
		swap(m_rangeStart, b.m_rangeStart);
		swap(m_rangeEnd, b.m_rangeEnd);
	}

	//////////////////////////////////////////////////////////////////////////

	//! Interpolate value between two keys.

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddContainer(m_keys);
	}

protected:
	FastDynArray<key_type> m_keys;      //!< List of keys.
	uint8                  m_flags;
	uint8                  m_ORT;   //!< Out-Of-Range type.
	int16                  m_curr;  //!< Current key in track.

	float                  m_rangeStart;
	float                  m_rangeEnd;

	//! Return key before or equal to this time.
	inline int seek_key(float t)
	{
		assert(num_keys() < (1 << 15));
		if ((m_curr >= num_keys()) || (time(m_curr) > t))
			// Search from begining.
			m_curr = 0;
		while ((m_curr < num_keys() - 1) && (time(m_curr + 1) <= t))
			++m_curr;
		return m_curr;
	}

	inline void adjust_time(float& t)
	{
		if (isORT(ORT_CYCLE) || isORT(ORT_LOOP))
		{
			if (num_keys() > 0)
			{
				float endtime = time(num_keys() - 1);
				if (t > endtime)
				{
					// Warp time.
					t = fmod_tpl(t, endtime);
				}
			}
		}
	}

};

//! ClampedSpline is default implementation of slope computation
template<class T>
struct ClampedKey : public SplineKey<T>
{
	static int tangent_passes() { return 2; }

	void       compute_tangents(const ClampedKey* prev, const ClampedKey* next, int pass, int)
	{
		if (pass == 0)
		{
			// Compute slopes to ensure interpolation never goes outside end point range.
			// Initialise all slopes to continuous values.
			T slope = prev && next ? minmag(this->value - prev->value, next->value - this->value) : T(0);
			if (prev && this->flags.inTangentType != ETangentType::Custom)
				this->ds = slope;
			if (next && this->flags.outTangentType != ETangentType::Custom)
				this->dd = slope;
		}
		else
		{
			// Change discontinuous slopes.

			// Out slope.
			if (next && this->flags.outTangentType == ETangentType::Linear)
			{
				// Set linear between points.
				this->dd = next->value - this->value;
				if (next->flags.inTangentType != ETangentType::Linear)
					// Match continuous slope on right.
					this->dd = 2.0f * this->dd - next->ds;
			}

			// In slopes.
			if (prev && this->flags.inTangentType == ETangentType::Linear)
			{
				// Set linear between points.
				this->ds = this->value - prev->value;
				if (prev->flags.outTangentType != ETangentType::Linear)
					// Match continuous slope on left.
					this->ds = 2.0f * this->ds - prev->dd;
			}
		}
	}

#ifdef _DEBUG
	// Verify clamped values
	void interpolate(const ClampedKey& key2, float tr, T& val) const
	{
		SplineKey<T>::interpolate(key2, tr, val);

		static const int Dim = sizeof(T) / sizeof(float);
		typedef const float* F;

		for (int i = 0; i < Dim; ++i)
		{
			if (((F)&this->value)[i] < ((F)&key2.value)[i])
				assert(((F)&val)[i] >= ((F)&this->value)[i] && ((F)&val)[i] <= ((F)&key2.value)[i]);
			else
				assert(((F)&val)[i] <= ((F)&this->value)[i] && ((F)&val)[i] >= ((F)&key2.value)[i]);
		}
	}
#endif
};

//! CatmullRomSpline class implementation
template<class T>
struct CatmullRomKey : public SplineKey<T>
{
	void compute_tangents(const CatmullRomKey* prev, const CatmullRomKey* next, int = 0, int = 0)
	{
		if (prev && next)
		{
			this->ds = this->dd = 0.5f * (next->value - prev->value);
		}
		else
		{
			if (prev)
				this->ds = (0.5f) * (this->value - prev->value);
			if (next)
				this->dd = (0.5f) * (next->value - this->value);
		}
	}
};

template<class T>
class CatmullRomSpline : public TSpline<CatmullRomKey<T>>
{
};

template<class T>
struct HermiteKey : CatmullRomKey<T>
{
};

template<class T>
struct BezierKey : public SplineKey<T>
{
	void compute_tangents(const BezierKey* prev, const BezierKey* next, int, int)
	{
		if (prev || next)
		{
			const float oneThird = 1 / 3.0f;

			if (!prev)
			{
				switch (this->flags.outTangentType)
				{
				case ETangentType::Smooth:
				case ETangentType::Linear:
					this->dd = oneThird * (next->value - this->value);
				}
			}
			else if (!next)
			{
				switch (this->flags.inTangentType)
				{
				case ETangentType::Smooth:
				case ETangentType::Linear:
					this->ds = oneThird * (this->value - prev->value);
				}
			}
			else
			{
				// middle key
				T ds0 = this->ds;
				T dd0 = this->dd;

				const float deltaTime = next->time - prev->time;
				if (deltaTime <= 0)
				{
					Zero(this->ds);
					Zero(this->dd);
				}
				else
				{
					const float k = (this->time - prev->time) / deltaTime;
					const T deltaValue = next->value - prev->value;
					this->ds = oneThird * deltaValue * k;
					this->dd = oneThird * deltaValue * (1 - k);
				}

				switch (this->flags.inTangentType)
				{
				case ETangentType::Linear:
					this->ds = oneThird * (this->value - prev->value);
					break;
				case ETangentType::Custom:
					this->ds = ds0;
					break;
				}

				switch (this->flags.outTangentType)
				{
				case ETangentType::Linear:
					this->dd = oneThird * (next->value - this->value);
					break;
				case ETangentType::Custom:
					this->dd = dd0;
					break;
				}
			}
		}
	}

	void interpolate(const BezierKey& key2, float tr, T& val) const
	{
		const T p0 = this->value;
		const T p3 = key2.value;
		const T p1 = p0 + this->dd;
		const T p2 = p3 - key2.ds;

		const float t = tr;
		const float t2 = t * t;
		const float t3 = t2 * t;

		const float basis[4] =
		{
			-t3 + 3 * t2 - 3 * t + 1,
			3 * t3 - 6 * t2 + 3 * t,
			-3 * t3 + 3 * t2,
			t3
		};

		val = (basis[0] * p0) + (basis[1] * p1) + (basis[2] * p2) + (basis[3] * p3);
	}
};

//////////////////////////////////////////////////////////////////////////
template<class Spline, int DIM>
struct CMultiSplineEvaluator : public IMultiSplineEvaluator
{
	virtual int               GetNumSplines() const override { return DIM; }
	virtual ISplineEvaluator* GetSpline(int i) override      { return &m_splines[i]; }
	// virtual bool Serialize(yasli::Archive& ar, const char* name = "", const char* label = "");

protected:
	Spline m_splines[DIM];
};

//! Base class for spline interpolators.
template<typename Spline>
struct SSplineBackup : public ISplineBackup, public Spline
{
	int refCount;

	SSplineBackup(Spline const& s) : Spline(s), refCount(0) {}
	virtual void              AddRef()    { ++refCount; }
	virtual void              Release()   { if (--refCount <= 0) delete this; }
	virtual ISplineEvaluator* GetSpline() { return static_cast<Spline*>(this); }
};

template<class Spline>
class CBaseSplineInterpolator : public ISplineInterpolator, public Spline
{
public:
	typedef Spline spline_type;
	using_type(Spline, key_type)
	using_type(Spline, value_type)

	static const int DIM = sizeof(value_type) / sizeof(ElemType);

	static void ToValueType(const value_type& t, ElemType v[])   { *(value_type*)v = t; }
	static void FromValueType(const ElemType v[], value_type& t) { t = *(value_type*)v; }

	static void ToKeyType(const key_type& in, KeyType& out)
	{
		out.time = in.time;
		out.flags = in.flags;
		ToValueType(in.value, out.value);
		ToValueType(in.ds, out.ds);
		ToValueType(in.dd, out.dd);
	}
	static void FromKeyType(const KeyType& in, key_type& out)
	{
		out.flags = in.flags;
		out.time = in.time;
		FromValueType(in.value, out.value);
		FromValueType(in.ds, out.ds);
		FromValueType(in.dd, out.dd);
	}

	// ISplineEvaluator
	virtual int GetNumDimensions() const
	{
		static_assert(sizeof(value_type) % sizeof(ElemType) == 0, "Invalid type sizes!");
		static_assert(DIM > 0 && DIM <= 4, "Invalid dimension count!");
		return DIM;
	}
	virtual int GetKeyCount() const
	{
		return this->num_keys();
	}
	virtual void GetKey(int i, KeyType& key) const
	{
		ToKeyType(this->key(i), key);
	}
	virtual void FromSpline(const ISplineEvaluator& source)
	{
		this->resize(source.GetKeyCount());
		KeyType key;
		for (int i = 0; i < this->num_keys(); ++i)
		{
			source.GetKey(i, key);
			FromKeyType(key, this->key(i));
		}
		this->SetModified(true, true);
		this->update();
	}
	virtual bool FromString(cstr str)
	{
		Formatting format = GetFormatting();
		stack_string scopy = str;
		char* start = scopy.begin();
		KeyType inkey;
		key_type key;
		this->clear();
		while (inkey.ParseFromString(start, format))
		{
			FromKeyType(inkey, key);
			this->insert_key(key);
		}
		this->SetModified(true);
		this->update();
		return true;
	}

	virtual ISplineBackup* Backup()
	{
		return new SSplineBackup<CBaseSplineInterpolator<Spline>>(*this);
	}
	virtual void Restore(ISplineBackup* p)
	{
		*this = *static_cast<SSplineBackup<CBaseSplineInterpolator<Spline>>*>(p);
	}

	virtual void Interpolate(float time, ValueType& value)
	{
		value_type v;
		this->interpolate(time, v);
		ToValueType(v, value);
	}

	//////////////////////////////////////////////////////////////////////////
	// ISplineInterpolator
	virtual void Clear()
	{
		this->clear();
	}

	virtual int InsertKey(const KeyType& skey)
	{
		key_type key;
		FromKeyType(skey, key);
		int i = this->insert_key(key);
		this->SetModified(true);
		return i;
	}
	virtual int InsertKey(float t, ValueType val)
	{
		key_type key;
		key.time = t;
		FromValueType(val, key.value);
		int i = this->insert_key(key);
		this->SetModified(true);
		return i;
	}
	virtual void RemoveKey(int key)
	{
		if (key >= 0 && key < this->num_keys())
		{
			this->erase(key);
			this->SetModified(true);
		}
	}
	virtual void FindKeysInRange(float startTime, float endTime, int& firstFoundKey, int& numFoundKeys)
	{
		int count = this->num_keys();
		int start = 0;
		int end = count;
		for (int i = 0; i < count; ++i)
		{
			float keyTime = this->key(i).time;
			if (keyTime < startTime)
				start = i + 1;
			if (keyTime > endTime && end > i)
				end = i;
		}
		if (start < end)
		{
			firstFoundKey = start;
			numFoundKeys = end - start;
		}
		else
		{
			firstFoundKey = -1;
			numFoundKeys = 0;
		}
	}
	virtual void RemoveKeysInRange(float startTime, float endTime)
	{
		int firstFoundKey, numFoundKeys;
		FindKeysInRange(startTime, endTime, firstFoundKey, numFoundKeys);
		while (numFoundKeys-- > 0)
			this->erase(firstFoundKey++);
		this->SetModified(true);
	}
	virtual float GetKeyTime(int key)
	{
		if (key >= 0 && key < this->num_keys())
			return this->key(key).time;
		return 0;
	}
	virtual bool GetKeyValue(int key, ValueType& val)
	{
		if (key >= 0 && key < this->num_keys())
		{
			ToValueType(this->key(key).value, val);
			return true;
		}
		return false;
	}
	virtual void SetKeyValue(int k, ValueType val)
	{
		if (k >= 0 && k < this->num_keys())
		{
			FromValueType(val, this->key(k).value);
			this->SetModified(true);
		}
	}
	virtual void SetKeyTime(int k, float fTime)
	{
		if (k >= 0 && k < this->num_keys())
		{
			this->key(k).time = fTime;
			this->SetModified(true, true);
		}
	}
	virtual void SetKeyInTangent(int k, ValueType tin)
	{
		if (k >= 0 && k < this->num_keys())
		{
			FromValueType(tin, this->key(k).ds);
			this->SetModified(true);
		}
	}
	virtual void SetKeyOutTangent(int k, ValueType tout)
	{
		if (k >= 0 && k < this->num_keys())
		{
			FromValueType(tout, this->key(k).dd);
			this->SetModified(true);
		}
	}
	virtual void SetKeyTangents(int k, ValueType tin, ValueType tout)
	{
		if (k >= 0 && k < this->num_keys())
		{
			FromValueType(tin, this->key(k).ds);
			FromValueType(tout, this->key(k).dd);
			this->SetModified(true);
		}
	}
	virtual bool GetKeyTangents(int k, ValueType& tin, ValueType& tout)
	{
		if (k >= 0 && k < this->num_keys())
		{
			ToValueType(this->key(k).ds, tin);
			ToValueType(this->key(k).dd, tout);
			return true;
		}
		else
			return false;
	}
	virtual void SetKeyFlags(int k, int flags)
	{
		if (k >= 0 && k < this->num_keys())
		{
			this->key(k).flags = flags;
			this->SetModified(true);
		}
	}
	virtual int GetKeyFlags(int k)
	{
		if (k >= 0 && k < this->num_keys())
		{
			return this->key(k).flags;
		}
		return 0;
	}
};

template<class TKey>
class CSplineKeyInterpolator : public CBaseSplineInterpolator<TSpline<TKey>>
{
};

} //namespace spline

//! \endcond