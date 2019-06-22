// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryMath/Cry_Math.h>

namespace CryAudio
{
class CListener;
class CTransformation;

class CListenerManager final
{
public:

	CListenerManager() = default;
	~CListenerManager();

	CListenerManager(CListenerManager const&) = delete;
	CListenerManager(CListenerManager&&) = delete;
	CListenerManager& operator=(CListenerManager const&) = delete;
	CListenerManager& operator=(CListenerManager&&) = delete;

	void              Initialize();
	void              Terminate();
	void              ReleaseImplData();
	void              Update(float const deltaTime);
	CListener*        CreateListener(CTransformation const& transformation, char const* const szName, bool const isUserCreated);
	void              ReleaseListener(CListener* const pListener);
	CListener*        GetListener(ListenerId const id) const;
	void              GetUniqueListenerName(char const* const szName, CryFixedStringT<MaxObjectNameLength>& newName);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	size_t GetNumListeners() const { return m_constructedListeners.size(); }
	void   ReconstructImplData();
#endif // CRY_AUDIO_USE_DEBUG_CODE

private:

	void GenerateUniqueListenerName(CryFixedStringT<MaxObjectNameLength>& name);

	std::vector<CListener*> m_constructedListeners;
};
} // namespace CryAudio
