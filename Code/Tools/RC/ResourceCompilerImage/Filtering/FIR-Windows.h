// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2013-2014.
// -------------------------------------------------------------------------
//  File name:   FIR-Windows.h
//  Version:     v1.00
//  Created:     24/05/2013 by nielsf.
//  Compilers:   Visual Studio.NET
//  Description:
//   A collection of window functions with Finite Impulse Response [FIR]
//   and some helper window functions with Infinite Impulse Response [IIR]
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef FIR_WINDOWS_H
#define FIR_WINDOWS_H

#define _USE_MATH_DEFINES
#include <math.h>

/* ####################################################################################################################
 */

enum EWindowFunction
{
	eWindowFunction_Combiner        =  0,

	/*--------------- unit-area filters for unit-spaced samples ----------------*/
	eWindowFunction_Point           =  1,

	eWindowFunction_Box             =  2, // box, pulse, Fourier window,  1st order (constant) b-spline
	eWindowFunction_Triangle        =  3, // triangle, Bartlett window,  2nd order (linear) b-spline
	eWindowFunction_Linear          =  eWindowFunction_Triangle,
	eWindowFunction_Bartlett        =  eWindowFunction_Triangle,

	eWindowFunction_Quadric         =  4, // 3rd order (quadratic) b-spline
	eWindowFunction_Bilinear        =  eWindowFunction_Quadric,
	eWindowFunction_Welch           =  eWindowFunction_Quadric,
	eWindowFunction_Cubic           =  5, // 4th order (cubic) b-spline
	eWindowFunction_Hermite         =  6, // 4th order (cubic hermite) b-spline
	eWindowFunction_Catrom          =  7, // Catmull-Rom spline, Overhauser spline

	eWindowFunction_Sine            =  8, // IIR
	eWindowFunction_Sinc            =  9, // Sinc, perfect lowpass filter (infinite)
	eWindowFunction_Bessel          = 10, // Bessel (for circularly symm. 2-d filt, inf)
	eWindowFunction_Lanczos         = 11, // Lanczos filtering, windowed Sinc

	/*------------------ filters for non-unit spaced samples -------------------*/
	eWindowFunction_Gaussian        = 12, // Gaussian (infinite)
	eWindowFunction_Normal          = 13, // Normal distribution (infinite)

	/*------------------------- parameterized filters --------------------------*/
	eWindowFunction_Mitchell        = 14, // Mitchell & Netravali's two-param cubic

	/*--------------------------- window functions -----------------------------*/
	eWindowFunction_Hann            = 15, // Hanning window
	eWindowFunction_BartlettHann    = 16,
	eWindowFunction_Hamming         = 17, // Hamming window
	eWindowFunction_Blackman        = 18, // Blackman window
	eWindowFunction_BlackmanHarris  = 19,
	eWindowFunction_BlackmanNuttall = 20,
	eWindowFunction_Flattop         = 21,

	/*------------------------- parameterized windows --------------------------*/
	eWindowFunction_Kaiser          = 22, // parameterized Kaiser window

	/*---------------------------- custom windows ------------------------------*/
	eWindowFunction_SigmaSix        = 23, // two Normal distributions
	eWindowFunction_KaiserSinc      = 24, // Kaiser and Sinc

	eWindowFunction_Num             = eWindowFunction_KaiserSinc + 1,
};

int GetFilterFunctionDefault();
int GetFilterFunctionCount();
const char* GetFilterFunctionName(int index);
const int GetFilterFunctionIndex(const char* name, int defaultIndex, const char* disabledValue);

enum EWindowEvaluation
{
	eWindowEvaluation_Sum,
	eWindowEvaluation_Max,
	eWindowEvaluation_Min,
};

int GetFilterEvaluationDefault();
int GetFilterEvaluationCount();
const char* GetFilterEvaluationName(int index);
const int GetFilterEvaluationIndex(const char* name, int defaultIndex);

/* ####################################################################################################################
 */
template<typename T = float>
class IWindowFunction
{

public:
	virtual ~IWindowFunction() {}

	virtual const char* getName() const = 0;
	virtual T getLength() const = 0;

