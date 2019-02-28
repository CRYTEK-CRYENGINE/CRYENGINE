// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Skeleton.h"

class CAttachmentManager;
class CModelMesh;

class CProxy : public IProxy
{
public:

	CProxy()
	{
		m_nPurpose = 0;
		m_nHideProxy = 0;
		m_nJointID = -1;
		m_ProxyAbsoluteDefault.SetIdentity();
		m_ProxyRelativeDefault.SetIdentity();
		m_ProxyModelRelative.SetIdentity();
		m_params(0, 0, 0, 0);
		m_pAttachmentManager = 0;
		m_ProxyModelRelativePrev.SetIdentity();
	}

	virtual const char*  GetName() const                                  { return m_strProxyName.c_str(); }
	virtual uint32       GetNameCRC() const                               { return m_nProxyCRC32; };
	virtual uint32       ReName(const char* strNewName, uint32 nNewCRC32) { m_strProxyName.clear(); m_strProxyName = strNewName; m_nProxyCRC32 = nNewCRC32; return 1; };
	virtual uint32       SetJointName(const char* szJointName);
	virtual uint32       GetJointID() const                               { return m_nJointID; }
	virtual const QuatT& GetProxyAbsoluteDefault() const                  { return m_ProxyAbsoluteDefault; };
	virtual const QuatT& GetProxyRelativeDefault() const                  { return m_ProxyRelativeDefault; };
	virtual const QuatT& GetProxyModelRelative() const                    { return m_ProxyModelRelative; };
	virtual void         SetProxyAbsoluteDefault(const QuatT& qt)         { m_ProxyAbsoluteDefault = qt; ProjectProxy(); }
	virtual uint32       ProjectProxy();
	virtual void         AlignProxyWithJoint();
	virtual Vec4         GetProxyParams() const         { return m_params; }
	virtual void         SetProxyParams(const Vec4& p)  { m_params = p; }
	virtual int8         GetProxyPurpose() const        { return m_nPurpose; }
	virtual void         SetProxyPurpose(int8 p)        { m_nPurpose = p; }
	virtual void         SetHideProxy(uint8 nHideProxy) { m_nHideProxy = nHideProxy; }

	f32                  GetDistance(const Vec3& p) const;
	f32                  GetDistance(const Vec3& p, f32 r) const;
	f32                  TestOverlapping(const Vec3& p0, Vec3 dir, f32 sl, f32 sr) const;
	Vec3                 ShortvecTranslationalProjection(const Vec3& ipos, f32 sr) const;
	f32                  DirectedTranslationalProjection(const Vec3& ipos, const Vec3& idir, f32 sl, f32 sr) const;
	Vec3                 ShortarcRotationalProjection(const Vec3& ipos, const Vec3& idir, f32 sl, f32 sr) const;
	Vec3                 DirectedRotationalProjection(const Vec3& ipos, const Vec3& idir, f32 sl, f32 sr, const Vec3& iha) const;
	void                 Draw(const QuatTS& qt, const ColorB clr, uint32 tesselation, const Vec3& vdir);

	uint8               m_nPurpose;   //0-auxiliary / 1-cloth / 2-ragdoll
	uint8               m_nHideProxy; //0-visible / 1-hidden
	int16               m_nJointID;
	Vec4                m_params;               //parameters for bounding volumes
	QuatT               m_ProxyRelativeDefault; //proxy location relative to the face/joint in default pose;
	QuatT               m_ProxyAbsoluteDefault; //proxy location relative to the default pose of the model  (=relative to Vec3(0,0,0))
	QuatT               m_ProxyModelRelative;   //proxy location relative to the animated pose of the model (=relative to Vec3(0,0,0))
	string              m_strJointName;
	string              m_strProxyName;
	uint32              m_nProxyCRC32;
	QuatT               m_ProxyModelRelativePrev;
	CAttachmentManager* m_pAttachmentManager;
};
