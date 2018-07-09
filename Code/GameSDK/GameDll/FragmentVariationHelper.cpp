// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FragmentVariationHelper.h"

CFragmentVariationHelper::CFragmentVariationHelper()
	: m_currentFragmentId( FRAGMENT_ID_INVALID )
	, m_requestedFragmentId( FRAGMENT_ID_INVALID )
	, m_selectedOptionIndex( OPTION_IDX_INVALID )
	, m_reachedFragmentEnd( false )
{
}


CFragmentVariationHelper::~CFragmentVariationHelper()
{
}


void CFragmentVariationHelper::SetFragmentId( const FragmentID fragmentId )
{
	m_requestedFragmentId = fragmentId;
}


void CFragmentVariationHelper::SetTagState( const SFragTagState& tagState )
{
	m_requestedTagState = tagState;
}


bool CFragmentVariationHelper::Update( const IAnimationDatabase* pDatabase, const bool forceReevaluate )
{
	const bool shouldQueueNewFragment = Reevaluate( pDatabase, forceReevaluate );
	return shouldQueueNewFragment;
}


void CFragmentVariationHelper::OnFragmentEnd()
{
	m_reachedFragmentEnd = true;
}


bool CFragmentVariationHelper::Reevaluate( const IAnimationDatabase* pDatabase, const bool forceReevaluate )
{
	CRY_ASSERT( pDatabase );
	const bool fragmentIdChanged = ( m_requestedFragmentId != m_currentFragmentId );
	const bool tagStateChanged = ( m_requestedTagState != m_currentTagState );
	const bool reevaluate = ( m_reachedFragmentEnd || tagStateChanged || fragmentIdChanged || forceReevaluate );
	if ( ! reevaluate )
	{
		return false;
	}

	SFragTagState matchingTagState;
	if ( m_requestedFragmentId != FRAGMENT_ID_INVALID )
	{
		SFragmentQuery fragQuery(m_requestedFragmentId, m_requestedTagState);
		const uint32 optionCount = pDatabase->FindBestMatchingTag( fragQuery, &matchingTagState );
	}

	const bool fragmentTagsChanged = ( matchingTagState != m_selectedTagState );
	const bool fragmentChanged = ( fragmentIdChanged || fragmentTagsChanged );

	m_currentFragmentId = m_requestedFragmentId;
	m_selectedTagState = matchingTagState;
	m_selectedOptionIndex = OPTION_IDX_RANDOM; // TODO: Be smarter about this!

	m_currentTagState = m_requestedTagState;

	const bool shouldQueueNewFragment = ( fragmentChanged || m_reachedFragmentEnd || forceReevaluate );

	m_reachedFragmentEnd = false;

	return shouldQueueNewFragment;
}


FragmentID CFragmentVariationHelper::GetFragmentId() const
{
	return m_currentFragmentId;
}

TagState CFragmentVariationHelper::GetFragmentTagState() const
{
	return m_selectedTagState.fragmentTags;
}

TagState CFragmentVariationHelper::GetTagState() const
{
	return m_selectedTagState.globalTags;
}

uint32 CFragmentVariationHelper::GetOptionIndex() const
{
	return m_selectedOptionIndex;
}