	virtual bool isCardinal() const = 0;
	virtual bool isInfinite() const = 0;
	virtual bool isUnitSpaced() const = 0;
	virtual bool isCentered() const = 0;

public:
	virtual T operator () (T pos) const = 0;
};

/* ####################################################################################################################
 * box, pulse, Fourier window,
 * box function also know as rectangle function
 * 1st order (constant) b-spline
 */
template<typename T = double>
class BoxWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Box-window";
	}
	virtual T getLength() const
	{
		return 0.5;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos <  0.0)
		{
			pos = -pos;
		}
		if (pos <= 0.5)
		{
			return 1.0;
		}
		return 0.0;
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * triangle, Bartlett window,
 * triangle function also known as lambda function
 * 2nd order (linear) b-spline 
 */
template<typename T = double>
class TriangleWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Triangle-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < 1.0)
		{
			return 1.0 - pos;
		}
		return 0.0;
	}
};

/* ####################################################################################################################
 * 3rd order (quadratic) b-spline
 */
template<typename T = double>
class QuadricWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Quadric-window";
	}
	virtual T getLength() const
	{
		return 1.5;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < 0.5)
		{
			return 0.75 - square(pos);
		}
		if (pos < 1.5)
		{
			return 0.50 * square(pos - 1.5);
		}
		return 0.0;
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * 4th order (cubic) b-spline
 */
template<typename T = double>
class CubicWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Cubic-window";
	}
	virtual T getLength() const
	{
		return 2.0;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < 1.0)
		{
			return 0.5 * cube(pos) - square(pos) + 2.0 / 3.0;
		}
		if (pos < 2.0)
		{
			return cube(2.0 - pos) / 6.0;
		}
		return 0.0;
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Hermite filter
 * f(x) = 2|x|^3 - 3|x|^2 + 1, -1 <= x <= 1
 */
template<typename T = double>
class HermiteWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Hermite-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < 1.0)
		{
			return 2.0 * cube(pos) - 3.0 * square(pos) + 1.0;
		}
		return 0.0;
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Catmull-Rom spline, Overhauser spline 
 */
template<typename T = double>
class CatromWindowFunction : public IWindowFunction<T>
{

public:
	CatromWindowFunction() { }

public:
	virtual const char* getName() const
	{
		return "Catrom-window";
	}
	virtual T getLength() const
	{
		return 2.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < 1.0)
		{
			return  1.5 * cube(pos) - 2.5 * square(pos)             + 1.0;
		}
		if (pos < 2.0)
		{
			return -0.5 * cube(pos) + 2.5 * square(pos) - 4.0 * pos + 2.0;
		}
		return  0.0;
	}
};

/* ####################################################################################################################
 */
