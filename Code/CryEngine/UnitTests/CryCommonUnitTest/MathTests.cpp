// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryCore/BitFiddling.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Math_SSE.h>
#include <CryMath/LCGRandom.h>
#include <CryMath/SNoise.h>
#include <CryMath/RadixSort.h>
#include <CryMath/Cry_GeoProjection.h>
#include <CryMemory/HeapAllocator.h>
using namespace crymath;

const float HALF_EPSILON = 0.00025f;
CRndGen MathRand;  // Static generator for reproducible sequence

TEST(CryMathTest, BitFiddling)
{
	for (uint32 i = 0, j = ~0U; i < 32; ++i, j >>= 1)
		REQUIRE(countLeadingZeros32(j) == i);
	for (uint32 i = 0, j = ~0U; i < 32; ++i, j <<= 1)
		REQUIRE(countTrailingZeros32(j) == i);

	for (uint64 i = 0, j = ~0ULL; i < 64; ++i, j >>= 1)
		REQUIRE(countLeadingZeros64(j) == i);
	for (uint64 i = 0, j = ~0ULL; i < 64; ++i, j <<= 1)
		REQUIRE(countTrailingZeros64(j) == i);

	for (uint8 i = 7, j = 0x80; i > 0; --i, j >>= 1)
	{
		REQUIRE(IntegerLog2(uint8(j)) == i);
		REQUIRE(IntegerLog2(uint8(j + (j >> 1))) == i);
		REQUIRE(IntegerLog2_RoundUp(uint8(j)) == i);
		REQUIRE(IntegerLog2_RoundUp(uint8(j + (j >> 1))) == (i + 1));
	}
}

float RandomExp(float fMaxExp)
{
	float f = pow(10.0f, MathRand.GetRandom(-fMaxExp, fMaxExp));
	if (MathRand.GetRandom(0, 1))
		f = -f;
	assert(IsValid(f));
	return f;
}

template<typename Real, typename S>
void TestFunction(Real(*func)(Real), S in, S res)
{
	Real out = func(convert<Real>(in));
	REQUIRE(All(out == convert<Real>(res)));
}

template<typename Real, typename S>
void TestFunction(Real(*func)(Real, Real), S a, S b, S res)
{
	Real out = func(convert<Real>(a), convert<Real>(b));
	REQUIRE(All(out == convert<Real>(res)));
}

template<typename Real, typename S>
void TestFunction(Real(*func)(Real, Real, Real), S a, S b, S c, S res)
{
	Real out = func(convert<Real>(a), convert<Real>(b), convert<Real>(c));
	REQUIRE(All(out == convert<Real>(res)));
}

template<typename Real, typename S>
bool IsEquiv(Real a, Real b, S epsilon)
{
	return NumberValid(a) && NumberValid(b) && IsEquivalent(a, b, -epsilon); // Relative comparison
}

template<typename Real, typename S>
void TestFunction(Real(*func)(Real), S in, S res, S epsilon)
{
	Real out = func(convert<Real>(in));
	REQUIRE(IsEquiv(out, convert<Real>(res), epsilon));
}

template<typename Real, typename S>
void TestFunction(Real(*func)(Real, Real), S a, S b, S res, S epsilon)
{
	Real out = func(convert<Real>(a), convert<Real>(b));
	REQUIRE(IsEquiv(out, convert<Real>(res), epsilon));
}

template<typename Real, typename S>
void TestFunction(Real(*func)(Real, Real, Real), S a, S b, S c, S res, S epsilon)
{
	Real out = func(convert<Real>(a), convert<Real>(b), convert<Real>(c));
	REQUIRE(IsEquiv(out, convert<Real>(res), epsilon));
}

template<typename Real, typename S>
void FunctionApproxTest(Real(*func)(Real), Real(*func_fast)(Real), S in, S res)
{
	TestFunction(func, in, res, FLT_EPSILON * 2.0f);
	TestFunction(func_fast, in, res, HALF_EPSILON * 2.0f);
}

