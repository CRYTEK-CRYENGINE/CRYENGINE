// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/**** Decompose.c ****/
/* Ken Shoemake, 1993 */

#include "StdAfx.h"

#include "Util/AffineParts.h"

#pragma warning ( disable : 4244 )  // conversion from 'double' to 'float', possible loss of data.

/**** Decompose.h - Basic declarations ****/
typedef struct
{
	float x, y, z, w;
} Quatern;                                  /* Quaternernion */
enum QuaternPart {X, Y, Z, W};
typedef Quatern HVect;         /* Homogeneous 3D vector */
typedef float   HMatrix[4][4]; /* Right-handed, for column vectors */
typedef struct
{
	HVect   t;  /* Translation components */
	Quatern q;  /* Essential rotation   */
	Quatern u;  /* Stretch rotation   */
	HVect   k;  /* Stretch factors    */
	float   f;  /* Sign of determinant    */
} SAffineParts;

float   polar_decomp(HMatrix M, HMatrix Q, HMatrix S);
HVect   spect_decomp(HMatrix S, HMatrix U);
Quatern snuggle(Quatern q, HVect* k);

/******* Matrix Preliminaries *******/

/** Fill out 3x3 matrix to 4x4 **/
#define mat_pad(A) (A[W][X] = A[X][W] = A[W][Y] = A[Y][W] = A[W][Z] = A[Z][W] = 0, A[W][W] = 1)

/** Copy nxn matrix A to C using "gets" for assignment **/
#define mat_copy(C, gets, A, n) { int i, j; for (i = 0; i < n; i++) \
  for (j = 0; j < n; j++)                                           \
    C[i][j] gets(A[i][j]); }

/** Copy transpose of nxn matrix A to C using "gets" for assignment **/
#define mat_tpose(AT, gets, A, n) { int i, j; for (i = 0; i < n; i++) \
  for (j = 0; j < n; j++)                                             \
    AT[i][j] gets(A[j][i]); }

/** Assign nxn matrix C the element-wise combination of A and B using "op" **/
#define mat_binop(C, gets, A, op, B, n) { int i, j; for (i = 0; i < n; i++) \
  for (j = 0; j < n; j++)                                                   \
    C[i][j] gets(A[i][j]) op(B[i][j]); }

/** Multiply the upper left 3x3 parts of A and B to get AB **/
static void mat_mult(HMatrix A, HMatrix B, HMatrix AB)
{
	int i, j;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			AB[i][j] = A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j];
}

/** Return dot product of length 3 vectors va and vb **/
static float vdot(float* va, float* vb)
{
	return (va[0] * vb[0] + va[1] * vb[1] + va[2] * vb[2]);
}

/** Set v to cross product of length 3 vectors va and vb **/
static void vcross(float* va, float* vb, float* v)
{
	v[0] = va[1] * vb[2] - va[2] * vb[1];
	v[1] = va[2] * vb[0] - va[0] * vb[2];
	v[2] = va[0] * vb[1] - va[1] * vb[0];
}

/** Set MadjT to transpose of inverse of M times determinant of M **/
static void adjoint_transpose(HMatrix M, HMatrix MadjT)
{
	vcross(M[1], M[2], MadjT[0]);
	vcross(M[2], M[0], MadjT[1]);
	vcross(M[0], M[1], MadjT[2]);
}

/******* Quaternernion Preliminaries *******/

/* Construct a (possibly non-unit) Quaternernion from real components. */
static Quatern Qt_(float x, float y, float z, float w)
{
	Quatern qq;
	qq.x = x;
	qq.y = y;
	qq.z = z;
	qq.w = w;
	return (qq);
}

/* Return conjugate of Quaternernion. */
static Quatern Qt_Conj(Quatern q)
{
	Quatern qq;
	qq.x = -q.x;
	qq.y = -q.y;
	qq.z = -q.z;
	qq.w = q.w;
	return (qq);
}

/* Return Quaternernion product qL * qR.  Note: order is important!
 * To combine rotations, use the product Mul(qSecond, qFirst),
 * which gives the effect of rotating by qFirst then qSecond. */
