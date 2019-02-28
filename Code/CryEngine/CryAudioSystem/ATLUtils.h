// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
template<typename TMap, typename TKey>
bool FindPlace(TMap& map, TKey const& key, typename TMap::iterator& iPlace);

template<typename TMap, typename TKey>
bool FindPlaceConst(TMap const& map, TKey const& key, typename TMap::const_iterator& iPlace);

template<typename TMap, typename TKey>
bool FindPlace(TMap& map, TKey const& key, typename TMap::iterator& iPlace)
{
	iPlace = map.find(key);
	return (iPlace != map.end());
}

template<typename TMap, typename TKey>
bool FindPlaceConst(TMap const& map, TKey const& key, typename TMap::const_iterator& iPlace)
{
	iPlace = map.find(key);
	return (iPlace != map.end());
}
} // namespace CryAudio