template<typename Real, typename Int, typename Uint>
void FunctionTest()
{
	// Check trunc, floor, ceil
	float fdata[] = { 0.0f, 257.85f, 124.35f, 73.0f, 0.806228399f, 0.2537399f, -0.123432f, -0.986552f, -5.0f, -63.4f, -102.7f, 1e-18f, -4.567e8f };

	for (float f : fdata)
	{
		float res;

		res = float(int(f));
		TestFunction<Real>(trunc, f, res);

		res = std::floor(f);
		TestFunction<Real>(floor, f, res);

		res = std::ceil(f);
		TestFunction<Real>(ceil, f, res);

		res = f<0.0f ? 0.0f : f> 1.0f ? 1.0f : f;
		TestFunction<Real>(saturate, f, res);

		res = f < 0.0f ? -f : f;
		TestFunction<Real>(abs, f, res);

		res = f < 0.0f ? -1.0f : 1.0f;
		TestFunction<Real>(signnz, f, res);

		res = f<0.0f ? -1.0f : f> 0.0f ? 1.0f : 0.0f;
		TestFunction<Real>(sign, f, res);

		FunctionApproxTest<Real>(sqrt, sqrt_fast, f * f, abs(f));

		if (f != 0.0f)
		{
			FunctionApproxTest<Real>(rcp, rcp_fast, f, 1.0f / f);
			FunctionApproxTest<Real>(rsqrt, rsqrt_fast, 1.0f / (f * f), abs(f));
		}
	}

	TestFunction<Real>(max, 257.35f, 510.0f, 510.0f);
	TestFunction<Real>(max, -102.0f, -201.4f, -102.0f);

	//Disabled until fixing bug for mod under linux fast math
	//https://jira.cryengine.com/browse/DEV-8389
	//TestFunction<Real>(mod, 3.0f, 1.0f, 0.0f, FLT_EPSILON * 4);
	//TestFunction<Real>(mod, 0.75f, 0.5f, 0.25f, FLT_EPSILON * 4);
	//TestFunction<Real>(mod, -0.6f, 0.5f, -0.1f, FLT_EPSILON * 4);
	//TestFunction<Real>(mod, -0.2f, -0.3f, -0.2f, FLT_EPSILON * 4);

	TestFunction<Real>(wrap, -6.0f, -1.0f, 3.0f, 2.0f, FLT_EPSILON * 4);
	TestFunction<Real>(wrap, -6.3f, 0.0f, 1.0f, 0.7f, FLT_EPSILON * 4);
	TestFunction<Real>(wrap, 4.8f, -1.0f, 1.0f, 0.8f, FLT_EPSILON * 4);

	TestFunction<Int>(min, -3, 1, -3);
	TestFunction<Int>(max, -3, 1, 1);

	TestFunction<Uint>(min, 8u, 3u, 3u);
	TestFunction<Uint>(min, 8u, 0x9FFFFFFFu, 8u);
	TestFunction<Uint>(max, 8u, 0x9FFFFFFFu, 0x9FFFFFFFu);
	TestFunction<Uint>(max, 0x90000000u, 0xA0000000u, 0xA0000000u);
}

#if CRY_PLATFORM_SSE2

template<typename V>
void ConvertTest()
{
	using T = scalar_t<V>;
	V v = convert<V>(T(0), T(1), T(2), T(3));
	REQUIRE(NumberValid(v));
	REQUIRE(get_element<0>(v) == T(0));
	REQUIRE(get_element<3>(v) == T(3));
	v = convert<V>(T(1), 0);
	REQUIRE(All(v == convert<V>(T(1), T(0), T(0), T(0))));
}

#define ELEMENT_OPERATE(V, a, op, b) convert<V>( \
		  get_element<0>(a) op get_element<0>(b),        \
		  get_element<1>(a) op get_element<1>(b),        \
		  get_element<2>(a) op get_element<2>(b),        \
		  get_element<3>(a) op get_element<3>(b))        \

template<typename V>
void TestIntOperators(V a, V b)
{
	REQUIRE(All((a + b) == ELEMENT_OPERATE(V, a, +, b)));
	REQUIRE(All((a - b) == ELEMENT_OPERATE(V, a, -, b)));
	REQUIRE(All((a * b) == ELEMENT_OPERATE(V, a, *, b)));
	REQUIRE(All((a / b) == ELEMENT_OPERATE(V, a, / , b)));
	REQUIRE(All((a % b) == ELEMENT_OPERATE(V, a, %, b)));
	REQUIRE(All((a & b) == ELEMENT_OPERATE(V, a, &, b)));
	REQUIRE(All((a | b) == ELEMENT_OPERATE(V, a, | , b)));
	REQUIRE(All((a ^ b) == ELEMENT_OPERATE(V, a, ^, b)));
	REQUIRE(All((a << Scalar(3)) == ELEMENT_OPERATE(V, a, << , convert<V>(3))));
	REQUIRE(All((a >> Scalar(2)) == ELEMENT_OPERATE(V, a, >> , convert<V>(2))));
}