static Quatern Qt_Mul(Quatern qL, Quatern qR)
{
	Quatern qq;
	qq.w = qL.w * qR.w - qL.x * qR.x - qL.y * qR.y - qL.z * qR.z;
	qq.x = qL.w * qR.x + qL.x * qR.w + qL.y * qR.z - qL.z * qR.y;
	qq.y = qL.w * qR.y + qL.y * qR.w + qL.z * qR.x - qL.x * qR.z;
	qq.z = qL.w * qR.z + qL.z * qR.w + qL.x * qR.y - qL.y * qR.x;
	return (qq);
}

/* Return product of Quaternernion q by scalar w. */
static Quatern Qt_Scale(Quatern q, float w)
{
	Quatern qq;
	qq.w = q.w * w;
	qq.x = q.x * w;
	qq.y = q.y * w;
	qq.z = q.z * w;
	return (qq);
}

/* Construct a unit Quaternernion from rotation matrix.  Assumes matrix is
 * used to multiply column vector on the left: vnew = mat vold.  Works
 * correctly for right-handed coordinate system and right-handed rotations.
 * Translation and perspective components ignored. */
static Quatern Qt_FromMatrix(HMatrix mat)
{
	/* This algorithm avoids near-zero divides by looking for a large component
	 * - first w, then x, y, or z.  When the trace is greater than zero,
	 * |w| is greater than 1/2, which is as small as a largest component can be.
	 * Otherwise, the largest diagonal entry corresponds to the largest of |x|,
	 * |y|, or |z|, one of which must be larger than |w|, and at least 1/2. */
	Quatern qu;
	double tr, s;

	tr = mat[X][X] + mat[Y][Y] + mat[Z][Z];
	if (tr >= 0.0)
	{
		s = sqrt(tr + mat[W][W]);
		qu.w = s * 0.5;
		s = 0.5 / s;
		qu.x = (mat[Z][Y] - mat[Y][Z]) * s;
		qu.y = (mat[X][Z] - mat[Z][X]) * s;
		qu.z = (mat[Y][X] - mat[X][Y]) * s;
	}
	else
	{
		int h = X;
		if (mat[Y][Y] > mat[X][X]) h = Y;
		if (mat[Z][Z] > mat[h][h]) h = Z;
		switch (h)
		{
#define caseMacro(i, j, k, I, J, K)                              \
  case I:                                                        \
    s = sqrt((mat[I][I] - (mat[J][J] + mat[K][K])) + mat[W][W]); \
    qu.i = s * 0.5;                                              \
    s = 0.5 / s;                                                 \
    qu.j = (mat[I][J] + mat[J][I]) * s;                          \
    qu.k = (mat[K][I] + mat[I][K]) * s;                          \
    qu.w = (mat[K][J] - mat[J][K]) * s;                          \
    break
			caseMacro(x, y, z, X, Y, Z);
			caseMacro(y, z, x, Y, Z, X);
			caseMacro(z, x, y, Z, X, Y);
		}
	}
	if (mat[W][W] != 1.0) qu = Qt_Scale(qu, 1.0f / sqrt(mat[W][W]));
	return (qu);
}
/******* Decomp Auxiliaries *******/

static HMatrix mat_id = {
	{ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 }
};

/** Compute either the 1 or infinity norm of M, depending on tpose **/
static float mat_norm(HMatrix M, int tpose)
{
	int i;
	float sum, max;
	max = 0.0;
	for (i = 0; i < 3; i++)
	{
		if (tpose) sum = fabs(M[0][i]) + fabs(M[1][i]) + fabs(M[2][i]);
		else     sum = fabs(M[i][0]) + fabs(M[i][1]) + fabs(M[i][2]);
		if (max < sum) max = sum;
	}
	return max;
}

static float norm_inf(HMatrix M) { return mat_norm(M, 0); }
static float norm_one(HMatrix M) { return mat_norm(M, 1); }

