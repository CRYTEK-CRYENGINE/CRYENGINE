// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MaterialHelpers.h
//  Version:     v1.00
//  Created:     6/6/2014 by NielsF.
//  Compilers:   Visual Studio 2012
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MaterialHelpers_h__
#define __MaterialHelpers_h__
#pragma once

#include <Cry3DEngine/IMaterial.h>

// Description:
//   Namespace "implementation", not a class "implementation", no member-variables, only const functions;
//   Used to encapsulate the material-definition/io into Cry3DEngine (and make it plugable that way).
struct MaterialHelpers : public IMaterialHelpers
{
	//////////////////////////////////////////////////////////////////////////
	virtual EEfResTextures FindTexSlot(const char* texName) const final;
	virtual const char*    FindTexName(EEfResTextures texSlot) const final;
	virtual const char*    LookupTexName(EEfResTextures texSlot) const final;
	virtual const char*    LookupTexEnum(EEfResTextures texSlot) const final;
	virtual const char*    LookupTexSuffix(EEfResTextures texSlot) const final;
	virtual bool           IsAdjustableTexSlot(EEfResTextures texSlot) const final;

	//////////////////////////////////////////////////////////////////////////

	virtual const char* GetNameFromTextureType(uint8 texType) const final;
	virtual uint8       GetTextureTypeFromName(const char* szName) const final;
	virtual bool        IsTextureTypeExposed(uint8 texType) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual bool SetGetMaterialParamFloat(IRenderShaderResources& pShaderResources, const char* sParamName, float& v, bool bGet) const final;
	virtual bool SetGetMaterialParamVec3(IRenderShaderResources& pShaderResources, const char* sParamName, Vec3& v, bool bGet) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetTexModFromXml(SEfTexModificator& pShaderResources, const XmlNodeRef& node) const final;
	virtual void SetXmlFromTexMod(const SEfTexModificator& pShaderResources, XmlNodeRef& node) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetTexturesFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node, const char* szBaseFileName) const final;
	virtual void SetXmlFromTextures(const SInputShaderResources& pShaderResources, XmlNodeRef& node, const char* szBaseFileName) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetVertexDeformFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const final;
	virtual void SetXmlFromVertexDeform(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetDetailDecalFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const final;
	virtual void SetXmlFromDetailDecal(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetLightingFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const final;
	virtual void SetXmlFromLighting(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetShaderParamsFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const final;
	virtual void SetXmlFromShaderParams(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const final;

	//////////////////////////////////////////////////////////////////////////
	virtual void MigrateXmlLegacyData(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const final;
};

#endif