template<typename T = double>
class SineWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Sine-window";
	}
	virtual T getLength() const
	{
		return 0.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return sin(pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * sinc, perfect lowpass filter (infinite)
 *
 * Note: Some people say sinc(x) is sin(x)/x.  Others say it's
 * sin(PI*x)/(PI*x), a horizontal compression of the former which is
 * zero at integer values.  We use the latter, whose Fourier transform
 * is a canonical rectangle function (edges at -1/2, +1/2, height 1).
 */
template<typename T = double>
class SincWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Sinc-window";
	}
	virtual T getLength() const
	{
		return 4.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos == 0.0)
		{
			return 1.0;
		}

		return sin(M_PI * pos) / (M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Bessel (for circularly symm. 2-d filt, infinite)
 * See Pratt "Digital Image Processing" p. 97 for Bessel functions
 */
template<typename T = double>
class BesselWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Bessel-window";
	}
	virtual T getLength() const
	{
		return 3.2383;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos == 0.0)
		{
			return M_PI / 4.0;
		}

		return j1(M_PI * pos) / (2.0 * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Lanczos filter
 */
template<typename T = double>
class LanczosWindowFunction : public SincWindowFunction<T>
{

public:
	explicit LanczosWindowFunction(T tap = 3.0)
	{
		this->tap = max(3.0, tap);
	}

protected:
	T tap;

public:
	virtual const char* getName() const
	{
		return "Lanczos-window";
	}
	virtual T getLength() const
	{
		return tap;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < tap)
		{
			return ((SincWindowFunction<T>)*this)(pos) * ((SincWindowFunction<T>)*this)(pos / tap);
		}
		return 0.0;
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Gaussian filter (infinite)
 */
template<typename T = double>
class GaussianWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Gaussian-window";
	}
	virtual T getLength() const
	{
		return 1.25;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return exp(-2.0 * square(pos)) * sqrt(2.0 / M_PI);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * normal distribution (infinite)
 * Normal(x) = Gaussian(x/2)/2
 */
template<typename T = double>
class NormalWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Normal-window";
	}
	virtual T getLength() const
	{
		return 2.5;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return exp(-square(pos) / 2.0) / sqrt(2.0 * M_PI);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 */
template<typename T = double>
class SigmaSixWindowFunction : public IWindowFunction<T>
{

public:
	SigmaSixWindowFunction(T diameter = 1.0, T negative = 0.0)
	{
		// we aim for 6 * sigma = 99,99996% of all values
		const T sigma = 1.0 / 3.0;

		s2 = sigma * sigma * 2.0;
		d2 = s2 / (diameter * diameter);
		d = diameter;
		n = negative;
	}

protected:
	T s2, d2, d, n;

public:
	virtual const char* getName() const
	{
		return "SigmaSix-window";
	}
	virtual T getLength() const
	{
		return 1.44;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		T o = exp(-square(pos) / s2) - (1.0 - 0.9999996);
		T i = exp(-square(pos) / d2) - (1.0 - 0.9999996);
		return (pos >= d ? 0.0 : i) - o * n;
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Mitchell & Netravali's two-param cubic
 * see Mitchell & Netravali,
 * "Reconstruction Filters in Computer Graphics", SIGGRAPH 88
 */
template<typename T = double>
class MitchellWindowFunction : public IWindowFunction<T>
{

public:
	MitchellWindowFunction(T b = 1.0 / 3.0, T c = 1.0 / 3.0)
	{
		p0 = (  6. -  2. * b          ) / 6.;
		p2 = (-18. + 12. * b +  6. * c) / 6.;
		p3 = ( 12. -  9. * b -  6. * c) / 6.;
		q0 = (        8. * b + 24. * c) / 6.;
		q1 = (     - 12. * b - 48. * c) / 6.;
		q2 = (        6. * b + 30. * c) / 6.;
		q3 = (     -       b -  6. * c) / 6.;
	}

protected:
	T p0, p2, p3, q0, q1, q2, q3;

public:
	virtual const char* getName() const
	{
		return "Mitchell-window";
	}
	virtual T getLength() const
	{
		return 2.0;
	}

	virtual bool isCardinal() const
	{
		return false;
	}
	virtual bool isInfinite() const
	{
		return false;
	}
	virtual bool isUnitSpaced() const
	{
		return false;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		if (pos < 0.0)
		{
			pos = -pos;
		}
		if (pos < 1.0)
		{
			return p3 * cube(pos) + p2 * square(pos)            + p0;
		}
		if (pos < 2.0)
		{
			return q3 * cube(pos) + q2 * square(pos) + q1 * pos + q0;
		}
		return 0.0;
	}
};

/* ####################################################################################################################
 * Hanning window (infinite)
 */
template<typename T = double>
class HannWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Hann-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.5 + 0.5 * cos(M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * BartlettHanning window (infinite)
 */
template<typename T = double>
class BartlettHannWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Bartlett-Hann-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.62 + 0.48 * (1.0 - pos) + 0.38 * cos(M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Hamming window (infinite)
 */
template<typename T = double>
class HammingWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Hamming-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.53836 + 0.46164 * cos(M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * Blackman window (infinite)
 */
template<typename T = double>
class BlackmanWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Blackman-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.42659
		     + 0.49656 * cos(      M_PI * pos)
		     + 0.07685 * cos(2.0 * M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * BlackmanHarris window (infinite)
 */
template<typename T = double>
class BlackmanHarrisWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Blackman-Harris-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.35875
		     + 0.48829 * cos(      M_PI * pos)
		     + 0.14128 * cos(2.0 * M_PI * pos)
		     + 0.01168 * cos(3.0 * M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * BlackmanNuttall window (infinite)
 */
template<typename T = double>
class BlackmanNuttallWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Blackman-Nuttall-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.3635819
		     + 0.4891775 * cos(      M_PI * pos)
		     + 0.1365995 * cos(2.0 * M_PI * pos)
		     + 0.0106411 * cos(3.0 * M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * FlatTop window (infinite)
 */
template<typename T = double>
class FlatTopWindowFunction : public IWindowFunction<T>
{

public:
	virtual const char* getName() const
	{
		return "Flat-Top-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return 0.215578948
		     + 0.416631580 * cos(      M_PI * pos)
		     + 0.277263158 * cos(2.0 * M_PI * pos)
		     + 0.083578947 * cos(3.0 * M_PI * pos)
		     + 0.006947368 * cos(4.0 * M_PI * pos);
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 * parameterized Kaiser window (infinite)
 * from Oppenheim & Schafer, Hamming
 */
template<typename T = double>
class KaiserWindowFunction : public IWindowFunction<T>
{

public:
	explicit KaiserWindowFunction(T a = 6.5)
	{
		/* typically 4<a<9 */
		/* param a trades off main lobe width (sharpness) */
		/* for side lobe amplitude (ringing) */
		this->a   = a;
		this->i0a = 1. / bessel_i0(a);
	}

protected:
	T a, i0a;

	/* modified zeroth order Bessel function of the first kind. */
	static T bessel_i0(T x)
	{
#define EPSILON 1e-7
		const T y = square(x) / 4.0;

		T sum = 1.0;
		T t = y;

		for (int i = 2; t > EPSILON; i++)
		{
			sum += t;
			t *= y / square(i);
		}

		return sum;

#undef EPSILON
	}

public:
	virtual const char* getName() const
	{
		return "Kaiser-window";
	}
	virtual T getLength() const
	{
		return 1.0;
	}

	virtual bool isCardinal() const
	{
		return true;
	}
	virtual bool isInfinite() const
	{
		return true;
	}
	virtual bool isUnitSpaced() const
	{
		return true;
	}
	virtual bool isCentered() const
	{
		return true;
	}

public:
	virtual T operator () (T pos) const
	{
		return i0a * bessel_i0(a * sqrt(1.0 - square(pos)));
	}
};

/* ####################################################################################################################
 */
template<typename T = double>
class CombinerWindowFunction : public IWindowFunction<T>
{

public:
	CombinerWindowFunction(IWindowFunction<T>* fu, IWindowFunction<T>* wi)
	{
		shaper     = fu;
		restrictor = wi;
	}

protected:
	IWindowFunction<T>* shaper;
	IWindowFunction<T>* restrictor;

public:
	virtual const char* getName() const
	{
		return "Combiner of two window-generators";
	}
	virtual T getLength() const
	{
		return shaper->getLength();
	}

	virtual bool isCardinal() const
	{
		return shaper->isCardinal() && restrictor->isCardinal();
	}
	virtual bool isInfinite() const
	{
		return shaper->isInfinite() && restrictor->isInfinite();
	}
	virtual bool isUnitSpaced() const
	{
		return shaper->isUnitSpaced() && restrictor->isUnitSpaced();
	}
	virtual bool isCentered() const
	{
		return shaper->isCentered() && restrictor->isCentered();
	}

public:
	virtual T operator () (T pos) const
	{
		return (*shaper)(pos) * (*restrictor)(pos / shaper->getLength());
	}
};

/* --------------------------------------------------------------------------------------------------------------------
 */
template<typename T = double>
class PointWindowFunction : public BoxWindowFunction<T>
{

public:
	PointWindowFunction() : BoxWindowFunction<T>() { }

public:
	virtual const char* getName() const
	{
		return "Point-window";
	}
	virtual T getLength() const
	{
		return 0.0;
	}
};

#endif // FIR_WINDOWS_H