/** Return index of column of M containing maximum abs entry, or -1 if M=0 **/
static int find_max_col(HMatrix M)
{
	float abs, max;
	int i, j, col;
	max = 0.0;
	col = -1;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
		{
			abs = M[i][j];
			if (abs < 0.0) abs = -abs;
			if (abs > max) { max = abs; col = j; }
		}
	return col;
}

/** Setup u for Household reflection to zero all v components but first **/
static void make_reflector(float* v, float* u)
{
	float s = sqrt(vdot(v, v));
	u[0] = v[0];
	u[1] = v[1];
	u[2] = v[2] + ((v[2] < 0.0) ? -s : s);
	s = sqrt(2.0 / vdot(u, u));
	u[0] = u[0] * s;
	u[1] = u[1] * s;
	u[2] = u[2] * s;
}

/** Apply Householder reflection represented by u to column vectors of M **/
static void reflect_cols(HMatrix M, float* u)
{
	int i, j;
	for (i = 0; i < 3; i++)
	{
		float s = u[0] * M[0][i] + u[1] * M[1][i] + u[2] * M[2][i];
		for (j = 0; j < 3; j++)
			M[j][i] -= u[j] * s;
	}
}
/** Apply Householder reflection represented by u to row vectors of M **/
static void reflect_rows(HMatrix M, float* u)
{
	int i, j;
	for (i = 0; i < 3; i++)
	{
		float s = vdot(u, M[i]);
		for (j = 0; j < 3; j++)
			M[i][j] -= u[j] * s;
	}
}

/** Find orthogonal factor Q of rank 1 (or less) M **/
static void do_rank1(HMatrix M, HMatrix Q)
{
	float v1[3], v2[3], s;
	int col;
	mat_copy(Q, =, mat_id, 4);
	/* If rank(M) is 1, we should find a non-zero column in M */
	col = find_max_col(M);
	if (col < 0) return; /* Rank is 0 */
	v1[0] = M[0][col];
	v1[1] = M[1][col];
	v1[2] = M[2][col];
	make_reflector(v1, v1);
	reflect_cols(M, v1);
	v2[0] = M[2][0];
	v2[1] = M[2][1];
	v2[2] = M[2][2];
	make_reflector(v2, v2);
	reflect_rows(M, v2);
	s = M[2][2];
	if (s < 0.0) Q[2][2] = -1.0;
	reflect_cols(Q, v1);
	reflect_rows(Q, v2);
}

/** Find orthogonal factor Q of rank 2 (or less) M using adjoint transpose **/
static void do_rank2(HMatrix M, HMatrix MadjT, HMatrix Q)
{
	float v1[3], v2[3];
	float w, x, y, z, c, s, d;
	int col;
	/* If rank(M) is 2, we should find a non-zero column in MadjT */
	col = find_max_col(MadjT);
	if (col < 0) { do_rank1(M, Q); return; } /* Rank<2 */
	v1[0] = MadjT[0][col];
	v1[1] = MadjT[1][col];
	v1[2] = MadjT[2][col];
	make_reflector(v1, v1);
	reflect_cols(M, v1);
	vcross(M[0], M[1], v2);
	make_reflector(v2, v2);
	reflect_rows(M, v2);
	w = M[0][0];
	x = M[0][1];
	y = M[1][0];
	z = M[1][1];
	if (w * z > x * y)
	{
		c = z + w;
		s = y - x;
		d = sqrt(c * c + s * s);
		c = c / d;
		s = s / d;
		Q[0][0] = Q[1][1] = c;
		Q[0][1] = -(Q[1][0] = s);
	}
	else
	{
		c = z - w;
		s = y + x;
		d = sqrt(c * c + s * s);
		c = c / d;
		s = s / d;
		Q[0][0] = -(Q[1][1] = c);
		Q[0][1] = Q[1][0] = s;
	}
	Q[0][2] = Q[2][0] = Q[1][2] = Q[2][1] = 0.0;
	Q[2][2] = 1.0;
	reflect_cols(Q, v1);
	reflect_rows(Q, v2);
}

/******* Polar Decomposition *******/