template<typename V>
void TestFloatOperators(V a, V b)
{
	REQUIRE(All((a + b) == ELEMENT_OPERATE(V, a, +, b)));
	REQUIRE(All((a - b) == ELEMENT_OPERATE(V, a, -, b)));
	REQUIRE(All((a * b) == ELEMENT_OPERATE(V, a, *, b)));
	REQUIRE(IsEquiv(a / b, ELEMENT_OPERATE(V, a, / , b), FLT_EPSILON));
}

template<typename V>
void HorizontalTest()
{
	i32v4 idata[] = {
		convert<i32v4>(1,  -2,  3,   -4),
		convert<i32v4>(-2, -1,  5,   6),
		convert<i32v4>(7,  -12, -13, 24),
		convert<i32v4>(-2, -5,  5,   6)
	};

	for (i32v4 d : idata)
	{
		V v = convert<V>(d);

		auto h_sum = get_element<0>(v) + get_element<1>(v) + get_element<2>(v) + get_element<3>(v);
		REQUIRE(All(hsumv(v) == to_v4(h_sum)));

		auto h_min = min(get_element<0>(v), min(get_element<1>(v), min(get_element<2>(v), get_element<3>(v))));
		REQUIRE(All(hminv(v) == to_v4(h_min)));

		auto h_max = max(get_element<0>(v), max(get_element<1>(v), max(get_element<2>(v), get_element<3>(v))));
		REQUIRE(All(hmaxv(v) == to_v4(h_max)));
	}
}

template<typename V>
void CompareTest()
{
	REQUIRE(All(convert<V>(-3, -5, 6, 9) == convert<V>(-3, -5, 6, 9)));
	REQUIRE(Any(convert<V>(-3, -5, 6, 9) != convert<V>(-3, 5, 6, 9)));
	REQUIRE(All(convert<V>(-3, -5, 6, 9) < convert<V>(4, -2, 7, 10)));
	REQUIRE(All(convert<V>(-3, -5, 6, 9) <= convert<V>(4, -2, 6, 10)));
	REQUIRE(All(convert<V>(-3, -5, 6, 9) > convert<V>(-4, -7, -8, 0)));
	REQUIRE(All(convert<V>(-3, -5, 6, 9) >= convert<V>(-3, -6, 6, 8)));
}
void UCompareTest()
{
	REQUIRE(All(convert<u32v4>(1, 2, 3, 4) == convert<u32v4>(1, 2, 3, 4)));
	REQUIRE(!All(convert<u32v4>(1, 2, 3, 4) == convert<u32v4>(0, 2, 3, 4)));
	REQUIRE(All(convert<u32v4>(1, 2, 3, 4) != convert<u32v4>(4, 3, 2, 1)));
	REQUIRE(All(convert<u32v4>(0, 2, 3, 4) < convert<u32v4>(2u, 4u, 8u, 0x99999999)));
	REQUIRE(All(convert<u32v4>(0, 2, 3, 4) <= convert<u32v4>(2u, 2u, 3u, 0x99999999)));
	REQUIRE(All(convert<u32v4>(1u, 2u, 3u, 0x80000000) > convert<u32v4>(0, 1, 2, 3)));
	REQUIRE(All(convert<u32v4>(1u, 2u, 3u, 0x80000000) >= convert<u32v4>(1, 2, 3, 4)));
}
#endif // CRY_PLATFORM_SSE2

template<typename F> NO_INLINE
void QuadraticTest(int mode = 2)
{
	F maxErrAbs = 0, maxErrRel = 0;

	for (int i = 0; i < 240; ++i)
	{
		F c0 = RandomExp(6.0f) * (i % 5);
		F c1 = RandomExp(6.0f) * (i % 4);
		F c2 = RandomExp(6.0f) * (i % 3);

		F r[2];
		int n = solve_quadratic(c2, c1, c0, r);
		while (--n >= 0)
		{
			F err = abs(c0 + r[n] * (c1 + r[n] * c2));
			F mag = abs(r[n] * c1) + abs(r[n] * r[n] * c2 * F(2));

			if (err > maxErrAbs)
				maxErrAbs = err;
			if (err > mag * maxErrRel)
			{
				maxErrRel = err / mag;
				REQUIRE(err <= mag * std::numeric_limits<F>::epsilon() * F(5));
			}
		}
	}
}

