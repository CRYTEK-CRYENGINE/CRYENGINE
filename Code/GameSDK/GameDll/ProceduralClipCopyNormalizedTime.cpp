// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryExtension/ClassWeaver.h>
#include <Mannequin/Serialization.h>
#include <CrySerialization/IArchive.h>

static const int INVALID_LAYER_INDEX = -1;

struct SProceduralParamsCopyNormalizedTime
	: public IProceduralParams
{
	SProceduralParamsCopyNormalizedTime()
		: sourceLayer(0)
		, layer(0)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(sourceLayer, "SourceLayer", "Source Layer");
		ar(layer, "Layer", "Layer");
		ar(sourceScope, "SourceScope", "Source Scope");
	}

	float sourceLayer;
	float layer;
	SProcDataCRC sourceScope;
};

class CProceduralClipCopyNormalizedTime
	: public TProceduralClip< SProceduralParamsCopyNormalizedTime >
{
public:
	CProceduralClipCopyNormalizedTime()
		: m_sourceLayer( INVALID_LAYER_INDEX )
		, m_targetLayer( INVALID_LAYER_INDEX )
	{
	}

	virtual void OnEnter( float blendTime, float duration, const SProceduralParamsCopyNormalizedTime& params )
	{
		IF_UNLIKELY ( m_charInstance == NULL )
		{
			return;
		}

		ISkeletonAnim* pSkeletonAnim = m_charInstance->GetISkeletonAnim();
		IF_UNLIKELY ( pSkeletonAnim == NULL )
		{
			return;
		}

		{
			const IScope* pSourceScope = GetSourceScope( params.sourceScope );
			if ( pSourceScope == NULL )
			{
				const char* const entityName = m_entity ? m_entity->GetName() : "";
				CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CopyNormalizedTime procedural clip setup is not valid for entity '%s'. Not able to find source scope named '%s'.", entityName, params.sourceScope.c_str() );
				return;
			}

			const int sourceScopeLayerCount = static_cast< int >( pSourceScope->GetTotalLayers() );
			const int sourceScopeLayerBase = static_cast< int >( pSourceScope->GetBaseLayer() );
			const int sourceLayerParam = static_cast< int >( params.sourceLayer );

			IF_UNLIKELY ( sourceLayerParam < 0 || sourceScopeLayerCount <= sourceLayerParam )
			{
				const char* const entityName = m_entity ? m_entity->GetName() : "";
				CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CopyNormalizedTime procedural clip setup is not valid for entity '%s'. Source layer is %d but should be between 0 and %d.", entityName, sourceLayerParam, sourceScopeLayerCount );
				return;
			}

			m_sourceLayer = sourceScopeLayerBase + sourceLayerParam;
		}
		
		{
			const int targetScopeLayerCount = static_cast< int >( m_scope->GetTotalLayers() );
			const int targetScopeLayerBase = static_cast< int >( m_scope->GetBaseLayer() );
			const int targetLayerParam = static_cast< int >( params.layer );

			IF_UNLIKELY ( targetLayerParam < 0 || targetScopeLayerCount <= targetLayerParam )
			{
				const char* const entityName = m_entity ? m_entity->GetName() : "";
				CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CopyNormalizedTime procedural clip setup is not valid for entity '%s'. Layer is %d but should be between 0 and %d.", entityName, targetLayerParam, targetScopeLayerCount );
				return;
			}

			m_targetLayer = targetScopeLayerBase + targetLayerParam;
		}

		IF_UNLIKELY ( m_targetLayer == m_sourceLayer )
		{
			const char* const entityName = m_entity ? m_entity->GetName() : "";
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CopyNormalizedTime procedural clip setup is not valid for entity '%s'. Target and source layers have the same value.", entityName );
			m_sourceLayer = INVALID_LAYER_INDEX;
			m_targetLayer = INVALID_LAYER_INDEX;
			return;
		}
	}

	virtual void OnExit( float blendTime )
	{
		const bool isSetupValid = IsSetupValid();
		IF_UNLIKELY ( ! isSetupValid )
		{
			return;
		}

		CRY_ASSERT( m_charInstance );
		ISkeletonAnim* pSkeletonAnim = m_charInstance->GetISkeletonAnim();
		CRY_ASSERT( pSkeletonAnim );

		const int targetAnimationsCount = pSkeletonAnim->GetNumAnimsInFIFO( m_targetLayer );
		for ( int i = 0; i < targetAnimationsCount; ++i )
		{
			CAnimation& targetAnimation = pSkeletonAnim->GetAnimFromFIFO( m_targetLayer, i );
			targetAnimation.ClearStaticFlag( CA_MANUAL_UPDATE );
		}
	}

	virtual void Update( float timePassed )
	{
		const bool isSetupValid = IsSetupValid();
		IF_UNLIKELY ( ! isSetupValid )
		{
			return;
		}

		CRY_ASSERT( m_charInstance );
		ISkeletonAnim* pSkeletonAnim = m_charInstance->GetISkeletonAnim();
		CRY_ASSERT( pSkeletonAnim );

		const int sourceAnimationsCount = pSkeletonAnim->GetNumAnimsInFIFO( m_sourceLayer );
		IF_UNLIKELY ( sourceAnimationsCount <= 0 )
		{
			return;
		}

		const CAnimation& sourceAnimation = pSkeletonAnim->GetAnimFromFIFO( m_sourceLayer, sourceAnimationsCount - 1 );
		const float sourceNormalizedTime = sourceAnimation.GetCurrentSegmentNormalizedTime();

		const int targetAnimationsCount = pSkeletonAnim->GetNumAnimsInFIFO( m_targetLayer );
		for ( int i = 0; i < targetAnimationsCount; ++i )
		{
			CAnimation& targetAnimation = pSkeletonAnim->GetAnimFromFIFO( m_targetLayer, i );
			targetAnimation.SetStaticFlag( CA_MANUAL_UPDATE );
			targetAnimation.SetCurrentSegmentNormalizedTime( sourceNormalizedTime );
		}
	}

private:

	bool IsSetupValid() const
	{
		const bool isSetupValid = ( m_sourceLayer != INVALID_LAYER_INDEX && m_targetLayer != INVALID_LAYER_INDEX );
		return isSetupValid;
	}


	const IScope* GetSourceScope( const SProcDataCRC& sourceScopeCrc ) const
	{
		if ( sourceScopeCrc.IsEmpty() )
		{
			return m_scope;
		}

		const IActionController& actionController = m_scope->GetActionController();
		const TagID sourceScopeTagId = actionController.GetContext().controllerDef.m_scopeIDs.Find( sourceScopeCrc.crc );
		IF_UNLIKELY ( sourceScopeTagId == SCOPE_ID_INVALID )
		{
			return NULL;
		}

		const IScope* pSourceScope = actionController.GetScope( sourceScopeTagId );
		CRY_ASSERT( pSourceScope );

		return pSourceScope;
	}

private:
	int m_sourceLayer;
	int m_targetLayer;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipCopyNormalizedTime, "CopyNormalizedTime");