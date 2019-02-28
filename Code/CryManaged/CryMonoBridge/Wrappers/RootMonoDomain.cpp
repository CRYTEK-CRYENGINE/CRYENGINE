#include "StdAfx.h"
#include "RootMonoDomain.h"

CRootMonoDomain::CRootMonoDomain()
{
	m_pDomain = nullptr;
}

void CRootMonoDomain::Initialize()
{
	m_pDomain = MonoInternals::mono_jit_init_version("CryEngine", "v4.0.30319");
	m_bNativeDomain = true;

	CacheObjectMethods();
}

CMonoLibrary& CRootMonoDomain::GetNetCoreLibrary()
{
	MonoInternals::MonoImage* pMonoCorlib = MonoInternals::mono_get_corlib();
	
	return GetLibraryFromMonoAssembly(MonoInternals::mono_image_get_assembly(pMonoCorlib), pMonoCorlib);
}