TEST(CryMathTest, Quadratic)
{
	MathRand = {};
	QuadraticTest<f32>();
	QuadraticTest<f64>();
}

TEST(CryMathTest, Function)
{
	FunctionTest<float, int, uint>();
#if CRY_PLATFORM_SSE2
	FunctionTest<f32v4, i32v4, u32v4>();
#endif // CRY_PLATFORM_SSE2
}

#if CRY_PLATFORM_SSE2

TEST(CryMathTest, Convert)
{
	ConvertTest<f32v4>();
	ConvertTest<i32v4>();
}

TEST(CryMathTest, Operator)
{
	i32v4 idata[6] = {
		convert<i32v4>(1,    -2,    3,   -4),     convert<i32v4>(-2,    -1,     5,    6),
		convert<i32v4>(7,    -12,   -13, 24),     convert<i32v4>(-2,    -5,     5,    6),
		convert<i32v4>(7654, -1234, -13, 249999), convert<i32v4>(-2373, -59999, 5432, 62)
	};

	for (int i = 0; i < 6; i += 2)
	{
		i32v4 a = idata[i];
		i32v4 b = idata[i + 1];

		TestIntOperators(a, b);
		TestIntOperators(convert<u32v4>(abs(a)), convert<u32v4>(abs(b)));
		TestIntOperators(convert<u32v4>(a), convert<u32v4>(b));
		TestFloatOperators(convert<f32v4>(a), convert<f32v4>(b));
	}
}

TEST(CryMathTest, Horizontal)
{
	HorizontalTest<f32v4>();
	HorizontalTest<i32v4>();
	HorizontalTest<u32v4>();
}

TEST(CryMathTest, Compare)
{
	CompareTest<f32v4>();
	CompareTest<i32v4>();
	UCompareTest();
}

TEST(CryMathTest, Select)
{
	REQUIRE(if_else(3 == 4, 1.0f, 2.0f) == 2.0f);
	REQUIRE(if_else_zero(7.0f != 5.0f, 9) == 9);
	REQUIRE(__fsel(-7.0f, -5.0f, -3.0f) == -3.0f);
	REQUIRE(All(if_else(
		convert<f32v4>(0.5f, 0.0f, -7.0f, -0.0f) >= convert<f32v4>(2.0f, -4.0f, -5.0f, 0.0f),
		convert<i32v4>(-3, 3, -5, 0),
		convert<i32v4>(4, 40, -1, -7)
	) == convert<i32v4>(4, 3, -1, 0)));
	REQUIRE(All(if_else_zero(
		convert<i32v4>(3, -7, 0, 4) >= convert<i32v4>(2, 7, -1, -8),
		convert<f32v4>(-3.0f, 3.0f, -3.0f, 0.9f)
	) == convert<f32v4>(-3.0f, 0.0f, -3.0f, 0.9f)));
	REQUIRE(All(__fsel(
		convert<f32v4>(0.5f, 0.0f, -7.0f, -0.0f),
		convert<f32v4>(2.0f, -4.0f, -5.0f, 2.5f),
		convert<f32v4>(-3.0f, 3.0f, -3.0f, 0.9f)
	) == convert<f32v4>(2.0f, -4.0f, -3.0f, 2.5f)));
}
#endif // CRY_PLATFORM_SSE2

#ifdef CRY_HARDWARE_VECTOR4

// Enable this macro only to get vector timing results. Normally off, as it slows down compilation.
// #define VECTOR_PROFILE

float RandomFloat()
{
	return RandomExp(3.0f);
}
Vec4 RandomVec4()
{
	return Vec4(RandomFloat(), RandomFloat(), RandomFloat(), RandomFloat());
}
Matrix44 RandomMatrix44()
{
	return Matrix44(RandomVec4(), RandomVec4(), RandomVec4(), RandomVec4());
}

static const int VCount = 256;
static const int VRep = 1024;

template<typename Vec, typename Mat>
struct Element
{
	float f0;
	Vec v0, v1;
	Mat m0, m1;