/* Polar Decomposition of 3x3 matrix in 4x4,
 * M = QS.  See Nicholas Higham and Robert S. Schreiber,
 * Fast Polar Decomposition of An Arbitrary Matrix,
 * Technical Report 88-942, October 1988,
 * Department of Computer Science, Cornell University.
 */
float polar_decomp(HMatrix M, HMatrix Q, HMatrix S)
{
#define TOL 1.0e-6
	HMatrix Mk, MadjTk, Ek;
	float det, M_one, M_inf, MadjT_one, MadjT_inf, E_one, gamma, g1, g2;
	int i, j;
	mat_tpose(Mk, =, M, 3);
	M_one = norm_one(Mk);
	M_inf = norm_inf(Mk);
	do
	{
		adjoint_transpose(Mk, MadjTk);
		det = vdot(Mk[0], MadjTk[0]);
		if (det == 0.0) { do_rank2(Mk, MadjTk, Mk); break; }
		MadjT_one = norm_one(MadjTk);
		MadjT_inf = norm_inf(MadjTk);
		gamma = sqrt(sqrt((MadjT_one * MadjT_inf) / (M_one * M_inf)) / fabs(det));
		g1 = gamma * 0.5;
		g2 = 0.5 / (gamma * det);
		mat_copy(Ek, =, Mk, 3);
		mat_binop(Mk, =, g1 * Mk, +, g2 * MadjTk, 3);
		mat_copy(Ek, -=, Mk, 3);
		E_one = norm_one(Ek);
		M_one = norm_one(Mk);
		M_inf = norm_inf(Mk);
	}
	while (E_one > (M_one * TOL));
	mat_tpose(Q, =, Mk, 3);
	mat_pad(Q);
	mat_mult(Mk, M, S);
	mat_pad(S);
	for (i = 0; i < 3; i++)
		for (j = i; j < 3; j++)
			S[i][j] = S[j][i] = 0.5 * (S[i][j] + S[j][i]);
	return (det);
}

/******* Spectral Decomposition *******/

/* Compute the spectral decomposition of symmetric positive semi-definite S.
 * Returns rotation in U and scale factors in result, so that if K is a diagonal
 * matrix of the scale factors, then S = U K (U transpose). Uses Jacobi method.
 * See Gene H. Golub and Charles F. Van Loan. Matrix Computations. Hopkins 1983.
 */
HVect spect_decomp(HMatrix S, HMatrix U)
{
	HVect kv;
	double Diag[3], OffD[3];  /* OffD is off-diag (by omitted index) */
	double g, h, fabsh, fabsOffDi, t, theta, c, s, tau, ta, OffDq, a, b;
	static char nxt[] = { Y, Z, X };
	int sweep, i, j;
	mat_copy(U, =, mat_id, 4);
	Diag[X] = S[X][X];
	Diag[Y] = S[Y][Y];
	Diag[Z] = S[Z][Z];
	OffD[X] = S[Y][Z];
	OffD[Y] = S[Z][X];
	OffD[Z] = S[X][Y];
	for (sweep = 20; sweep > 0; sweep--)
	{
		float sm = fabs(OffD[X]) + fabs(OffD[Y]) + fabs(OffD[Z]);
		if (sm == 0.0) break;
		for (i = Z; i >= X; i--)
		{
			int p = nxt[i];
			int q = nxt[p];
			fabsOffDi = fabs(OffD[i]);
			g = 100.0 * fabsOffDi;
			if (fabsOffDi > 0.0)
			{
				h = Diag[q] - Diag[p];
				fabsh = fabs(h);
				if (fabsh + g == fabsh)
				{
					t = OffD[i] / h;
				}
				else
				{
					theta = 0.5 * h / OffD[i];
					t = 1.0 / (fabs(theta) + sqrt(theta * theta + 1.0));
					if (theta < 0.0) t = -t;
				}
				c = 1.0 / sqrt(t * t + 1.0);
				s = t * c;
				tau = s / (c + 1.0);
				ta = t * OffD[i];
				OffD[i] = 0.0;
				Diag[p] -= ta;
				Diag[q] += ta;
				OffDq = OffD[q];
				OffD[q] -= s * (OffD[p] + tau * OffD[q]);
				OffD[p] += s * (OffDq - tau * OffD[p]);
				for (j = Z; j >= X; j--)
				{
					a = U[j][p];
					b = U[j][q];
					U[j][p] -= s * (b + tau * a);
					U[j][q] += s * (a - tau * b);
				}
			}
		}
	}
	kv.x = Diag[X];
	kv.y = Diag[Y];
	kv.z = Diag[Z];
	kv.w = 1.0;
	return (kv);
}

