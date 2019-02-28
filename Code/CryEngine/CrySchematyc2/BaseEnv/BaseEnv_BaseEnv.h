// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IBaseEnv.h>

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include "BaseEnv/Utils/BaseEnv_EntityClassRegistrar.h" // TODO : Remove!!!
#include "BaseEnv/Utils/BaseEnv_EntityMap.h" // TODO : Remove!!!

namespace Schematyc2
{
	class CUpdateRelevanceContext;
}

namespace SchematycBaseEnv
{
	class CSpatialIndex;

//	class CEntityClassRegistrar;
//	class CEntityMap;

	class CBaseEnv : public SchematycBaseEnv::IBaseEnv
	{
	public:

		explicit CBaseEnv();
		~CBaseEnv();

		void PrePhysicsUpdate();
		void Update(Schematyc2::CUpdateRelevanceContext* pRelevanceContext = nullptr);

		static CBaseEnv& GetInstance();

		CSpatialIndex& GetSpatialIndex();
		CEntityMap& GetGameEntityMap();

		// SchematycBaseEnv::IBaseEnv
		virtual void UpdateSpatialIndex() override;

		virtual int GetEditorGameDefaultUpdateMask() const override { return m_editorGameDefaultUpdateMask; }
		virtual void SetEditorGameDefaultUpdateMask(const int mask) override { m_editorGameDefaultUpdateMask = mask; }
		virtual int GetGameDefaultUpdateMask() const override { return m_gameDefaultUpdateMask; }
		virtual void SetGameDefaultUpdateMask(const int mask) override { m_gameDefaultUpdateMask = mask; }
		// ~SchematycBaseEnv::IBaseEnv

	private:
		void Refresh();
		int GetUpdateFlags() const;
		int GetDefaultUpdateFlags() const;

	private:
		static CBaseEnv* ms_pInstance;

		std::shared_ptr<CSpatialIndex>  m_pSpatialIndex; // TODO : Why can't we use std::unique_ptr?
		CEntityClassRegistrar           m_gameEntityClassRegistrar;
		CEntityMap                      m_gameEntityMap;
		TemplateUtils::CConnectionScope m_connectionScope;

		int                             m_editorGameDefaultUpdateMask;
		int                             m_gameDefaultUpdateMask;
		int                             sc_Update;
	};
}