	Element() {}

	Element(int)
		: f0(RandomFloat())
		, v0(RandomVec4()), v1(RandomVec4())
		, m0(RandomMatrix44()), m1(RandomMatrix44()) {}

	template<typename Vec2, typename Mat2>
	Element(const Element<Vec2, Mat2>& in)
		: f0(in.f0)
		, v0(in.v0), v1(in.v1)
		, m0(in.m0), m1(in.m1)
	{}
};

std::vector<cstr> TestNames;

template<typename Elem>
struct Tester
{
	cstr name;
	std::vector<int64> times;
	Elem elems[VCount];

	void Log() const
	{
		uint32 divide = VCount * VRep;
#if CRY_PLATFORM_WINDOWS
		// Ticks are kilocycles
		divide /= 1000;
#endif

		CryLog("Timings for %s", name);
		for (int i = 0; i < TestNames.size(); ++i)
		{
			uint32 ticks = uint32((times[i] + divide / 2) / divide);
			CryLog(" %3u: %s", ticks, TestNames[i]);
		}
	}
};

template<typename Elem1, typename Elem2, typename Code1, typename Code2, typename E>
void VerifyCode(cstr message, Tester<Elem1>& tester1, Tester<Elem2>& tester2, Code1 code1, Code2 code2, E tolerance)
{
	for (int i = 0; i < VCount; ++i)
	{
		auto res1 = code1(tester1.elems[i]);
		auto res2 = code2(tester2.elems[i]);
		string msgFormatted;
		msgFormatted.Format(message, i);
		EXPECT_TRUE(IsEquivalent(res1, res2, tolerance)) << msgFormatted;
	}
}

enum class TestMode { Verify, Prime, Profile };

using Element4H = Element<Vec4, Matrix44>;   Tester<Element4H> test4H = { "Vec4H, Matrix44H" };
using Element4 = Element<Vec4f, Matrix44f>;  Tester<Element4>  test4 = { "Vec4, Matrix44" };
using Element3H = Element<Vec3, Matrix34>;   Tester<Element3H> test3H = { "Vec3, Matrix34H" };
using Element3 = Element<Vec3, Matrix34f>;  Tester<Element3>  test3 = { "Vec3, Matrix34" };

void VectorTest(TestMode mode)
{
#ifdef VECTOR_PROFILE
#define	VECTOR_PROFILE_TESTER(tester, code) { \
		auto& e = tester.elems[0]; \
		typedef decltype(code) Result; \
		Result results[VCount]; \
		int i = 0; \
		int64 time = CryGetTicks(); \
		for (auto& e: tester.elems) \
			results[i++] = code; \
		time = CryGetTicks() - time; \
		if (mode == TestMode::Prime) \
			tester.times.resize(TestNames.size()); \
		if (mode == TestMode::Profile) \
			tester.times[stat] += time; \
	}

#define	VECTOR_PROFILE_CODE(code)  { \
		VECTOR_PROFILE_TESTER(test4H, code) \
		VECTOR_PROFILE_TESTER(test4,  code) \
		VECTOR_PROFILE_TESTER(test3H, code) \
		VECTOR_PROFILE_TESTER(test3,  code) \
		++stat; \
	}

#else
#define	VECTOR_PROFILE_CODE(code)
#endif

