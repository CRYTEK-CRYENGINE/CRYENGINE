// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AttachmentProxy.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "AttachmentManager.h"
#include "CharacterInstance.h"

uint32 CProxy::SetJointName(const char* szJointName)
{
	m_nJointID = -1;
	if (!szJointName)  { assert(0); return 0; }
	int nJointID = m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetJointIDByName(szJointName);
	if (nJointID < 0)
	{
		// this is a severe bug if the bone name is invalid and someone tries to attach something
		g_pILog->LogError("SetJointName is called for bone \"%s\", which is not in the model \"%s\". Ignoring, but this may cause a crash because the corresponding object won't be detached after it's destroyed", szJointName, m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetModelFilePath());
		return 0;
	}
	m_strJointName = szJointName;
	ProjectProxy();
	return 1;
};

uint32 CProxy::ProjectProxy()
{
	m_nJointID = -1;
	int32 JointId = m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetJointIDByName(m_strJointName.c_str());
	if (JointId < 0)
		return 0;
	CCharInstance* pSkelInstance = m_pAttachmentManager->m_pSkelInstance;
	int nJointCount = pSkelInstance->m_pDefaultSkeleton->GetJointCount();
	if (JointId >= nJointCount)
		return 0;

	m_nJointID = JointId;
	const CDefaultSkeleton* pDefaultSkeleton = pSkelInstance->m_pDefaultSkeleton;
	QuatT jointQT = pDefaultSkeleton->GetDefaultAbsJointByID(m_nJointID);
	m_ProxyRelativeDefault = jointQT.GetInverted() * m_ProxyAbsoluteDefault;
	return 1;
}

void CProxy::AlignProxyWithJoint()
{
	CCharInstance* pSkelInstance = m_pAttachmentManager->m_pSkelInstance;
	int32 nJointCount = pSkelInstance->m_pDefaultSkeleton->GetJointCount();
	if (m_nJointID < nJointCount)
	{
		m_ProxyAbsoluteDefault = pSkelInstance->m_pDefaultSkeleton->GetDefaultAbsJointByID(m_nJointID);
		m_ProxyRelativeDefault.SetIdentity();
	}
}

#define HPLANEEPSILON (0.000001f)
#define MAXRADEPSILON (1000.0f)
#define HXEPSILON     (0.0001f)

ILINE f32 fclamp01_tpl(f32 x) { return x < 0.0f ? 0.0f : x < 1.0f ? x : 1.0f; }
ILINE f64 fclamp01_tpl(f64 x) { return x < 0.0 ? 0.0 : x < 1.0 ? x : 1.0;  }

ILINE f32 fispos(f32 x)       { return x > 0 ? 1.0f : 0.0f; }
ILINE f64 fispos(f64 x)       { return x > 0 ? 1.0 : 0.0; }

ILINE f32 fisneg(f32 x)       { return x < 0 ? 1.0f : 0.0f; }
ILINE f64 fisneg(f64 x)       { return x < 0 ? 1.0 : 0.0; }

//returns the closest-distance of a point to the surface of a lozenge
f32 CProxy::GetDistance(const Vec3& p) const
{
	f32 sq = 0, t;
	t = fabs_tpl(p.x) - m_params.x;
	if (t > 0) sq += t * t;
	t = fabs_tpl(p.y) - m_params.y;
	if (t > 0) sq += t * t;
	t = fabs_tpl(p.z) - m_params.z;
	if (t > 0) sq += t * t;
	return sqrt_tpl(sq) - m_params.w;
};

//returns the closest-distance of a sphere to the surface of a lozenge
f32 CProxy::GetDistance(const Vec3& p, f32 r) const
{
	f32 sq = 0, t;
	Vec4 loz = m_params;
	loz.w += r;
	t = fabs(p.x) - loz.x;
	if (t > 0) sq += t * t;
	t = fabs(p.y) - loz.y;
	if (t > 0) sq += t * t;
	t = fabs(p.z) - loz.z;
	if (t > 0) sq += t * t;
	return sqrt_tpl(sq) - loz.w;
};

//checks if a capsule and a lozenge overlap and returns a "pseudo" square-distance
f32 CProxy::TestOverlapping(const Vec3& p0, Vec3 dir, f32 sl, f32 sr) const
{
	Vec3 p1 = p0 + dir * sl, m = (p1 + p0) * 0.5f;
	Vec4 loz = m_params;
	loz.w += sr;
	if (loz.x + loz.y + loz.z == 0)
	{
		Vec3 ab = p1 - p0;
		f32 e = -p0 | ab, f = ab | ab;
		if (e <= 0) return (p0 | p0) - loz.w * loz.w;
		if (e >= f) return (p1 | p1) - loz.w * loz.w;
		return (p0 | p0) - e * e / f - loz.w * loz.w;
	}
	if (dir.x < 0) dir.x = -dir.x, m.x = -m.x;
	if (dir.y < 0) dir.y = -dir.y, m.y = -m.y;
	if (dir.z < 0) dir.z = -dir.z, m.z = -m.z;
	f32 lext = sl * 0.5f;
	struct
	{
		f32 CheckEdge(const Vec3& p0, const Vec3& p1, const Vec4& loz)
		{
			f32 p0x = fabs(p0.x) - loz.x, p0y = fabs(p0.y) - loz.y, p0z = fabs(p0.z) - loz.z;
			f32 p1x = fabs(p1.x) - loz.x, p1y = fabs(p1.y) - loz.y, p1z = fabs(p1.z) - loz.z;
			p0x = max(0.0f, p0x), p0y = max(0.0f, p0y), p0z = max(0.0f, p0z);
			p1x = max(0.0f, p1x), p1y = max(0.0f, p1y), p1z = max(0.0f, p1z);
			return min(p0x * p0x + p0y * p0y + p0z * p0z, p1x * p1x + p1y * p1y + p1z * p1z) - loz.w * loz.w;
		}
	} e;
	if (dir.x == 0 && dir.y == 0)
		return fabs((loz.z - m.z) / dir.z) > lext ? e.CheckEdge(p0, p1, loz) : sqr(max(0.0f, fabsf(m.x) - loz.x)) + sqr(max(0.0f, fabsf(m.y) - loz.y)) - loz.w * loz.w;
	if (dir.x == 0 && dir.z == 0)
		return fabs((loz.y - m.y) / dir.y) > lext ? e.CheckEdge(p0, p1, loz) : sqr(max(0.0f, fabsf(m.x) - loz.x)) + sqr(max(0.0f, fabsf(m.z) - loz.z)) - loz.w * loz.w;
	if (dir.y == 0 && dir.z == 0)
		return fabs((loz.x - m.x) / dir.x) > lext ? e.CheckEdge(p0, p1, loz) : sqr(max(0.0f, fabsf(m.y) - loz.y)) + sqr(max(0.0f, fabsf(m.z) - loz.z)) - loz.w * loz.w;
	Vec3 PmE(m.x - loz.x, m.y - loz.y, m.z - loz.z);
	int p, i0 = 2, i1 = 0, i2 = 1;
	if (dir.x * PmE.y <= dir.y * PmE.x && dir.x * PmE.z <= dir.z * PmE.x) i0 = 0, i1 = 1, i2 = 2;
	if (dir.x * PmE.y >= dir.y * PmE.x && dir.y * PmE.z <= dir.z * PmE.y) i0 = 1, i1 = 2, i2 = 0;
	if (dir[i0] && dir[i0] * (m[i1] + loz[i1]) >= dir[i1] * PmE[i0] && dir[i0] * (m[i2] + loz[i2]) >= dir[i2] * PmE[i0])
		return fabs(PmE[i0] / dir[i0]) > lext ? e.CheckEdge(p0, p1, loz) : -loz.w * loz.w;
	for (uint32 i = 0; i < 2; p = i1, i1 = i2, i2 = p, i++)
	{
		f32 pE1 = m[i1] + loz[i1], pE2 = m[i2] + loz[i2], bE1 = loz[i1], bE2 = loz[i2];
		f32 s = dir[i0] * dir[i0] + dir[i2] * dir[i2], x = s * pE1 - dir[i1] * (dir[i0] * PmE[i0] + dir[i2] * pE2);
		if (dir[i0] * pE1 >= dir[i1] * PmE[i0] || x >= 0)
		{
			s == 0 ? x = x <= s * bE1 * 2 ? pE1 - x * FLT_MAX : PmE[i1] : x = x <= s * bE1 * 2 ? pE1 - x / s : PmE[i1];
			f32 d = dir[i0] * PmE[i0] + dir[i1] * x + dir[i2] * pE2, t = d / (s + dir[i1] * dir[i1]);
			return fabs(t) > lext ? e.CheckEdge(p0, p1, loz) : PmE[i0] * PmE[i0] + x * x + pE2 * pE2 - d * t - loz.w * loz.w;
		}
	}
	f32 pE1 = m[i1] + loz[i1], pE2 = m[i2] + loz[i2];
	f32 d = dir[i0] * PmE[i0] + dir[i2] * pE2 + dir[i1] * pE1, t = d / (dir | dir);
	return (fabs(t) > lext) ? e.CheckEdge(p0, p1, loz) : PmE[i0] * PmE[i0] + sqr(pE2) + sqr(pE1) - d * t - loz.w * loz.w;
};

