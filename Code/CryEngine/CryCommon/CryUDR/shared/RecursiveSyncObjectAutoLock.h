// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		class CRecursiveSyncObjectAutoLock
		{
		public:

			CRecursiveSyncObjectAutoLock();
			~CRecursiveSyncObjectAutoLock();

		private:

			// disallow all kinds of copying
			CRecursiveSyncObjectAutoLock(const CRecursiveSyncObjectAutoLock&) = delete;
			CRecursiveSyncObjectAutoLock(CRecursiveSyncObjectAutoLock&&) = delete;
			CRecursiveSyncObjectAutoLock& operator=(const CRecursiveSyncObjectAutoLock&) = delete;
			CRecursiveSyncObjectAutoLock& operator=(CRecursiveSyncObjectAutoLock&&) = delete;
		};

		inline CRecursiveSyncObjectAutoLock::CRecursiveSyncObjectAutoLock()
		{
			gEnv->pUDR->GetRecursiveSyncObject().Lock();
		}

		inline CRecursiveSyncObjectAutoLock::~CRecursiveSyncObjectAutoLock()
		{
			gEnv->pUDR->GetRecursiveSyncObject().Unlock();
		}

	}
}