/******* Spectral Axis Adjustment *******/

/* Given a unit Quaternernion, q, and a scale vector, k, find a unit Quaternernion, p,
 * which permutes the axes and turns freely in the plane of duplicate scale
 * factors, such that q p has the largest possible w component, i.e. the
 * smallest possible angle. Permutes k's components to go with q p instead of q.
 * See Ken Shoemake and Tom Duff. Matrix Animation and Polar Decomposition.
 * Proceedings of Graphics Interface 1992. Details on p. 262-263.
 */
Quatern snuggle(Quatern q, HVect* k)
{
#define SQRTHALF (0.7071067811865475244f)
#define sgn(n, v)     ((n) ? -(v) : (v))
#define swap3(a, i, j) { a[3] = a[i]; a[i] = a[j]; a[j] = a[3]; }
#define cycle(a, p)   if (p) { a[3] = a[0]; a[0] = a[1]; a[1] = a[2]; a[2] = a[3]; } \
  else { a[3] = a[2]; a[2] = a[1]; a[1] = a[0]; a[0] = a[3]; }
	Quatern p;
	float ka[4];
	int i, turn = -1;
	ka[X] = k->x;
	ka[Y] = k->y;
	ka[Z] = k->z;
	if (ka[X] == ka[Y]) { if (ka[X] == ka[Z]) turn = W; else turn = Z; }
	else
	{
		if (ka[X] == ka[Z])
			turn = Y;
		else if (ka[Y] == ka[Z])
			turn = X;
	}
	if (turn >= 0)
	{
		Quatern qtoz, qp;
		unsigned neg[3], win;
		double mag[3], t;
		static Quatern qxtoz = { 0, SQRTHALF, 0, SQRTHALF };
		static Quatern qytoz = { SQRTHALF, 0, 0, SQRTHALF };
		static Quatern qppmm = { 0.5, 0.5, -0.5, -0.5 };
		static Quatern qpppp = { 0.5, 0.5, 0.5, 0.5 };
		static Quatern qmpmm = { -0.5, 0.5, -0.5, -0.5 };
		static Quatern qpppm = { 0.5, 0.5, 0.5, -0.5 };
		static Quatern q0001 = { 0.0, 0.0, 0.0, 1.0 };
		static Quatern q1000 = { 1.0, 0.0, 0.0, 0.0 };
		switch (turn)
		{
		default:
			return (Qt_Conj(q));
		case X:
			q = Qt_Mul(q, qtoz = qxtoz);
			swap3(ka, X, Z);
			break;
		case Y:
			q = Qt_Mul(q, qtoz = qytoz);
			swap3(ka, Y, Z);
			break;
		case Z:
			qtoz = q0001;
			break;
		}
		q = Qt_Conj(q);
		mag[0] = (double)q.z * q.z + (double)q.w * q.w - 0.5;
		mag[1] = (double)q.x * q.z - (double)q.y * q.w;
		mag[2] = (double)q.y * q.z + (double)q.x * q.w;
		for (i = 0; i < 3; i++)
			if (neg[i] = (mag[i] < 0.0)) mag[i] = -mag[i];
		if (mag[0] > mag[1]) { if (mag[0] > mag[2]) win = 0; else win = 2; }
		else { if (mag[1] > mag[2]) win = 1; else win = 2; }
		switch (win)
		{
		case 0:
			if (neg[0]) p = q1000; else p = q0001; break;
		case 1:
			if (neg[1]) p = qppmm; else p = qpppp; cycle(ka, 0);
			break;
		case 2:
			if (neg[2]) p = qmpmm; else p = qpppm; cycle(ka, 1);
			break;
		}
		qp = Qt_Mul(q, p);
		t = sqrt(mag[win] + 0.5);
		p = Qt_Mul(p, Qt_(0.0, 0.0, -qp.z / t, qp.w / t));
		p = Qt_Mul(qtoz, Qt_Conj(p));
	}
	else
	{
		float qa[4], pa[4];
		unsigned lo, hi, neg[4], par = 0;
		double all, big, two;
		qa[0] = q.x;
		qa[1] = q.y;
		qa[2] = q.z;
		qa[3] = q.w;
		for (i = 0; i < 4; i++)
		{
			pa[i] = 0.0;
			if (neg[i] = (qa[i] < 0.0)) qa[i] = -qa[i];
			par ^= neg[i];
		}
		/* Find two largest components, indices in hi and lo */
		if (qa[0] > qa[1]) lo = 0; else lo = 1;
		if (qa[2] > qa[3]) hi = 2; else hi = 3;
		if (qa[lo] > qa[hi])
		{
			if (qa[lo ^ 1] > qa[hi]) { hi = lo; lo ^= 1; }
			else { hi ^= lo; lo ^= hi; hi ^= lo; }
		}
		else { if (qa[hi ^ 1] > qa[lo]) lo = hi ^ 1; }
		all = (qa[0] + qa[1] + qa[2] + qa[3]) * 0.5;
		two = (qa[hi] + qa[lo]) * SQRTHALF;
		big = qa[hi];
		if (all > two)
		{
			if (all > big)/*all*/
			{
				{
					int i;
					for (i = 0; i < 4; i++)
						pa[i] = sgn(neg[i], 0.5);
				}
				cycle(ka, par)
			}
			else { /*big*/ pa[hi] = sgn(neg[hi], 1.0); }
		}
		else
		{
			if (two > big)/*two*/
			{
				pa[hi] = sgn(neg[hi], SQRTHALF);
				pa[lo] = sgn(neg[lo], SQRTHALF);
				if (lo > hi) { hi ^= lo; lo ^= hi; hi ^= lo; }
				if (hi == W) { hi = "\001\002\000"[lo]; lo = 3 - hi - lo; }
				swap3(ka, hi, lo)
			}
			else { /*big*/ pa[hi] = sgn(neg[hi], 1.0); }
		}
		p.x = -pa[0];
		p.y = -pa[1];
		p.z = -pa[2];
		p.w = pa[3];
	}
	k->x = ka[X];
	k->y = ka[Y];
	k->z = ka[Z];
	return (p);

#undef SQRTHALF
#undef sgn
#undef swap3
#undef cycle
}

