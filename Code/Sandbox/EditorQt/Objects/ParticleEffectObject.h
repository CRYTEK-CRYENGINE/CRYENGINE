// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ParticleEffectObject_h__
#define __ParticleEffectObject_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"

class CParticleItem;

class SANDBOX_API CParticleEffectObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CParticleEffectObject)

	void        Serialize(CObjectArchive& ar) override;

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	bool        Init(CBaseObject* prev, const string& file);
	void        PostInit(const string& file);
	void        AssignEffect(const string& effectName);

	void        Display(CObjectRenderHelper& objRenderHelper) override;

	static bool IsGroup(const string& effectName);
	string     GetParticleName() const;
	string     GetComment() const;

	string      GetAssetPath() const override { return m_ParticleEntityName; }
	void        OnMenuGoToDatabase() const;

	class CFastParticleParser
	{
	public:
		void    ExtractParticlePathes(const string& particlePath);
		void    ExtractLevelParticles();
		size_t  GetCount()                         { return m_ParticleList.size(); }
		string GetParticlePath(size_t index)      { return m_ParticleList[index].pathName; }
		bool    HaveParticleChildren(size_t index) { return m_ParticleList[index].bHaveChildren; }

	private:
		void ParseParticle(XmlNodeRef& node, const string& parentPath);

	private:
		struct SParticleInfo
		{
			SParticleInfo()
			{
				bHaveChildren = false;
				pathName = "";
			}
			~SParticleInfo(){}
			bool    bHaveChildren;
			string pathName;
		};

		std::vector<SParticleInfo> m_ParticleList;
	};

private:

	CParticleItem* GetChildParticleItem(CParticleItem* pParentItem, string composedName, const string& wishName) const;

private:
	string m_ParticleEntityName;
};

/*!
 * Class Description of ParticleEffectObject
 */
class CParticleEffectDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()                     { return OBJTYPE_ENTITY; };
	const char*         ClassName()                         { return "ParticleEntity"; };
	const char*         Category()                          { return "Particle Entity"; };
	CRuntimeClass*      GetRuntimeClass()                   { return RUNTIME_CLASS(CParticleEffectObject); };
	const char*         GetFileSpec()                       { return "*ParticleEffects"; };
	virtual const char* GetDataFilesFilterString() override { return "*.pfx"; }
	virtual bool        IsCreatable() const override { return CObjectClassDesc::IsEntityClassAvailable(); }
};

#endif // __ParticleEffectObject_h__