//Projects a penetrating object out of a lozenge and back to its surface by using the smallest translation possible.
Vec3 CProxy::ShortvecTranslationalProjection(const Vec3& ipos, f32 sr) const
{
	f32 fDistPointLozenge = GetDistance(ipos, sr);
	if (fDistPointLozenge >= 0)
		return Vec3(ZERO);
	Vec4 loz = m_params;
	loz.w += sr;
	if (loz.x + loz.y + loz.z == 0)
	{
		if (ipos | ipos) return ipos * isqrt_tpl(ipos | ipos) * loz.w - ipos;
		return Vec3(loz.w, 0, 0);
	}
	struct
	{
		Vec3 CheckEdge(const Vec3& ipos, f32 ca, f32 cr)
		{
			f32 t = ipos.x < 0 ? -1.0f : 1.0f;
			if (fabs(ipos.x) > ca)
			{
				Vec3 tmp = ipos;
				tmp.x -= ca * t;
				if (tmp | tmp) return tmp * isqrt_tpl(tmp | tmp) * cr + Vec3(ca * t, 0, 0);
				return Vec3(cr * t, 0, 0);
			}
			Vec3 tmp = Vec3(0, ipos.y, ipos.z);
			if (tmp | tmp) return tmp * isqrt_tpl(tmp | tmp) * cr + Vec3(ipos.x, 0, 0);
			return Vec3((cr + ca) * t, 0, 0);
		}
	} e;
	const Matrix33 m[2] = { Matrix33(0, 1, 0, -1, 0, 0, 0, 0, 1), Matrix33(0, 0, 1, 0, 1, 0, -1, 0, 0) };
	if (loz.y + loz.z == 0)
		return e.CheckEdge(ipos, loz.x, loz.w) - ipos;
	if (loz.x + loz.z == 0)
		return e.CheckEdge(Vec3(ipos.y, -ipos.x, ipos.z), loz.y, loz.w) * m[0] - ipos;
	if (loz.x + loz.y == 0)
		return e.CheckEdge(Vec3(ipos.z, ipos.y, -ipos.x), loz.z, loz.w) * m[1] - ipos;
	uint32 i = 3;
	f32 x = FLT_MAX;
	Vec3 tmp, abspos = Vec3(fabs(ipos.x), fabs(ipos.y), fabs(ipos.z));
	f32 distx = loz.w + loz.x - abspos.x;
	if (x > distx && abspos.y <= loz.y && abspos.z <= loz.z)
		x = distx, i = 0, tmp.x = (loz.w + loz.x) * (ipos.x < 0 ? -1 : 1), tmp.y = ipos.y, tmp.z = ipos.z;
	f32 disty = loz.w + loz.y - abspos.y;
	if (x > disty && abspos.x <= loz.x && abspos.z <= loz.z)
		x = disty, i = 1, tmp.x = ipos.x, tmp.y = (loz.w + loz.y) * (ipos.y < 0 ? -1 : 1), tmp.z = ipos.z;
	f32 distz = loz.w + loz.z - abspos.z;
	if (x > distz && abspos.x <= loz.x && abspos.y <= loz.y)
		x = distz, i = 2, tmp.x = ipos.x, tmp.y = ipos.y, tmp.z = (loz.w + loz.z) * (ipos.z < 0 ? -1.0f : 1.0f);
	if (i < 3) return tmp - ipos;
	f32 tx = (ipos.x < 0 ? -1.0f : 1.0f), ty = (ipos.y < 0 ? -1.0f : 1.0f), tz = (ipos.z < 0 ? -1.0f : 1.0f);
	if (abspos.y >= loz.y && abspos.x >= loz.x)
	{
		tmp = e.CheckEdge(Vec3(ipos.z, ipos.y - loz.y * ty, -ipos.x + loz.x * tx), loz.z, loz.w);
		return Vec3(-tmp.z + loz.x * tx, tmp.y + loz.y * ty, tmp.x) - ipos;
	}
	if (abspos.y >= loz.y && abspos.z >= loz.z)
	{
		tmp = e.CheckEdge(Vec3(ipos.x, ipos.y - loz.y * ty, ipos.z - loz.z * tz), loz.x, loz.w), tmp.y += loz.y * ty, tmp.z += loz.z * tz;
		return tmp - ipos;
	}
	if (abspos.z >= loz.z && abspos.x >= loz.x)
	{
		tmp = e.CheckEdge(Vec3(ipos.y, -ipos.x + loz.x * tx, ipos.z - loz.z * tz), loz.y, loz.w);
		return Vec3(-tmp.y + loz.x * tx, tmp.x, tmp.z + loz.z * tz) - ipos;
	}
	return Vec3(ZERO);
}

//Projects a penetrating object out of a lozenge and back to its surface by using the predefined translation direction.
f32 CProxy::DirectedTranslationalProjection(const Vec3& ipos, const Vec3& idir, f32 sl, f32 sr) const
{
	Diag33 r(idir.x < 0 ? -1.0f : 1.0f, idir.y < 0 ? -1.0f : 1.0f, idir.z < 0 ? -1.0f : 1.0f);
	Vec3 rp = ipos * r, rd = idir * r, l, v;
	Vec4 loz = m_params;
	loz.w += sr;
	f32 sl2 = sl + sl, t;
	const Matrix33 m[4] = { Matrix33(0, 1, 0, -1, 0, 0, 0, 0, 1), Matrix33(1, 0, 0, 0, 1, 0, 0, 0, 1), Matrix33(1, 0, 0, 0, 0, -1, 0, 1, 0) };
	struct
	{
		f32 CheckEdge(const Vec3& ipos, const Vec3& idir, f32 ca, f32 cr)
		{
			Vec3 ns = ipos, ps = ipos;
			ns.x += ca, ps.x -= ca;
			f32 a = ca + ca, b = a * a, c = a * ns.x, d = a * idir.x, e = idir | idir, f = ns | idir;
			f32 g = ((ns | ns) - cr * cr) * b - c * c, h = b * e - d * d;
			if (h)
			{
				f32 x = b * f - d * c, discr = x * x - h * g;
				if (discr < 0) return 0;
				f32 sq = sqrt(discr), t = (-x - sq) / h;
				if (c + t * d > b) return CheckCorner(ps, idir, cr);
				if (c + t * d < 0) return CheckCorner(ns, idir, cr);
				return t * fisneg(t - (sq - x) / h);
			}
			if (g > 0) return 0;
			return CheckCorner(ps, idir, cr) * fisneg(idir.x) + CheckCorner(ns, idir, cr) * fispos(idir.x);
		}
		f32 CheckCorner(Vec3 p, Vec3 dir, f32 r)
		{
			f32 b = (p | dir), d = b * b + r * r - (p | p);
			if (d < 0)  return 0;
			return -b - sqrt_tpl(d);
		}
	} e;
	if (loz.x + loz.y + loz.z == 0)
	{
		t = e.CheckCorner(ipos, idir, loz.w);
		return (t && t < sl2) ? sl2 - t : 0;
	}
	if (loz.y + loz.z == 0)
	{
		t = e.CheckEdge(ipos, idir, loz.x, loz.w);
		return (t && t < sl2) ? sl2 - t : 0;
	}
	if (loz.x + loz.z == 0)
	{
		t = e.CheckEdge(Vec3(ipos.y, -ipos.x, ipos.z), Vec3(idir.y, -idir.x, idir.z), loz.y, loz.w);
		return (t && t < sl2) ? sl2 - t : 0;
	}
	if (loz.x + loz.y == 0)
	{
		t = e.CheckEdge(Vec3(ipos.z, ipos.y, -ipos.x), Vec3(idir.z, idir.y, -idir.x), loz.z, loz.w);
		return (t && t < sl2) ? sl2 - t : 0;
	}
	for (uint32 axis = 0; axis < 3; axis++)
	{
		if (axis == 0)  l(loz.y, loz.x, loz.z);
		if (axis == 1)  l(loz.x, loz.y, loz.z);
		if (axis == 2)  l(loz.x, loz.z, loz.y);
		Vec3 p = rp * m[axis], d = rd * m[axis];
		if (d.y == 0)
			continue;
		f32 maxx = l.x + loz.w, maxy = l.y + loz.w, maxz = l.z + loz.w, c = (-p.y - maxy) / d.y;
		Vec3 spnt(d.x * c + p.x, -maxy, d.z * c + p.z);

		if (fabs(spnt.z) < l.z && fabs(spnt.x) < l.x)
		{
			Vec3 spntp = spnt - p;
			f32 len = spntp.GetLength(), dot = d | spntp;
			if (dot >= 0 && sl2 > len)
				return sl2 - len;
			if (dot < 0)
				return sl2 + len;
		}
		if (l.z && spnt.x >= l.x && (t = e.CheckEdge(Vec3(p.z, p.y + l.y, -p.x + l.x), Vec3(d.z, d.y, -d.x), l.z, loz.w)))
		{
			v = d * t + p;
			if (fabs(v.z) <= maxz && (!l.x || v.x >= l.x) && (!l.y || v.y <= -l.y)) return (t && t < sl2) ? sl2 - t : 0;
		}
		if (l.x && spnt.z >= l.z && (t = e.CheckEdge(Vec3(p.x, p.y + l.y, p.z - l.z), d, l.x, loz.w)))
		{
			v = d * t + p;
			if (fabs(v.x) <= maxx && (!l.z || v.z >= l.z) && (!l.y || v.y <= -l.y)) return (t && t < sl2) ? sl2 - t : 0;
		}
		if (l.z && spnt.x <= -l.x && (t = e.CheckEdge(Vec3(p.z, p.y + l.y, -p.x - l.x), Vec3(d.z, d.y, -d.x), l.z, loz.w)))
		{
			v = d * t + p;
			if (fabs(v.z) <= maxz && (!l.y || v.y <= -l.y) && (!l.x || v.x <= -l.x)) return (t && t < sl2) ? sl2 - t : 0;
		}
		if (l.x && spnt.z <= -l.z && (t = e.CheckEdge(Vec3(p.x, p.y + l.y, p.z + l.z), d, l.x, loz.w)))
		{
			v = d * t + p;
			if (fabs(v.x) <= maxx && (!l.y || v.y <= -l.y) && (!l.z || v.z <= -l.z)) return (t && t < sl2) ? sl2 - t : 0;
		}
	}
	return 0;
}