/******* Decompose Affine Matrix *******/

/* Decompose 4x4 affine matrix A as TFRUK(U transpose), where t contains the
 * translation components, q contains the rotation R, u contains U, k contains
 * scale factors, and f contains the sign of the determinant.
 * Assumes A transforms column vectors in right-handed coordinates.
 * See Ken Shoemake and Tom Duff. Matrix Animation and Polar Decomposition.
 * Proceedings of Graphics Interface 1992.
 */
static void decomp_affine(HMatrix A, SAffineParts* parts)
{
	HMatrix Q, S, U;
	Quatern p;
	float det;
	parts->t = Qt_(A[X][W], A[Y][W], A[Z][W], 0);
	det = polar_decomp(A, Q, S);
	if (det < 0.0)
	{
		mat_copy(Q, =, -Q, 3);
		parts->f = -1;
	}
	else parts->f = 1;
	parts->q = Qt_FromMatrix(Q);
	parts->k = spect_decomp(S, U);
	parts->u = Qt_FromMatrix(U);
	p = snuggle(parts->u, &parts->k);
	parts->u = Qt_Mul(parts->u, p);
}

static void spectral_decomp_affine(HMatrix A, SAffineParts* parts)
{
	HMatrix Q, S, U;
	float det;

	parts->t = Qt_(A[X][W], A[Y][W], A[Z][W], 0);
	det = polar_decomp(A, Q, S);
	if (det < 0.0)
	{
		mat_copy(Q, =, -Q, 3);
		parts->f = -1;
	}
	else parts->f = 1;
	parts->q = Qt_FromMatrix(Q);
	parts->k = spect_decomp(S, U);
	parts->u = Qt_FromMatrix(U);
}