#define	VECTOR_TEST_CODE(code, tolerance) \
		if (mode == TestMode::Verify) \
		{ \
			if (add_names) TestNames.push_back(#code); \
			VerifyCode<Element4H, Element4>("mismatch 4/4H #%d: " #code, test4H, test4, [](Element4H& e) { return (code); }, [](Element4& e) { return (code); }, tolerance); \
			VerifyCode<Element3H, Element3>("mismatch 3/3H #%d: " #code, test3H, test3, [](Element3H& e) { return (code); }, [](Element3& e) { return (code); }, tolerance); \
		} \
		else \
		{ \
			VECTOR_PROFILE_CODE(code) \
		} \

	static const float Tolerance = -1e-5f;
	bool add_names = TestNames.empty();

	VECTOR_TEST_CODE(e.v0, 0.0f);
	VECTOR_TEST_CODE(e.v0 == e.v1, bool());
	VECTOR_TEST_CODE(IsEquivalent(e.v0, e.v1, Tolerance), bool());
	VECTOR_TEST_CODE(IsEquivalent(e.v0, e.v1, Tolerance), bool());
	VECTOR_TEST_CODE(e.v0 | e.v1, Tolerance * 10.0f);
	VECTOR_TEST_CODE(e.v0 + e.v1, 0.0f);
	VECTOR_TEST_CODE(e.v0 * e.f0, Tolerance);
	VECTOR_TEST_CODE(e.v0.GetLength(), Tolerance);
	VECTOR_TEST_CODE(e.v0.GetNormalized(), Tolerance);
	VECTOR_TEST_CODE(e.v0.ProjectionOn(e.v1), Tolerance * 10.0f);

	VECTOR_TEST_CODE(e.m0, 0.0f);
	VECTOR_TEST_CODE(e.m0 == e.m1, bool());
	VECTOR_TEST_CODE(IsEquivalent(e.m0, e.m1, Tolerance), bool());
	VECTOR_TEST_CODE(IsEquivalent(e.m0, e.m1, Tolerance), bool());
	VECTOR_TEST_CODE(e.m0.GetTransposed(), 0.0f);
	VECTOR_TEST_CODE(e.m0.GetInverted(), Tolerance * 10.0f);
	VECTOR_TEST_CODE(e.m0.Determinant(), Tolerance * 10.0f);

	VECTOR_TEST_CODE(e.v0 * e.m0, Tolerance);
	VECTOR_TEST_CODE(e.m0 * e.v0, Tolerance);
	VECTOR_TEST_CODE(e.m0.TransformVector(e.v0), Tolerance);
	VECTOR_TEST_CODE(e.m0.TransformPoint(e.v0), Tolerance);
	VECTOR_TEST_CODE(e.m0 * e.m1, Tolerance);
}

/*
//Disabled until fixing bug for mod under linux fast math
//https://jira.cryengine.com/browse/DEV-8389
TEST(CryMathTest, Vector)
{
	MathRand = {};

	// Init test to equivalent values
	for (int i = 0; i < VCount; ++i)
	{
		Construct(test4H.elems[i], 1);
		test4.elems[i] = test4H.elems[i];
		test3H.elems[i] = test4H.elems[i];
		test3.elems[i] = test3H.elems[i];
	}

	VectorTest(TestMode::Verify);

#ifdef VECTOR_PROFILE
	VectorTest(TestMode::Prime);
	for (int i = 0; i < VRep; ++i)
	{
		VectorTest(TestMode::Profile);
	}

	test4H.Log();
	test4.Log();
	test3H.Log();
	test3.Log();
#endif
}
*/

template<typename Real>
NO_INLINE Real SNoiseNoInline(Vec4_tpl<Real> v)
{
	return SNoise(v);
}

template<typename Real>
void SnoiseTest()
{
	using namespace crydetail;
	const Vec4_tpl<Real> s0 = ToVec4(convert<Real>(0.0f));
	const Vec4_tpl<Real> s1 = Vec4_tpl<Real>(convert<Real>(104.2f), convert<Real>(504.85f), convert<Real>(32.0f), convert<Real>(10.25f));
	const Vec4_tpl<Real> s2 = Vec4_tpl<Real>(convert<Real>(-104.2f), convert<Real>(504.85f), convert<Real>(-32.0f), convert<Real>(1.25f));
	const Vec4_tpl<Real> s3 = Vec4_tpl<Real>(convert<Real>(-257.07f), convert<Real>(-25.85f), convert<Real>(-0.5f), convert<Real>(105.5f));
	const Vec4_tpl<Real> s4 = Vec4_tpl<Real>(convert<Real>(104.2f), convert<Real>(1504.85f), convert<Real>(78.57f), convert<Real>(-0.75f));
	Real p0 = SNoiseNoInline(s0);
	Real p1 = SNoiseNoInline(s1);
	Real p2 = SNoiseNoInline(s2);
	Real p3 = SNoiseNoInline(s3);
	Real p4 = SNoiseNoInline(s4);
	// #PFX2_TODO : Orbis has a slight difference in precision
#if CRY_COMPILER_MSVC
	REQUIRE(All(p0 == convert<Real>(0.0f)));
	REQUIRE(All(p1 == convert<Real>(-0.291425288f)));
	REQUIRE(All(p2 == convert<Real>(-0.295406163f)));
	REQUIRE(All(p3 == convert<Real>(-0.127176195f)));
	REQUIRE(All(p4 == convert<Real>(-0.0293087773f)));
#endif
}

TEST(CryMathTest, SNoise)
{
	SnoiseTest<float>();
	SnoiseTest<f32v4>();
}

TEST(CryMathTest, RadixSortTest)
{
	stl::HeapAllocator<stl::PSyncNone> heap;

	const uint64 data[] = { 15923216, 450445401561561, 5061954, 5491494, 56494109840, 500120, 520025710, 58974, 45842669, 3226, 995422665 };
	const uint sz = sizeof(data) / sizeof(data[0]);
	const uint32 expectedIndices[sz] = { 9, 7, 5, 2, 3, 0, 8, 6, 10, 4, 1 };
	uint32 indices[sz];

	RadixSort(indices, indices + sz, data, data + sz, heap);

	REQUIRE(memcmp(expectedIndices, indices, sz * 4) == 0);
}

#endif // CRY_HARDWARE_VECTOR4

TEST(CryMathTest, Projection_PointOntoPlane)
{
	// plane normal going up
	{
		Plane plane = Plane::CreatePlane(Vec3(0, 0, 1), Vec3(0, 0, 10));
		Vec3 pointOnPlane = Projection::PointOntoPlane(Vec3(999, 999, 999), plane);
		REQUIRE(pointOnPlane.IsEquivalent(Vec3(999, 999, 10)));
	}

	// plane normal going down
	{
		Plane plane = Plane::CreatePlane(Vec3(0, 0, -1), Vec3(0, 0, 10));
		Vec3 pointOnPlane = Projection::PointOntoPlane(Vec3(999, 999, 999), plane);
		REQUIRE(pointOnPlane.IsEquivalent(Vec3(999, 999, 10)));
	}

	// plane normal going right
	{
		Plane plane = Plane::CreatePlane(Vec3(1, 0, 0), Vec3(10, 0, 0));
		Vec3 pointOnPlane = Projection::PointOntoPlane(Vec3(999, 999, 999), plane);
		REQUIRE(pointOnPlane.IsEquivalent(Vec3(10, 999, 999)));
	}

	// plane normal going left
	{
		Plane plane = Plane::CreatePlane(Vec3(-1, 0, 0), Vec3(10, 0, 0));
		Vec3 pointOnPlane = Projection::PointOntoPlane(Vec3(999, 999, 999), plane);
		REQUIRE(pointOnPlane.IsEquivalent(Vec3(10, 999, 999)));
	}

	// plane normal going into the screen
	{
		Plane plane = Plane::CreatePlane(Vec3(0, 1, 0), Vec3(0, 10, 0));
		Vec3 pointOnPlane = Projection::PointOntoPlane(Vec3(999, 999, 999), plane);
		REQUIRE(pointOnPlane.IsEquivalent(Vec3(999, 10, 999)));
	}

	// plane normal going out of the screen
	{
		Plane plane = Plane::CreatePlane(Vec3(0, -1, 0), Vec3(0, 10, 0));
		Vec3 pointOnPlane = Projection::PointOntoPlane(Vec3(999, 999, 999), plane);
		REQUIRE(pointOnPlane.IsEquivalent(Vec3(999, 10, 999)));
	}
}

TEST(CryMathTest, Projection_PointOntoLine)
{
	// line collinear to the cardinal x-axis
	{
		Line line(Vec3(10, 10, 10), Vec3(1, 0, 0));
		Vec3 pointOnLine = Projection::PointOntoLine(Vec3(999, 999, 999), line);
		REQUIRE(pointOnLine.IsEquivalent(Vec3(999, 10, 10)));
	}

	// line collinear to the cardinal y-axis
	{
		Line line(Vec3(10, 10, 10), Vec3(0, 1, 0));
		Vec3 pointOnLine = Projection::PointOntoLine(Vec3(999, 999, 999), line);
		REQUIRE(pointOnLine.IsEquivalent(Vec3(10, 999, 10)));
	}

	// line collinear to the cardinal z-axis
	{
		Line line(Vec3(10, 10, 10), Vec3(0, 0, 1));
		Vec3 pointOnLine = Projection::PointOntoLine(Vec3(999, 999, 999), line);
		REQUIRE(pointOnLine.IsEquivalent(Vec3(10, 10, 999)));
	}
}