//Projects a penetrating object out of a lozenge and back to its surface by using the smallest rotation possible.
Vec3 CProxy::ShortarcRotationalProjection(const Vec3& ipos, const Vec3& idir, f32 sl, f32 lr) const
{
	Vec4 loz = m_params;
	loz.w += lr;
	if (loz.x + loz.y + loz.z == 0)
	{
		Vec3 pnt = ipos - idir * (ipos | idir), iha = (pnt % idir).GetNormalized();
		f32 pn = iha.x < 0 ? -1.0f : 1.0f, vx = pn * iha.x, vy = pn * iha.y, vz = pn * iha.z, w = 1.0f / (1.0f + vx);
		Matrix33 m(vx, -vy, -vz, vy, w * vz * vz + vx, -w * vz * vy, vz, -w * vz * vy, w * vy * vy + vx);
		Vec3 ip(ipos.x, ipos.y, ipos.z), np = ip * m;
		f32 x = ip | ip, cr2 = loz.w * loz.w, c0 = x - cr2 - cr2, cy = np.y, cz = np.z, d2, id, a, h, b, s2 = (1.0f - sqr(np.x / loz.w)) * cr2;
		if (sl * sl - sqr(x - cr2) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
			d2 = cy * cy + cz * cz, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, h = sqrt_tpl(max(0.0f, s2 - a * a)) * id * pn;
		else
			cy *= 0.5f, cz *= 0.5f, id = isqrt_tpl(cy * cy + cz * cz), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(s2 - a * a) * id * pn;
		return m * Vec3(np.x, cy * b - cz * h, cz * b + cy * h) - ipos;
	}

	struct
	{
		Vec3 CheckCapsule(const Vec3& pos, const Vec3& dir, f32 sl, f32 ca, f32 cr)
		{
			f32 rr = cr * cr, flip = 1;
			for (f32 t = -1; t < 1.5f; t += 2)
			{
				Vec3 ip(pos.x - ca * t, pos.y, pos.z), pnt = ip - dir * (ip | dir);
				if ((pnt | pnt) > rr) continue;
				if (sqr(sl + cr) < (ip | ip)) continue;
				Vec3 iha = (pnt % dir).GetNormalized();
				f32 pn = iha.x < 0 ? -1.0f : 1.0f, vx = pn * iha.x, vy = pn * iha.y, vz = pn * iha.z, w = 1.0f / (1.0f + vx);
				Matrix33 m(vx, -vy, -vz, vy, w * vz * vz + vx, -w * vz * vy, vz, -w * vz * vy, w * vy * vy + vx);
				Vec3 np = ip * m;
				f32 sine = 1.0f - sqr(np.x / cr);
				if (sine < 0) continue;
				f32 id, a, b, h, x = ip | ip, c0 = x - rr - rr, s2 = sine * rr, d = 0, d2 = np.y * np.y + np.z * np.z;
				if (d2 == 0) continue;
				if (sl * sl - sqr(x - rr) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
					id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, d = s2 - a * a, h = sqrt_tpl(fabs(d)) * id * pn;
				else
					np.y *= 0.5f, np.z *= 0.5f, id = isqrt_tpl(np.y * np.y + np.z * np.z), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(s2 - a * a) * id * pn;
				if (d < 0) continue;
				Vec3 tmp = m * Vec3(np.x, np.y * b - np.z * h, np.z * b + np.y * h);
				if (tmp.x * t > 0)
					return Vec3(tmp.x + ca * t, tmp.y, tmp.z);
			}
			f32 aa = pos.y * pos.y + pos.z * pos.z, a = sqrt_tpl(aa), A = 0.5f / a;
			f32 l = 1.0f / a, f = pos.y < 0 ? -l : l, fcos = f * pos.y, fsin = f * pos.z;
			f32 diry = dir.y * fcos + dir.z * fsin, dirz = dir.z * fcos - dir.y * fsin;
			if (pos.y * fcos + pos.z * fsin >= 0)
				diry = -diry, dirz = -dirz, flip = -1;
			f32 rz = dirz < 0 ? -1.0f : 1.0f, cy = pos.y * 0.5f, cz = pos.z * 0.5f;
			f32 id = isqrt_tpl(cy * cy + cz * cz), qa = rr * id * 0.5f, b = qa * id, h = sqrt_tpl(fabs(rr - qa * qa)) * id * rz;
			f32 ty = cy * b + cz * h, tz = cz * b - cy * h, hx = ty * dir.z - tz * dir.y, hy = tz * dir.x, hz = -ty * dir.x;
			f32 tx = (pos.x * hx + pos.y * hy + pos.z * hz - hy * ty - hz * tz) / hx;
			if (sqr(Vec3(tx - pos.x, ty - pos.y, tz - pos.z)) < sl * sl)
				return Vec3(tx, ty, tz);
			f32 ycut = (ty * fcos + tz * fsin) * flip, RR = sl * sl, rar = RR - aa - rr, C = A * rar;
			f32 minX = C > ycut ? sqrt_tpl(rar - 2 * a * ycut) : 0;
			f32 maxX = sqrt_tpl(2 * a * cr + rar) - eps - eps;
			for (uint32 limit = 0; rr - sqr(C - A * maxX * maxX) < eps && limit < 256; maxX -= eps, limit++)
				;
			f32 dx = fabs(dir.x), dy = diry, dz = fabs(dirz);
			f32 x = (minX + maxX) * 0.5f, y, z, iz, yiz, d1, d2, rmz, e0 = 2 * A * dy, e1 = 2 * A * dz, e2 = 4 * A * A, e3 = e2 * dz, delta = 9;
			y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz, d1 = (e0 - e1 * yiz) * x - dx, d2 = (e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0;
			x = clamp_tpl(x + d1 / d2, minX, minX + (maxX - minX) * 0.980f);
			y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz, d1 = (e0 - e1 * yiz) * x - dx, d2 = (e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0;
			x = clamp_tpl(x + d1 / d2, minX, minX + (maxX - minX) * 0.995f);
			if (x < minX + (maxX - minX) * 0.96)
			{
				for (i = 9; fabs(delta) > eps && i; x = clamp_tpl(x - delta, minX, maxX), --i)
				{
					y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz;
					d1 = dx - (e0 - e1 * yiz) * x, d2 = (e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0, delta = d1 / d2;
				}
				y = (C - A * x * x), z = sqrt_tpl(rr - y * y) * rz;
			}
			else
			{
				f32 maxZ = sqrt_tpl(rr - sqr(C - A * minX * minX)), e4, zz, o;
				for (i = 9, z = sqrt_tpl(rr - sqr(C - A * x * x)); fabs(delta) > eps && i; z = clamp_tpl(z - delta, 0.0f, maxZ), --i)
				{
					zz = z * z, rmz = rr - zz, iz = isqrt_tpl(rmz), o = isqrt_tpl((1.0f / iz + C) / A), e4 = 0.5f / A * o * iz;
					d1 = z * e4 * dx - dz - z * iz * dy, d2 = (zz / rmz + 1) * (e4 * dx - iz * dy) + (dx * zz * o * o * o) / (e2 * rmz), delta = d1 / d2;
				}
				y = sqrt_tpl(rr - z * z), x = sqrt_tpl((y + C) / A), y = -y, z *= rz;
			}
			return Vec3(x * (dir.x < 0 ? -1.0f : 1.0f) + pos.x, (fcos * y - fsin * z) * flip, (fsin * y + fcos * z) * flip);
		}
		Vec3 CheckEdge(const Vec3& rpos, const Vec3& rdir, f32 sl, f32 lr, f32 ca, f32 cr)
		{
			rr = cr * cr;
			for (f32 t = -1; t < 1.5f; t += 2)
			{
				Vec3 ip(rpos.x - ca * t, rpos.y, rpos.z), pnt = ip - rdir * (ip | rdir);
				if (sqr(sl + cr) < (ip | ip)) continue;
				f32 dot = pnt | pnt;
				if (dot > rr) continue;
				Vec3 iha = dot ? (pnt % rdir).GetNormalized() : rdir.GetOrthogonal().GetNormalized() * (rpos.x > 0 ? -1.0f : 1.0f);
				f32 pn = iha.x < 0 ? -1.0f : 1.0f, vx = pn * iha.x, vy = pn * iha.y, vz = pn * iha.z, w = 1.0f / (1.0f + vx);
				Matrix33 m(vx, -vy, -vz, vy, w * vz * vz + vx, -w * vz * vy, vz, -w * vz * vy, w * vy * vy + vx);
				Vec3 np = ip * m;
				f32 sine = 1.0f - sqr(np.x / cr);
				if (sine < 0) continue;
				f32 d2, id, a, b, h, x = ip | ip, c0 = x - rr - rr, s2 = sine * rr, d = 0;
				if (sl * sl - sqr(x - rr) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
					d2 = np.y * np.y + np.z * np.z, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, d = s2 - a * a, h = sqrt_tpl(fabs(d)) * id * pn;
				else
					np.y *= 0.5f, np.z *= 0.5f, id = isqrt_tpl(np.y * np.y + np.z * np.z), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(s2 - a * a) * id * pn;
				if (d < 0) continue;
				Vec3 tmp = m * Vec3(np.x, np.y * b - np.z * h, np.z * b + np.y * h);
				if (tmp.x * t > 0 && tmp.y <= 0 && tmp.z >= 0)
					return Vec3(tmp.x + ca * t, tmp.y, tmp.z);
			}
			if (rpos.y * rpos.y + rpos.z * rpos.z < rr)
				return rpos - rdir * sl;
			aa = rpos.y * rpos.y + rpos.z * rpos.z, a = sqrt_tpl(aa);
			l = 1.0f / a, f = rpos.y < 0 ? -l : l, fcos = f * rpos.y, fsin = f * rpos.z;
			idy = fcos * rdir.y + fsin * rdir.z, idz = fcos * rdir.z - fsin * rdir.y, rx = rdir.x < 0 ? -1.0f : 1.0f;
			cy = rpos.y * 0.5f, cz = rpos.z * 0.5f, id = isqrt_tpl(cy * cy + cz * cz);
			cid = rr * id * 0.5f, b = cid * id, h = sqrt_tpl(fabs(rr - cid * cid)) * id;
			int gvisible = 1, rvisible = 1;
			if (fsin >= 0) idz < 0 ? (gvisible = 0, rvisible = 1) : (gvisible = 1, rvisible = 0);
			Vec3 vContactPoint = rpos - rdir * sl;
			if (gvisible && CheckHalfCapsule(rpos, rdir, sl, lr, ca, cr, vContactPoint))
				return vContactPoint;
			if (rvisible)
				return CheckHalfCapsule(rpos, rdir, sl, lr, ca, cr);
			return vContactPoint;
		}
		int CheckHalfCapsule(const Vec3& rpos, const Vec3& rdir, const f32 sl, const f32 lr, const f32 ca, const f32 cr, Vec3& vContactPoint)
		{
			const f32 rz = idz < 0 ? -1.0f : 1.0f;
			const f32 gh = h * rz;
			// cy, cz is (rpos.y, rpos.z) * 0.5
			const f32 ty = cy * b + cz * gh;
			const f32 tz = cz * b - cy * gh;
			// cross product ( hx, hy, hz ) = ( 0, ty, tz ) x ( rdir.x, rdir.y, rdir.z )
			const f32 hx = ty * rdir.z - tz * rdir.y;
			const f32 hy = tz * rdir.x;
			const f32 hz = -ty * rdir.x;
			// solve (hx, hy, hz) * ((rpos.x, rpos.y, rpos.z) - (tx, ty, tz)) = 0 for tx
			if (fabs(hx) < HXEPSILON)
				return 0;
			const f32 tx = (rpos.x * hx + rpos.y * hy + rpos.z * hz - hy * ty - hz * tz) / hx;
			if (sqr(Vec3(tx - rpos.x, ty - rpos.y, tz - rpos.z)) < sl * sl)
			{
				if (fabs(tx) > ca || ty > 0 || tz < 0) return 0;
				vContactPoint = Vec3(tx, ty, tz);
				return 1;
			}
			f32 ycut = ty * fcos + tz * fsin, RR = sl * sl, rar = RR - aa - rr, A = 0.5f / a, C = A * rar;
			f32 minX = C > ycut ? sqrt_tpl(rar - 2 * a * ycut) : 0, rminX = rx * minX + rpos.x;
			f32 maxX = sqrt_tpl(2 * a * cr + rar) - eps - eps, rmaxX = rx * maxX + rpos.x;
			if ((rminX > ca && rmaxX > ca) || (rminX < -ca && rmaxX < -ca))
				return 0;
			for (uint32 limit = 0; rr - sqr(C - A * maxX * maxX) < eps && limit < 256; maxX -= eps, limit++)
				;
			f32 dx = fabs(rdir.x), dy = fabs(idy), dz = idz * rz;
			f32 x = (minX + maxX) * 0.5f, y, z, zz, d1, d2, iz, yiz, o, e0 = 2 * A * dy, e1 = 2 * A * dz, e2, e3 = 4 * A * A * dz, dist = 9;
			y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz, d1 = (e0 - e1 * yiz) * x - dx, d2 = (e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0;
			x = clamp_tpl(x + d1 / d2, minX, minX + (maxX - minX) * 0.980f);
			y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz, d1 = (e0 - e1 * yiz) * x - dx, d2 = (e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0;
			x = clamp_tpl(x + d1 / d2, minX, minX + (maxX - minX) * 0.995f);
			if (x < minX + (maxX - minX) * 0.96)
			{
				for (i = 9; fabs(dist) > eps && i; x = clamp_tpl(x - dist, minX, maxX), --i)
				{
					y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz;
					d1 = dx - (e0 - e1 * yiz) * x, d2 = (e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0, dist = d1 / d2;
				}
				y = C - A * x * x, z = sqrt_tpl(rr - y * y) * rz;
			}
			else
			{
				f32 maxZ = sqrt_tpl(rr - sqr(C - A * minX * minX));
				for (i = 9, y = C - A * x * x, z = sqrt_tpl(rr - y * y); fabs(dist) > eps && i; z = clamp_tpl(z - dist, 0.0f, maxZ), --i)
				{
					zz = z * z, iz = isqrt_tpl(rr - zz), o = isqrt_tpl((1.0f / iz + C) / A), e2 = 0.5f / A * o * iz;
					d1 = z * e2 * dx - dz - z * iz * dy, d2 = (1 + zz / (rr - zz)) * (e2 * dx - iz * dy) + (dx * zz * o * o * o) / (4 * A * A * (rr - zz)), dist = d1 / d2;
				}
				y = sqrt_tpl(rr - z * z), x = sqrt_tpl((y + C) / A), y = -y, z *= rz;
			}
			Vec3 gpoint(x * rx + rpos.x, fcos * y - fsin * z, fsin * y + fcos * z);
			if (fabs(gpoint.x) > ca || gpoint.y > 0 || gpoint.z < 0)
				return 0;
			vContactPoint = gpoint;
			return 1;
		}
		Vec3 CheckHalfCapsule(const Vec3& rpos, const Vec3& rdir, const f32 sl, const f32 lr, const f32 ca, const f32 cr)
		{
			const f32 rz = idz < 0 ? 1.0f : -1.0f;
			const f32 rh = h * rz;
			const f32 ty = cy * b + cz * rh;
			const f32 tz = cz * b - cy * rh;
			const f32 hx = ty * rdir.z - tz * rdir.y;
			const f32 hy = tz * rdir.x;
			const f32 hz = -ty * rdir.x;

			if (fabs(hx) < HXEPSILON)
				return rpos - rdir * sl;

			const f32 tx = (rpos.x * hx + rpos.y * hy + rpos.z * hz - hy * ty - hz * tz) / hx;
			if (sqr(Vec3(tx, ty, tz) - rpos) < sl * sl)
			{
				if (fabs(tx) > ca || ty > 0 || tz < 0) return rpos - rdir * sl;
				return Vec3(tx, ty, tz);
			}
			f32 ycut = ty * fcos + tz * fsin, RR = sl * sl, rar = RR - aa - rr, A = 0.5f / a, C = A * rar;
			f32 minX = C > ycut ? sqrt_tpl(rar - 2 * a * ycut) : 0, rminX = rx * minX + rpos.x;
			f32 maxX = sqrt_tpl(2 * a * cr + rar), rmaxX = rx * maxX + rpos.x;
			if ((rminX > ca && rmaxX > ca) || (rminX < -ca && rmaxX < -ca))
				return rpos - rdir * sl;
			f32 dx = fabs(rdir.x), dy = fabs(idy), dz = idz * rz;
			f32 x = minX * 0.2f + maxX * 0.8f, y, z, clm[5] = { 0.97, 0.99, 1.0, 1.0, 1.0 }, dot = 9;
			for (i = 0; dot > 0 && i < 5; ++i)
			{
				f32 y = C - A * x * x, iz = isqrt_tpl(rr - y * y), iz3 = iz * iz * iz * 0.25f;
				f32 a0 = 4 * A * C, a1 = A * A * x, a2 = a0 * x - 4 * a1 * x * x, a3 = a2 * a2 * iz3, a4 = a2 * 3;
				f32 d1 = ((a0 - 12 * a1 * x) * iz * 0.5f - a3) * dz - 2 * A * dy;
				f32 d2 = (a4 * a3 * iz * iz * 0.5f - 12.0f * a1 * iz - a4 * a2 * iz3) * dz;
				if (d2 == 0) break;
				if ((x = clamp_tpl(x - d1 / d2, minX, minX + (maxX - minX) * clm[i])) >= maxX) break;
				a0 = 2 * A * x, y = C - A * x * x, dot = dx - a0 * dy + a0 * y * dz * isqrt_tpl(rr - y * y);
			}
			maxX = x;
			if (dot > 0)
				return rpos - rdir * sl;
			const f32 clx[10] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.995, 0.980 };
			f32 d = 9, j, iz, yiz, e0 = 2 * A * dy, e1 = 2 * A * dz * rz, e3 = 4 * A * A * dz * rz;
			for (i = 9, x = (minX + maxX) * 0.5f; fabs(d) > eps && i; x = clamp_tpl(x + d, minX, minX + (maxX - minX) * clx[i]), --i)
				y = C - A * x * x, iz = isqrt_tpl(rr - y * y), yiz = y * iz, j = (e0 - e1 * yiz) * x - dx, d = j / ((e1 * y - (e3 + e3 * yiz * yiz) * x * x) * iz - e0);
			y = C - A * x * x, z = sqrt_tpl(rr - y * y) * rz;
			Vec3 rpoint(x * rx + rpos.x, fcos * y - fsin * z, fsin * y + fcos * z);
			if (fabs(rpoint.x) > ca || rpoint.y > 0 || rpoint.z < 0)
				return rpos - rdir * sl;
			return rpoint;
		}
		uint32 i;
		f32    aa, a, l, f, fcos, fsin, eps, idy, idz, rx, rr, cy, cz, id, cid, b, h;
	} e;
	e.eps = 0.00001f;
	const Matrix33 m[2] = { Matrix33(0, 1, 0, -1, 0, 0, 0, 0, 1), Matrix33(0, 0, 1, 0, 1, 0, -1, 0, 0) };
	if (loz.y + loz.z == 0)
		return e.CheckCapsule(ipos, idir, sl, loz.x, loz.w) - ipos;
	if (loz.x + loz.z == 0)
		return e.CheckCapsule(Vec3(ipos.y, -ipos.x, ipos.z), Vec3(idir.y, -idir.x, idir.z), sl, loz.y, loz.w) * m[0] - ipos;
	if (loz.x + loz.y == 0)
		return e.CheckCapsule(Vec3(ipos.z, ipos.y, -ipos.x), Vec3(idir.z, idir.y, -idir.x), sl, loz.z, loz.w) * m[1] - ipos;
	Diag33 r(idir.x < 0 ? -1.0f : 1.0f, idir.y < 0 ? -1.0f : 1.0f, idir.z < 0 ? -1.0f : 1.0f);
	Vec3 p = ipos * r, d = idir * r;
	f32 arrdot[12] = { -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2 };
	Vec3 arrpnt[12] = { ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO }, ip;
	int s04 = 0, s26 = 0, s02 = 0, s46 = 0, s45 = 0, s67 = 0, s57 = 0, t46 = 0, t67 = 0, s37 = 0, s23 = 0, t26 = 0, xymid = 1, xzmid = 1, yzmid = 1;
	f32 mx = loz.x + loz.w, my = loz.y + loz.w, mz = loz.z + loz.w;
	if (-loz.x - loz.w > p.x)
	{
		Vec3 linedir = Vec3(0, d.y, d.z).GetNormalizedSafe(Vec3(0, 1, 0));
		Vec3 c = d * ((p.x + loz.x + loz.w) / d.x), pol = p - c;
		f32 bb = linedir | c, off = loz.x + loz.w - lr;
		arrpnt[0] = linedir * (bb + sqrt_tpl(sl * sl + bb * bb - (c | c))) + pol, arrdot[0] = (arrpnt[0] - p).GetNormalized() | d;
		f32 rop = sqrt_tpl(sqr(p.y - arrpnt[0].y) + sqr(p.z - arrpnt[0].z));
		s04 = loz.z - p.z < rop, s26 = p.z + loz.z < rop, xzmid = !s26, s02 = loz.y - p.y < rop, s46 = p.y + loz.y < rop, xymid = !s46;
	}
	else if (p.x <= -loz.x && p.x >= -loz.x - loz.w)
		s04 = p.z >= loz.z && p.z <= mz, s26 = p.z <= -loz.z && p.z >= -mz, xzmid = !s26, s02 = p.y >= loz.y && p.y <= my, s46 = p.y <= -loz.y && p.y >= -my, xymid = !s46;
	if (-loz.y - loz.w > p.y)
	{
		Vec3 linedir = Vec3(d.x, 0, d.z).GetNormalizedSafe(Vec3(1, 0, 0));
		Vec3 c = d * ((p.y + loz.y + loz.w) / d.y), pol = p - c;
		f32 bb = linedir | c, off = loz.y + loz.w - lr;
		arrpnt[1] = linedir * (bb + sqrt_tpl(sl * sl + bb * bb - (c | c))) + pol, arrdot[1] = (arrpnt[1] - p).GetNormalized() | d;
		f32 rop = sqrt_tpl(sqr(p.x - arrpnt[1].x) + sqr(p.z - arrpnt[1].z));
		s45 = loz.z - p.z < rop, s67 = p.z + loz.z < rop, yzmid = !s67, s57 = loz.x - p.x < rop, t46 = xymid && p.x + loz.x < rop;
	}
	else if (p.y <= -loz.y && p.y >= -loz.y - loz.w)
		s45 = p.z >= loz.z && p.z <= mz, s67 = p.z <= -loz.z && p.z >= -mz, yzmid = !s67, s57 = p.x >= loz.x && p.x <= mx, t46 = xymid && p.x <= -loz.x && p.x >= -mx;
	if (-loz.z - loz.w > p.z)
	{
		Vec3 linedir = Vec3(d.x, d.y, 0).GetNormalizedSafe(Vec3(0, 1, 0));
		Vec3 c = d * ((p.z + loz.z + loz.w) / d.z), pol = p - c;
		f32 bb = linedir | c, off = loz.z + loz.w - lr;
		arrpnt[2] = linedir * (bb + sqrt_tpl(sl * sl + bb * bb - (c | c))) + pol, arrdot[2] = (arrpnt[2] - p).GetNormalized() | d;
		f32 rop = sqrt_tpl(sqr(p.x - arrpnt[2].x) + sqr(p.y - arrpnt[2].y));
		s23 = loz.y - p.y < rop, t67 = yzmid && p.y + loz.y < rop, t26 = xzmid && p.x + loz.x < rop, s37 = loz.x - p.x < rop;
	}
	else if (p.z <= -loz.z && p.z >= -loz.z - loz.w)
		s23 = p.y >= loz.y && p.y <= my, t67 = yzmid && p.y <= -loz.y && p.y >= -my, t26 = xzmid && p.x <= -loz.x && p.x >= -mx, s37 = p.x >= loz.x && p.x <= mx;
	if (s04)
		ip = e.CheckEdge(Vec3(-p.y, p.x + loz.x, p.z - loz.z), Vec3(-d.y, d.x, d.z), sl, lr, loz.y, loz.w),
		arrpnt[5](ip.y - loz.x, -ip.x, ip.z + loz.z), arrdot[5] = (arrpnt[5] - p).GetNormalized() | d;
	if (s26)
		ip = e.CheckEdge(Vec3(p.y, p.x + loz.x, -p.z - loz.z), Vec3(d.y, d.x, -d.z), sl, lr, loz.y, loz.w),
		arrpnt[4](ip.y - loz.x, ip.x, -ip.z - loz.z), arrdot[4] = (arrpnt[4] - p).GetNormalized() | d;
	if (s02)
		ip = e.CheckEdge(Vec3(p.z, p.x + loz.x, p.y - loz.y), Vec3(d.z, d.x, d.y), sl, lr, loz.z, loz.w),
		arrpnt[6](ip.y - loz.x, ip.z + loz.y, ip.x), arrdot[6] = (arrpnt[6] - p).GetNormalized() | d;
	if (s46)
		ip = e.CheckEdge(Vec3(-p.z, p.x + loz.x, -p.y - loz.y), Vec3(-d.z, d.x, -d.y), sl, lr, loz.z, loz.w),
		arrpnt[3](ip.y - loz.x, -ip.z - loz.y, -ip.x), arrdot[3] = (arrpnt[3] - p).GetNormalized() | d;
	if (s45)
		ip = e.CheckEdge(Vec3(p.x, p.y + loz.y, p.z - loz.z), d, sl, lr, loz.x, loz.w),
		arrpnt[8](ip.x, ip.y - loz.y, ip.z + loz.z), arrdot[8] = (arrpnt[8] - p).GetNormalized() | d;
	if (s67)
		ip = e.CheckEdge(Vec3(-p.x, p.y + loz.y, -p.z - loz.z), Vec3(-d.x, d.y, -d.z), sl, lr, loz.x, loz.w),
		arrpnt[7](-ip.x, ip.y - loz.y, -ip.z - loz.z), arrdot[7] = (arrpnt[7] - p).GetNormalized() | d;
	if (s57)
		ip = e.CheckEdge(Vec3(-p.z, p.y + loz.y, p.x - loz.x), Vec3(-d.z, d.y, d.x), sl, lr, loz.z, loz.w),
		arrpnt[9](ip.z + loz.x, ip.y - loz.y, -ip.x), arrdot[9] = (arrpnt[9] - p).GetNormalized() | d;
	if (t46)
		ip = e.CheckEdge(Vec3(p.z, p.y + loz.y, -p.x - loz.x), Vec3(d.z, d.y, -d.x), sl, lr, loz.z, loz.w),
		arrpnt[3](-ip.z - loz.x, ip.y - loz.y, ip.x), arrdot[3] = (arrpnt[3] - p).GetNormalized() | d;
	if (s23)
		ip = e.CheckEdge(Vec3(-p.x, p.z + loz.z, p.y - loz.y), Vec3(-d.x, d.z, d.y), sl, lr, loz.x, loz.w),
		arrpnt[10](-ip.x, ip.z + loz.y, ip.y - loz.z), arrdot[10] = (arrpnt[10] - p).GetNormalized() | d;
	if (t67)
		ip = e.CheckEdge(Vec3(p.x, p.z + loz.z, -p.y - loz.y), Vec3(d.x, d.z, -d.y), sl, lr, loz.x, loz.w),
		arrpnt[7](ip.x, -ip.z - loz.y, ip.y - loz.z), arrdot[7] = (arrpnt[7] - p).GetNormalized() | d;
	if (t26)
		ip = e.CheckEdge(Vec3(-p.y, p.z + loz.z, -p.x - loz.x), Vec3(-d.y, d.z, -d.x), sl, lr, loz.y, loz.w),
		arrpnt[4](-ip.z - loz.x, -ip.x, ip.y - loz.z), arrdot[4] = (arrpnt[4] - p).GetNormalized() | d;
	if (s37)
		ip = e.CheckEdge(Vec3(p.y, p.z + loz.z, p.x - loz.x), Vec3(d.y, d.z, d.x), sl, lr, loz.y, loz.w),
		arrpnt[11](ip.z + loz.x, ip.x, ip.y - loz.z), arrdot[11] = (arrpnt[11] - p).GetNormalized() | d;
	int idx = 0;
	for (int i = 1; i < 12; i++)
		if (arrdot[i] > arrdot[idx]) idx = i;
	return arrpnt[idx] * r - ipos;
}

//Projects a penetrating object out of a lozenge and back to its surface by using the predefined rotation direction.
Vec3 CProxy::DirectedRotationalProjection(const Vec3& ipos, const Vec3& idir, f32 sl, f32 sr, const Vec3& iha) const
{
	Vec4 loz = m_params;
	loz.w += sr;
	if (loz.x + loz.y + loz.z == 0)
	{
		f32 pn = iha.x < 0 ? -1.0f : 1.0f, vx = pn * iha.x, vy = pn * iha.y, vz = pn * iha.z, w = 1.0f / (1.0f + vx);
		Matrix33 m(vx, -vy, -vz, vy, w * vz * vz + vx, -w * vz * vy, vz, -w * vz * vy, w * vy * vy + vx);
		Vec3 ip(ipos.x, ipos.y, ipos.z), np = ip * m;
		f32 x = ip | ip, cr2 = loz.w * loz.w, c0 = x - cr2 - cr2, cy = np.y, cz = np.z, d2, id, a, h, b, s2 = (1.0f - sqr(np.x / loz.w)) * cr2;
		if (sl * sl - sqr(x - cr2) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
			d2 = cy * cy + cz * cz, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, h = sqrt_tpl(max(0.0f, s2 - a * a)) * id * pn;
		else
			cy *= 0.5f, cz *= 0.5f, id = isqrt_tpl(cy * cy + cz * cz), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(s2 - a * a) * id * pn;
		return m * Vec3(np.x, cy * b - cz * h, cz * b + cy * h) - ipos;
	}
	struct
	{
		Quat CheckEdge(const Vec3& ipos, const Vec3& idir, f32 sl, const Vec3& ha, f32 ca, f32 cr)
		{
			if (ipos.y * ipos.y + ipos.z * ipos.z - sqr(cr + sl) > -0.00001f)
				return Quat(ZERO);
			f32 pd = ipos | ha, dn = -ca * ha.x - pd, dp = ca * ha.x - pd, cr2 = cr * cr;
			if ((dn < 0 && dp < 0) | (dp > 0 && dn > 0) && min(dn * dn, dp * dp) - cr2 > 0)
				return Quat(ZERO);
			f32 pn = ha.x < 0 ? -1.0f : 1.0f, vx = pn * ha.x, vy = pn * ha.y, vz = pn * ha.z, w = 1.0f / (1.0f + vx);
			Matrix33 m(vx, -vy, -vz, vy, w * vz * vz + vx, -w * vz * vy, vz, -w * vz * vy, w * vy * vy + vx);
			if (ipos.y * ipos.y + ipos.z * ipos.z < cr2)
			{
				f32 t = ipos.x < 0 ? -1.0f : 1.0f;
				Vec3 ip(ipos.x - ca * t, ipos.y, ipos.z), np = ip * m, vca(ca * t, 0, 0);
				f32 x = ip | ip, c0 = x - cr2 - cr2, cy = np.y, cz = np.z, d2, id, a, h, b, s2 = (1.0f - sqr(np.x / cr)) * cr2;
				if (sl * sl - sqr(x - cr2) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
					d2 = cy * cy + cz * cz, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, h = sqrt_tpl(max(0.0f, s2 - a * a)) * id * pn;
				else
					cy *= 0.5f, cz *= 0.5f, id = isqrt_tpl(cy * cy + cz * cz), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(fabs(s2 - a * a)) * id * pn;
				return Quat(1, m * Vec3(np.x, cy * b - cz * h, cz * b + cy * h) + vca);
			}
			if (fabs(ha.x) < f32(HPLANEEPSILON))
				return Quat(ZERO);
			for (f32 t = -1; t < 1.5f; t += 2)
			{
				Vec3 ip(ipos.x - ca * t, ipos.y, ipos.z);
				if (sqr(sl + cr) < (ip | ip)) continue;
				Vec3 np = ip * m;
				f32 sine = 1.0f - sqr(np.x / cr);
				if (sine < 0) continue;
				f32 d2, id, a, b, h, x = ip | ip, c0 = x - cr2 - cr2, s2 = sine * cr2, d = 0;
				if (sl * sl - sqr(x - cr2) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
					d2 = np.y * np.y + np.z * np.z, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, d = s2 - a * a, h = sqrt_tpl(fabs(d)) * id * pn;
				else
					np.y *= 0.5f, np.z *= 0.5f, id = isqrt_tpl(np.y * np.y + np.z * np.z), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(s2 - a * a) * id * pn;
				if (d < 0) continue;
				Vec3 tmp = m * Vec3(np.x, np.y * b - np.z * h, np.z * b + np.y * h);
				if (tmp.x * t > 0)
					return Quat(1, tmp.x + ca * t, tmp.y, tmp.z);
			}
			f32 cy = ipos.y * 0.5f, cz = ipos.z * 0.5f, id = isqrt_tpl(cy * cy + cz * cz);
			f32 a = cr2 * id * 0.5f, b = a * id, h = sqrt_tpl(fabs(cr2 - a * a)) * id * pn, d = ipos | ha, y0 = cy * b - cz * h, z0 = cz * b + cy * h;
			Vec3 tangpnt(-(ha.y * y0 + ha.z * z0 - d) / ha.x, y0, z0), gdist = tangpnt - ipos, pdir = (idir % ha).GetNormalized();
			if ((gdist | gdist) < sl * sl)
				return Quat(1, tangpnt);
			if (fabs(ha.x) > 0.999f && fabs(ipos.x) > ca)
				return Quat(ZERO);
			Vec2 n1(idir.y, idir.z), n2(pdir.y, pdir.z), n0(ipos.y, ipos.z);
			f32 n11 = n1 | n1, n12 = n1 | n2, n22 = n2 | n2, n10 = n1 | n0, n20 = n2 | n0, n00 = n0 | n0, ex, ey, u, v, det;
			f32 x = gdist | idir, y = gdist | pdir, isq = isqrt_tpl(x * x + y * y) * sl;
			x *= isq, y *= isq;
			for (int i = 0; i < 10; ey = sl * sl - x * x - y * y, x += (y * ex - v * ey) * det, y += (u * ey - x * ex) * det, ++i)
				u = n11 * x + n12 * y + n10, v = n22 * y + n12 * x + n20, det = 0.5f / (u * y - v * x), ex = cr2 - n11 * x * x - n22 * y * y - 2.0f * (n12 * x * y + n10 * x + n20 * y) - n00;
			Vec3 p = (idir * x + pdir * y).GetNormalized() * sl + ipos;
			if (sqr(max(0.0f, fabs(p.x) - ca)) - cr2 + p.y * p.y + p.z * p.z < 0.00001f)
				return Quat(1, p);
			return Quat(ZERO);
		}
		f32 TestOverlapping(Vec3 p0, Vec3 dir, f32 sl, f32 ca, f32 cr)
		{
			Vec3 p1 = p0 + dir * sl;
			if (ca == 0)
			{
				Vec3 ab = p1 - p0;
				f32 e = -p0 | ab, f = ab | ab;
				if (e <= 0) return (p0 | p0) - cr * cr;
				if (e >= f) return (p1 | p1) - cr * cr;
				return (p0 | p0) - e * e / f - cr * cr;
			}
			Vec3 x;
			if (p0.x > p1.x) x = p0, p0 = p1, p1 = x;
			Vec3 rdir = p1 - p0, p0ca = p0;
			p0ca.x += ca;
			f32 a = rdir | p0ca, b = ca + ca, c = b * b, d = rdir | rdir, e = p0ca.x * b, f = rdir.x * b, g = c * d - f * f;
			if (g)
			{
				f32 s = fclamp01_tpl((e * d - f * a) / g), t = (f * s - a) / d;
				if (t < 0.0f) t = 0, s = fclamp01_tpl(e / c);
				if (t > 1.0f) t = 1, s = fclamp01_tpl((f + e) / c);
				return sqr(Vec3(rdir.x * t + p0.x - b * s + ca, rdir.y * t + p0.y, rdir.z * t + p0.z)) - cr * cr;
			}
			f32 t = e / c;
			x = p0;
			if (p0.x > ca) t = fclamp01_tpl(t);
			if (p1.x < -ca) t = fclamp01_tpl((f + e) / c), x = p1;
			x.x -= b * t - ca;
			return (x | x) - cr * cr;
		}
		Vec3 CheckCapsule(const Vec3& ipos, const Vec3& idir, f32 sl, const Vec3& ha, f32 ca, f32 cr)
		{
			f32 pn = ha.x < 0 ? -1.0f : 1.0f, vx = pn * ha.x, vy = pn * ha.y, vz = pn * ha.z, w = 1.0f / (1.0f + vx), cr2 = cr * cr;
			Matrix33 m(vx, -vy, -vz, vy, w * vz * vz + vx, -w * vz * vy, vz, -w * vz * vy, w * vy * vy + vx);
			if (ipos.y * ipos.y + ipos.z * ipos.z < cr2)
			{
				f32 t = ipos.x < 0 ? -1.0f : 1.0f;
				Vec3 ip(ipos.x - ca * t, ipos.y, ipos.z), np = ip * m, vca(ca * t, 0, 0);
				f32 x = ip | ip, c0 = x - cr2 - cr2, cy = np.y, cz = np.z, d2, id, a, h, b, s2 = (1.0f - sqr(np.x / cr)) * cr2;
				if (sl * sl - sqr(x - cr2) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
					d2 = cy * cy + cz * cz, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, h = sqrt_tpl(max(0.0f, s2 - a * a)) * id * pn;
				else
					cy *= 0.5f, cz *= 0.5f, id = isqrt_tpl(cy * cy + cz * cz), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(fabs(s2 - a * a)) * id * pn;
				return m * Vec3(np.x, cy * b - cz * h, cz * b + cy * h) + vca;
			}
			if (fabs(ha.x) > f32(HPLANEEPSILON))
			{
				for (f32 t = -1; t < 1.5f; t += 2)
				{
					Vec3 ip(ipos.x - ca * t, ipos.y, ipos.z);
					if (sqr(sl + cr) < (ip | ip)) continue;
					Vec3 np = ip * m;
					f32 sine = 1.0f - sqr(np.x / cr);
					if (sine < 0) continue;
					f32 d2, id, a, b, h, x = ip | ip, c0 = x - cr2 - cr2, s2 = sine * cr2, d = 0;
					if (sl * sl - sqr(x - cr2) / x - (1.0f - c0 / x * c0 / x) * x * 0.25f < 0)
						d2 = np.y * np.y + np.z * np.z, id = isqrt_tpl(d2), a = (s2 - sl * sl + d2) * id * 0.5f, b = a * id, d = s2 - a * a, h = sqrt_tpl(fabs(d)) * id * pn;
					else
						np.y *= 0.5f, np.z *= 0.5f, id = isqrt_tpl(np.y * np.y + np.z * np.z), a = s2 * id * 0.5f, b = a * id, h = sqrt_tpl(s2 - a * a) * id * pn;
					if (d < 0) continue;
					Vec3 tmp = m * Vec3(np.x, np.y * b - np.z * h, np.z * b + np.y * h);
					if (tmp.x * t > 0)
						return Vec3(tmp.x + ca * t, tmp.y, tmp.z);
				}
				f32 cy = ipos.y * 0.5f, cz = ipos.z * 0.5f, id = isqrt_tpl(cy * cy + cz * cz);
				f32 a = cr2 * id * 0.5f, b = a * id, h = sqrt_tpl(fabs(cr2 - a * a)) * id * pn, d = ipos | ha, y0 = cy * b - cz * h, z0 = cz * b + cy * h;
				Vec3 tangpnt(-(ha.y * y0 + ha.z * z0 - d) / ha.x, y0, z0), gdist = tangpnt - ipos, pdir = idir % ha;
				if ((gdist | gdist) < sl * sl)
					return tangpnt;
				Vec2 n1(idir.y, idir.z), n2(pdir.y, pdir.z), n0(ipos.y, ipos.z);
				f32 n11 = n1 | n1, n12 = n1 | n2, n22 = n2 | n2, n10 = n1 | n0, n20 = n2 | n0, n00 = n0 | n0, ex, ey, u, v, det;
				f32 x = gdist | idir, y = gdist | pdir, isq = isqrt_tpl(x * x + y * y) * sl;
				x *= isq, y *= isq;
				for (int i = 0; i < 10; ey = sl * sl - x * x - y * y, x += (y * ex - v * ey) * det, y += (u * ey - x * ex) * det, ++i)
					u = n11 * x + n12 * y + n10, v = n22 * y + n12 * x + n20, det = 0.5f / (u * y - v * x), ex = cr2 - n11 * x * x - n22 * y * y - 2.0f * (n12 * x * y + n10 * x + n20 * y) - n00;
				return (idir * x + pdir * y).GetNormalized() * sl + ipos;
			}
			Vec3 g = (idir % ha - idir * MAXRADEPSILON).GetNormalized(), n, b = idir;
			for (int i = 0; ++i < 20; n = g + b, n *= isqrt_tpl(n | n), TestOverlapping(ipos, n, sl, ca, cr) > 0 ? g = n : b = n)
				;
			return g * sl + ipos;
		}
	} e;
	const Matrix33 m[4] = { Matrix33(0, 1, 0, -1, 0, 0, 0, 0, 1), Matrix33(1, 0, 0, 0, 1, 0, 0, 0, 1), Matrix33(1, 0, 0, 0, 0, -1, 0, 1, 0), Matrix33(0, 0, 1, 0, 1, 0, -1, 0, 0) };
	if (loz.y + loz.z == 0)
		return e.CheckCapsule(ipos, idir, sl, iha, loz.x, loz.w) - ipos;
	if (loz.x + loz.z == 0)
		return e.CheckCapsule(Vec3(ipos.y, -ipos.x, ipos.z), Vec3(idir.y, -idir.x, idir.z), sl, Vec3(iha.y, -iha.x, iha.z), loz.y, loz.w) * m[0] - ipos;
	if (loz.x + loz.y == 0)
		return e.CheckCapsule(Vec3(ipos.z, ipos.y, -ipos.x), Vec3(idir.z, idir.y, -idir.x), sl, Vec3(iha.z, iha.y, -iha.x), loz.z, loz.w) * m[3] - ipos;
	Quat q;
	Diag33 r(idir.x < 0 ? -1.0f : 1.0f, idir.y < 0 ? -1.0f : 1.0f, idir.z < 0 ? -1.0f : 1.0f);
	Vec3 rp = ipos * r, rd = idir * r, rh = iha * r * (r.x * r.y * r.z), l;
	for (uint32 axis = 0; axis < 3; axis++)
	{
		if (axis == 0)  l(loz.y, loz.x, loz.z);
		if (axis == 1)  l(loz.x, loz.y, loz.z);
		if (axis == 2)  l(loz.x, loz.z, loz.y);
		f32 y = rp.x * m[axis].m01 + rp.y * m[axis].m11 + rp.z * m[axis].m21;
		if (-l.y - loz.w >= y)
		{
			Matrix33 rm33 = r * m[axis];
			Vec3 p = rp * m[axis], d = rd * m[axis], h = rh * m[axis], lpnt, ldir;
			lpnt = d * (-l.y - loz.w - p.y) / d.y + p, ldir(h.z, 0, -h.x), ldir *= isqrt_tpl(ldir | ldir);
			f32 b = ldir | (p - lpnt), maxx = l.x + loz.w, maxz = l.z + loz.w;
			Vec3 spnt = ldir * (b + sqrt_tpl(sl * sl + b * b - sqr(lpnt - p))) + lpnt;
			if (fabs(spnt.z) <= l.z && fabs(spnt.x) <= l.x)
				return rm33 * spnt - ipos;
			if (l.z && spnt.x >= l.x)
			{
				q = e.CheckEdge(Vec3(p.z, p.y + l.y, -p.x + l.x), Vec3(d.z, d.y, -d.x), sl, Vec3(h.z, h.y, -h.x), l.z, loz.w);
				q.v(-q.v.z + l.x, q.v.y - l.y, q.v.x);
				if (q.w && fabs(q.v.z) <= maxz && (!l.x || q.v.x >= l.x) && (!l.y || q.v.y <= -l.y))
					return rm33 * q.v - ipos;
			}
			if (l.x && spnt.z >= l.z)
			{
				q = e.CheckEdge(Vec3(p.x, p.y + l.y, p.z - l.z), d, sl, h, l.x, loz.w), q.v.y -= l.y, q.v.z += l.z;
				if (q.w && fabs(q.v.x) <= maxx && (!l.z || q.v.z >= l.z) && (!l.y || q.v.y <= -l.y))
					return rm33 * q.v - ipos;
			}
			if (l.z && spnt.x <= -l.x)
			{
				q = e.CheckEdge(Vec3(p.z, p.y + l.y, -p.x - l.x), Vec3(d.z, d.y, -d.x), sl, Vec3(h.z, h.y, -h.x), l.z, loz.w);
				q.v(-q.v.z - l.x, q.v.y - l.y, q.v.x);
				if (q.w && fabs(q.v.z) <= maxz && (!l.y || q.v.y <= -l.y) && (!l.x || q.v.x <= -l.x))
					return rm33 * q.v - ipos;
			}
			if (l.x && spnt.z <= -l.z)
			{
				q = e.CheckEdge(Vec3(p.x, p.y + l.y, p.z + l.z), d, sl, h, l.x, loz.w), q.v.y -= l.y, q.v.z -= l.z;
				if (q.w && fabs(q.v.x) <= maxx && (!l.y || q.v.y <= -l.y) && (!l.z || q.v.z <= -l.z))
					return rm33 * q.v - ipos;
			}
		}
		if (y <= -l.y || y >= -l.y - loz.w)
		{
			Matrix33 rm33 = r * m[axis];
			Vec3 p = rp * m[axis], d = rd * m[axis], h = rh * m[axis];
			f32 maxx = l.x + loz.w, maxz = l.z + loz.w;
			if (p.x >= l.x && p.x <= maxx)
			{
				q = e.CheckEdge(Vec3(p.z, p.y + l.y, -p.x + l.x), Vec3(d.z, d.y, -d.x), sl, Vec3(h.z, h.y, -h.x), l.z, loz.w);
				q.v(-q.v.z + l.x, q.v.y - l.y, q.v.x);
				if (q.w && fabs(q.v.z) <= maxz && (!l.x || q.v.x >= l.x) && (!l.y || q.v.y <= -l.y))
					return rm33 * q.v - ipos;
			}
			if (p.z >= l.z && p.z <= maxz)
			{
				q = e.CheckEdge(Vec3(p.x, p.y + l.y, p.z - l.z), d, sl, h, l.x, loz.w), q.v.y -= l.y, q.v.z += l.z;
				if (q.w && fabs(q.v.x) <= maxx && (!l.z || q.v.z >= l.z) && (!l.y || q.v.y <= -l.y))
					return rm33 * q.v - ipos;
			}
			if (p.x <= -l.x && p.x >= -maxx)
			{
				q = e.CheckEdge(Vec3(p.z, p.y + l.y, -p.x - l.x), Vec3(d.z, d.y, -d.x), sl, Vec3(h.z, h.y, -h.x), l.z, loz.w);
				q.v(-q.v.z - l.x, q.v.y - l.y, q.v.x);
				if (q.w && fabs(q.v.z) <= maxz && (!l.y || q.v.y <= -l.y) && (!l.x || q.v.x <= -l.x))
					return rm33 * q.v - ipos;
			}
			if (p.z <= -l.z && p.z >= -maxz)
			{
				q = e.CheckEdge(Vec3(p.x, p.y + l.y, p.z + l.z), d, sl, h, l.x, loz.w), q.v.y -= l.y, q.v.z -= l.z;
				if (q.w && fabs(q.v.x) <= maxx && (!l.y || q.v.y <= -l.y) && (!l.z || q.v.z <= -l.z))
					return rm33 * q.v - ipos;
			}
		}
	}
	Vec3 g = (idir % iha - idir * MAXRADEPSILON).GetNormalized(), n, b = idir;
	for (int i = 0; ++i < 20; n = g + b, n *= isqrt_tpl(n | n), TestOverlapping(ipos, n, sl, sr) > 0 ? g = n : b = n)
		;
	return g * sl;
}

void CProxy::Draw(const QuatTS& qt, const ColorB clr, uint32 tesselation, const Vec3& vdir)
{
	uint32 p = 0, c = 0;
	Matrix34 m34 = Matrix34(qt);
	Matrix33 m33 = Matrix33(m34);
	Matrix33 mx, relx;
	relx.SetRotationX(gf_PI * 0.5f / f32(tesselation));
	Matrix33 my, rely;
	rely.SetRotationY(gf_PI * 0.5f / f32(tesselation));
	Matrix33 mz, relz;
	relz.SetRotationZ(gf_PI * 0.5f / f32(tesselation));
	uint32 stride = sqr(tesselation + 1);
	Vec3 ldir0 = vdir;
	ldir0.z = 0;
	(ldir0 = ldir0.normalized() * 0.5f).z = f32(sqrt3 * -0.5f);
	Vec3 ldir1(0, 0, 1);
	f32 cx = m_params.x, cy = m_params.y, cz = m_params.z, r = m_params.w;
	struct
	{
		ILINE ColorB shader(Vec3 n, const Vec3& d0, const Vec3& d1, ColorB c)
		{
			f32 a = 0.5f * n | d0, b = 0.1f * n | d1, l = min(1.0f, fabs_tpl(a) - a + fabs_tpl(b) - b + 0.05f);
			return RGBA8(uint8(l * c.r), uint8(l * c.g), uint8(l * c.b), c.a);
		}
		void Triangulate1(Vec3* p3d, Vec3* _p, ColorB* c3d, ColorB* _c, uint32 t, uint32 s, uint32& p, uint32& c, bool x = 0)
		{
			for (uint32 i = 0; i < t; i++)
			{
				_p[p++] = p3d[i + 1], _c[c++] = c3d[i + 1], _p[p++] = p3d[i + s + 0], _c[c++] = c3d[i + s + 0];
				_p[p++] = p3d[i + 0], _c[c++] = c3d[i + 0], _p[p++] = p3d[i + s + 0], _c[c++] = c3d[i + s + 0];
				_p[p++] = p3d[i + 1], _c[c++] = c3d[i + 1], _p[p++] = p3d[i + s + 1], _c[c++] = c3d[i + s + 1];
			}
			if (x) return;
			for (uint32 a = 0, b = 0; a < t; a++, b++)
			{
				for (uint32 i = 0; i < t; i++, b++)
				{
					_p[p++] = p3d[b + 0], _c[c++] = c3d[b + 0], _p[p++] = p3d[b + t + 1], _c[c++] = c3d[b + t + 1];
					_p[p++] = p3d[b + 1], _c[c++] = c3d[b + 1], _p[p++] = p3d[b + t + 2], _c[c++] = c3d[b + t + 2];
					_p[p++] = p3d[b + 1], _c[c++] = c3d[b + 1], _p[p++] = p3d[b + t + 1], _c[c++] = c3d[b + t + 1];
					_p[p++] = p3d[b + 1 + s], _c[c++] = c3d[b + 1 + s], _p[p++] = p3d[b + s + t + 1], _c[c++] = c3d[b + s + t + 1];
					_p[p++] = p3d[b + 0 + s], _c[c++] = c3d[b + 0 + s], _p[p++] = p3d[b + s + t + 1], _c[c++] = c3d[b + s + t + 1];
					_p[p++] = p3d[b + 1 + s], _c[c++] = c3d[b + 1 + s], _p[p++] = p3d[b + s + t + 2], _c[c++] = c3d[b + s + t + 2];
				}
			}
		}
		void Triangulate2(Vec3* p3d, Vec3* _p, ColorB* c3d, ColorB* _c, uint32 t, uint32 s, uint32& p, uint32& c, bool x = 0)
		{
			for (uint32 i = 0; i < t; i++)
			{
				_p[p++] = p3d[i + 0], _c[c++] = c3d[i + 0], _p[p++] = p3d[i + s + 0], _c[c++] = c3d[i + s + 0];
				_p[p++] = p3d[i + 1], _c[c++] = c3d[i + 1], _p[p++] = p3d[i + s + 1], _c[c++] = c3d[i + s + 1];
				_p[p++] = p3d[i + 1], _c[c++] = c3d[i + 1], _p[p++] = p3d[i + s + 0], _c[c++] = c3d[i + s + 0];
			}
			if (x) return;
			for (uint32 a = 0, b = 0; a < t; a++, b++)
			{
				for (uint32 i = 0; i < t; i++, b++)
				{
					_p[p++] = p3d[b + 1], _c[c++] = c3d[b + 1], _p[p++] = p3d[b + t + 1], _c[c++] = c3d[b + t + 1];
					_p[p++] = p3d[b + 0], _c[c++] = c3d[b + 0], _p[p++] = p3d[b + t + 1], _c[c++] = c3d[b + t + 1];
					_p[p++] = p3d[b + 1], _c[c++] = c3d[b + 1], _p[p++] = p3d[b + t + 2], _c[c++] = c3d[b + t + 2];
					_p[p++] = p3d[b + 0 + s], _c[c++] = c3d[b + 0 + s], _p[p++] = p3d[b + s + t + 1], _c[c++] = c3d[b + s + t + 1];
					_p[p++] = p3d[b + 1 + s], _c[c++] = c3d[b + 1 + s], _p[p++] = p3d[b + s + t + 2], _c[c++] = c3d[b + s + t + 2];
					_p[p++] = p3d[b + 1 + s], _c[c++] = c3d[b + 1 + s], _p[p++] = p3d[b + s + t + 1], _c[c++] = c3d[b + s + t + 1];
				}
			}
		}
	} draw;
	Vec3 p3d[0x4000], _p[0x4000];
	ColorB c3d[0x4000], _c[0x4000];
	my.SetIdentity();
	for (uint32 a = 0, b = 0; a < tesselation + 1; a++, my *= rely)
	{
		mx.SetIdentity();
		for (uint32 i = 0; i < tesselation + 1; i++, b++, mx *= relx)
		{
			Vec3 v = mx * Vec3(my.m02, my.m12, my.m22);
			p3d[b] = m34 * Vec3(-cx - v.x * r, -cy + v.y * r, cz + v.z * r);
			p3d[b + stride] = m34 * Vec3(+cx + v.x * r, -cy + v.y * r, cz + v.z * r);
			c3d[b] = draw.shader(m33 * Vec3(-v.x, v.y, v.z), ldir0, ldir1, clr);
			c3d[b + stride] = draw.shader(m33 * Vec3(v.x, v.y, v.z), ldir0, ldir1, clr);
		}
	}
	draw.Triangulate1(p3d, _p, c3d, _c, tesselation, stride, p, c);
	my.SetIdentity();
	for (uint32 a = 0, b = 0; a < tesselation + 1; a++, my *= rely)
	{
		mx.SetIdentity();
		for (uint32 i = 0; i < tesselation + 1; i++, b++, mx *= relx)
		{
			Vec3 v = mx * Vec3(my.m02, my.m12, my.m22);
			p3d[b] = m34 * Vec3(-cx - v.x * r, -cy + v.y * r, -cz - v.z * r);
			p3d[b + stride] = m34 * Vec3(+cx + v.x * r, -cy + v.y * r, -cz - v.z * r);
			c3d[b] = draw.shader(m33 * Vec3(-v.x, v.y, -v.z), ldir0, ldir1, clr);
			c3d[b + stride] = draw.shader(m33 * Vec3(v.x, v.y, -v.z), ldir0, ldir1, clr);
		}
	}
	draw.Triangulate2(p3d, _p, c3d, _c, tesselation, stride, p, c);
	my.SetIdentity();
	for (uint32 a = 0, b = 0; a < tesselation + 1; a++, my *= rely)
	{
		mx.SetIdentity();
		for (uint32 i = 0; i < tesselation + 1; i++, b++, mx *= relx)
		{
			Vec3 v = mx * Vec3(my.m02, my.m12, my.m22);
			p3d[b] = m34 * Vec3(-cx - v.x * r, +cy - v.y * r, cz + v.z * r);
			p3d[b + stride] = m34 * Vec3(+cx + v.x * r, +cy - v.y * r, cz + v.z * r);
			c3d[b] = draw.shader(m33 * Vec3(-v.x, -v.y, v.z), ldir0, ldir1, clr);
			c3d[b + stride] = draw.shader(m33 * Vec3(v.x, -v.y, v.z), ldir0, ldir1, clr);
		}
	}
	draw.Triangulate2(p3d, _p, c3d, _c, tesselation, stride, p, c);
	my.SetIdentity();
	for (uint32 a = 0, b = 0; a < tesselation + 1; a++, my *= rely)
	{
		mx.SetIdentity();
		for (uint32 i = 0; i < tesselation + 1; i++, b++, mx *= relx)
		{
			Vec3 v = mx * Vec3(my.m02, my.m12, my.m22);
			p3d[b] = m34 * Vec3(-cx - v.x * r, +cy - v.y * r, -cz - v.z * r);
			p3d[b + stride] = m34 * Vec3(+cx + v.x * r, +cy - v.y * r, -cz - v.z * r);
			c3d[b] = draw.shader(m33 * Vec3(-v.x, -v.y, -v.z), ldir0, ldir1, clr);
			c3d[b + stride] = draw.shader(m33 * Vec3(v.x, -v.y, -v.z), ldir0, ldir1, clr);
		}
	}
	draw.Triangulate1(p3d, _p, c3d, _c, tesselation, stride, p, c);
	my.SetIdentity();
	for (uint32 i = 0, b = 0; i < tesselation + 1; i++, b++, my *= rely)
	{
		Vec3 v = my.GetColumn2();
		p3d[b] = m34 * Vec3(+cx + v.x * r, -cy + v.y * r, cz + v.z * r);
		p3d[b + stride] = m34 * Vec3(+cx + v.x * r, cy - v.y * r, cz + v.z * r);
		c3d[b] = draw.shader(m33 * Vec3(v.x, v.y, v.z), ldir0, ldir1, clr);
		c3d[b + stride] = draw.shader(m33 * Vec3(v.x, -v.y, v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate1(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	my.SetIdentity();
	for (uint32 i = 0, b = 0; i < tesselation + 1; i++, b++, my *= rely)
	{
		Vec3 v = my.GetColumn2();
		p3d[b] = m34 * Vec3(+cx + v.x * r, -cy + v.y * r, -cz - v.z * r);
		p3d[b + stride] = m34 * Vec3(+cx + v.x * r, cy - v.y * r, -cz - v.z * r);
		c3d[b] = draw.shader(m33 * Vec3(v.x, v.y, -v.z), ldir0, ldir1, clr);
		c3d[b + stride] = draw.shader(m33 * Vec3(v.x, -v.y, -v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate2(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	my.SetIdentity();
	for (uint32 i = 0, b = 0; i < tesselation + 1; i++, b++, my *= rely)
	{
		Vec3 v = my.GetColumn2();
		p3d[b] = m34 * Vec3(-cx - v.x * r, -cy + v.y * r, cz + v.z * r);
		p3d[b + stride] = m34 * Vec3(-cx - v.x * r, cy - v.y * r, cz + v.z * r);
		c3d[b] = draw.shader(m33 * Vec3(-v.x, v.y, v.z), ldir0, ldir1, clr);
		c3d[b + stride] = draw.shader(m33 * Vec3(-v.x, -v.y, v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate2(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	my.SetIdentity();
	for (uint32 i = 0, b = 0; i < tesselation + 1; i++, b++, my *= rely)
	{
		Vec3 v = my.GetColumn2();
		p3d[b] = m34 * Vec3(-cx - v.x * r, -cy + v.y * r, -cz - v.z * r);
		p3d[b + stride] = m34 * Vec3(-cx - v.x * r, cy - v.y * r, -cz - v.z * r);
		c3d[b] = draw.shader(m33 * Vec3(-v.x, v.y, -v.z), ldir0, ldir1, clr);
		c3d[b + stride] = draw.shader(m33 * Vec3(-v.x, -v.y, -v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate1(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	mz.SetIdentity();
	for (uint32 i = 0, b = 0; i < tesselation + 1; i++, b++, mz *= relz)
	{
		Vec3 v = mz.GetColumn1();
		p3d[b] = m34 * Vec3(-cx + v.x * r, cy + v.y * r, -cz - v.z * r);
		p3d[b + stride] = m34 * Vec3(-cx + v.x * r, cy + v.y * r, cz + v.z * r);
		c3d[b] = draw.shader(m33 * Vec3(v.x, v.y, -v.z), ldir0, ldir1, clr);
		c3d[b + stride] = draw.shader(m33 * Vec3(v.x, v.y, v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate1(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	mz.SetIdentity();
	for (uint32 i = 0, c = 0; i < tesselation + 1; i++, c++, mz *= relz)
	{
		Vec3 v = mz.GetColumn1();
		p3d[c] = m34 * Vec3(cx - v.x * r, cy + v.y * r, -cz - v.z * r);
		p3d[c + stride] = m34 * Vec3(cx - v.x * r, cy + v.y * r, cz + v.z * r);
		c3d[c] = draw.shader(m33 * Vec3(-v.x, v.y, -v.z), ldir0, ldir1, clr);
		c3d[c + stride] = draw.shader(m33 * Vec3(-v.x, v.y, v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate2(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	mz.SetIdentity();
	for (uint32 i = 0, c = 0; i < tesselation + 1; i++, c++, mz *= relz)
	{
		Vec3 v = mz * Vec3(0, 1, 0);
		p3d[c] = m34 * Vec3(-cx + v.x * r, -cy - v.y * r, -cz - v.z * r);
		p3d[c + stride] = m34 * Vec3(-cx + v.x * r, -cy - v.y * r, cz + v.z * r);
		c3d[c] = draw.shader(m33 * Vec3(v.x, -v.y, -v.z), ldir0, ldir1, clr);
		c3d[c + stride] = draw.shader(m33 * Vec3(v.x, -v.y, v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate2(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	mz.SetIdentity();
	for (uint32 i = 0, c = 0; i < tesselation + 1; i++, c++, mz *= relz)
	{
		Vec3 v = mz * Vec3(0, 1, 0);
		p3d[c] = m34 * Vec3(cx - v.x * r, -cy - v.y * r, -cz - v.z * r);
		p3d[c + stride] = m34 * Vec3(cx - v.x * r, -cy - v.y * r, cz + v.z * r);
		c3d[c] = draw.shader(m33 * Vec3(-v.x, -v.y, -v.z), ldir0, ldir1, clr);
		c3d[c + stride] = draw.shader(m33 * Vec3(-v.x, -v.y, v.z), ldir0, ldir1, clr);
	}
	draw.Triangulate1(p3d, _p, c3d, _c, tesselation, stride, p, c, 1);
	p3d[0] = m34 * Vec3(-cx - r, -cy, -cz);
	p3d[0 + stride] = m34 * Vec3(-cx - r, +cy, -cz);
	c3d[0] = draw.shader(m33 * Vec3(-1, 0, 0), ldir0, ldir1, clr);
	c3d[0 + stride] = draw.shader(m33 * Vec3(-1, 0, 0), ldir0, ldir1, clr);
	p3d[1] = m34 * Vec3(-cx - r, -cy, +cz);
	p3d[1 + stride] = m34 * Vec3(-cx - r, +cy, +cz);
	c3d[1] = draw.shader(m33 * Vec3(-1, 0, 0), ldir0, ldir1, clr);
	c3d[1 + stride] = draw.shader(m33 * Vec3(-1, 0, 0), ldir0, ldir1, clr);
	draw.Triangulate1(p3d, _p, c3d, _c, 1, stride, p, c, 1);
	p3d[0] = m34 * Vec3(+cx + r, -cy, -cz);
	p3d[0 + stride] = m34 * Vec3(+cx + r, +cy, -cz);
	c3d[0] = draw.shader(m33 * Vec3(1, 0, 0), ldir0, ldir1, clr);
	c3d[0 + stride] = draw.shader(m33 * Vec3(1, 0, 0), ldir0, ldir1, clr);
	p3d[1] = m34 * Vec3(+cx + r, -cy, +cz);
	p3d[1 + stride] = m34 * Vec3(+cx + r, +cy, +cz);
	c3d[1] = draw.shader(m33 * Vec3(1, 0, 0), ldir0, ldir1, clr);
	c3d[1 + stride] = draw.shader(m33 * Vec3(1, 0, 0), ldir0, ldir1, clr);
	draw.Triangulate2(p3d, _p, c3d, _c, 1, stride, p, c, 1);
	p3d[0] = m34 * Vec3(-cx, +cy + r, -cz);
	p3d[0 + stride] = m34 * Vec3(cx, +cy + r, -cz);
	c3d[0] = draw.shader(m33 * Vec3(0, 1, 0), ldir0, ldir1, clr);
	c3d[0 + stride] = draw.shader(m33 * Vec3(0, 1, 0), ldir0, ldir1, clr);
	p3d[1] = m34 * Vec3(-cx, +cy + r, +cz);
	p3d[1 + stride] = m34 * Vec3(cx, +cy + r, +cz);
	c3d[1] = draw.shader(m33 * Vec3(0, 1, 0), ldir0, ldir1, clr);
	c3d[1 + stride] = draw.shader(m33 * Vec3(0, 1, 0), ldir0, ldir1, clr);
	draw.Triangulate1(p3d, _p, c3d, _c, 1, stride, p, c, 1);
	p3d[0] = m34 * Vec3(-cx, -cy - r, -cz);
	p3d[0 + stride] = m34 * Vec3(cx, -cy - r, -cz);
	c3d[0] = draw.shader(m33 * Vec3(0, -1, 0), ldir0, ldir1, clr);
	c3d[0 + stride] = draw.shader(m33 * Vec3(0, -1, 0), ldir0, ldir1, clr);
	p3d[1] = m34 * Vec3(-cx, -cy - r, +cz);
	p3d[1 + stride] = m34 * Vec3(cx, -cy - r, +cz);
	c3d[1] = draw.shader(m33 * Vec3(0, -1, 0), ldir0, ldir1, clr);
	c3d[1 + stride] = draw.shader(m33 * Vec3(0, -1, 0), ldir0, ldir1, clr);
	draw.Triangulate2(p3d, _p, c3d, _c, 1, stride, p, c, 1);
	p3d[0] = m34 * Vec3(-cx, -cy, cz + r);
	p3d[0 + stride] = m34 * Vec3(cx, -cy, cz + r);
	c3d[0] = draw.shader(m33 * Vec3(0, 0, 1), ldir0, ldir1, clr);
	c3d[0 + stride] = draw.shader(m33 * Vec3(0, 0, 1), ldir0, ldir1, clr);
	p3d[1] = m34 * Vec3(-cx, +cy, cz + r);
	p3d[1 + stride] = m34 * Vec3(cx, +cy, cz + r);
	c3d[1] = draw.shader(m33 * Vec3(0, 0, 1), ldir0, ldir1, clr);
	c3d[1 + stride] = draw.shader(m33 * Vec3(0, 0, 1), ldir0, ldir1, clr);
	draw.Triangulate2(p3d, _p, c3d, _c, 1, stride, p, c, 1);
	p3d[0] = m34 * Vec3(-cx, -cy, -cz - r);
	p3d[0 + stride] = m34 * Vec3(cx, -cy, -cz - r);
	c3d[0] = draw.shader(m33 * Vec3(0, 0, -1), ldir0, ldir1, clr);
	c3d[0 + stride] = draw.shader(m33 * Vec3(0, 0, -1), ldir0, ldir1, clr);
	p3d[1] = m34 * Vec3(-cx, +cy, -cz - r);
	p3d[1 + stride] = m34 * Vec3(cx, +cy, -cz - r);
	c3d[1] = draw.shader(m33 * Vec3(0, 0, -1), ldir0, ldir1, clr);
	c3d[1 + stride] = draw.shader(m33 * Vec3(0, 0, -1), ldir0, ldir1, clr);
	draw.Triangulate1(p3d, _p, c3d, _c, 1, stride, p, c, 1);
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	g_pAuxGeom->SetRenderFlags(renderFlags);
	g_pAuxGeom->DrawTriangles(&_p[0], p, &_c[0]);
}