/******* Invert Affine Decomposition *******/

/* Compute inverse of affine decomposition.
 */
static void invert_affine(SAffineParts* parts, SAffineParts* inverse)
{
	Quatern t, p;
	inverse->f = parts->f;
	inverse->q = Qt_Conj(parts->q);
	inverse->u = Qt_Mul(parts->q, parts->u);
	inverse->k.x = (parts->k.x == 0.0) ? 0.0 : 1.0 / parts->k.x;
	inverse->k.y = (parts->k.y == 0.0) ? 0.0 : 1.0 / parts->k.y;
	inverse->k.z = (parts->k.z == 0.0) ? 0.0 : 1.0 / parts->k.z;
	inverse->k.w = parts->k.w;
	t = Qt_(-parts->t.x, -parts->t.y, -parts->t.z, 0);
	t = Qt_Mul(Qt_Conj(inverse->u), Qt_Mul(t, inverse->u));
	t = Qt_(inverse->k.x * t.x, inverse->k.y * t.y, inverse->k.z * t.z, 0);
	p = Qt_Mul(inverse->q, inverse->u);
	t = Qt_Mul(p, Qt_Mul(t, Qt_Conj(p)));
	inverse->t = (inverse->f > 0.0) ? t : Qt_(-t.x, -t.y, -t.z, 0);
}

/*
   void  AffineParts::invert() {
   SAffineParts parts,inverse;

   parts.q.w = q.w; parts.q.x = q.x; parts.q.y = q.y; parts.q.z = q.z;
   //parts.u.w = u.w; parts.u.x = u.x; parts.u.y = u.y; parts.u.z = u.z;
   parts.u.w = 0; parts.u.x = 1; parts.u.y = 0; parts.u.z = 0;
   parts.t.x = t.x; parts.t.y = t.y; parts.t.z = t.z;
   parts.k.x = k.x; parts.k.y = k.y; parts.k.z = k.z;
   //parts.f = f;
   parts.f = 1;

   invert_affine( &parts,&inverse );

   q.set( inverse.q.w,inverse.q.x,inverse.q.y,inverse.q.z );
   //  u.set( inverse.u.w,inverse.u.x,inverse.u.y,inverse.u.z );
   t.set( inverse.t.x,inverse.t.y,inverse.t.z );
   k.set( inverse.k.x,inverse.k.y,inverse.k.z );
   //  f = inverse.f;
   }
 */

// Decompose matrix to affine parts.
void AffineParts::Decompose(const Matrix34& tm)
{
	SAffineParts parts;

	Matrix44 tm44(tm);
	HMatrix& H = *((HMatrix*)&tm44); // Treat HMatrix as a Matrix44.

	decomp_affine(H, &parts);

	rot = Quat(parts.q.w, parts.q.x, parts.q.y, parts.q.z);
	rotScale = Quat(parts.u.w, parts.u.x, parts.u.y, parts.u.z);
	pos = Vec3(parts.t.x, parts.t.y, parts.t.z);
	scale = Vec3(parts.k.x, parts.k.y, parts.k.z);
	fDet = parts.f;
}

// Spectral matrix decompostion to affine parts.
void AffineParts::SpectralDecompose(const Matrix34& tm)
{
	SAffineParts parts;

	Matrix44 tm44(tm);
	HMatrix& H = *((HMatrix*)&tm44); // Treat HMatrix as a Matrix44.

	spectral_decomp_affine(H, &parts);

	rot = Quat(parts.q.w, parts.q.x, parts.q.y, parts.q.z);
	rotScale = Quat(parts.u.w, parts.u.x, parts.u.y, parts.u.z);
	pos = Vec3(parts.t.x, parts.t.y, parts.t.z);
	scale = Vec3(parts.k.x, parts.k.y, parts.k.z);
	fDet = parts.f;
}

