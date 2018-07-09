// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

class ScopedLock
{
public:
	ScopedLock(bool& var) : m_var(var) { m_var = true; }
	~ScopedLock() { m_var = false; }

private:
	bool& m_var;
};

#define RECURSION_GUARD(VAR, ...) \
  if (VAR)                        \
    return __VA_ARGS__;           \
  ScopedLock scopedLock(VAR);

#define STATIC_RECURSION_GUARD(...)  \
  static bool feedbackGuard = false; \
  if (feedbackGuard)                 \
    return __VA_ARGS__;              \
  ScopedLock scopedLock(feedbackGuard);

#define RECURSION_GUARD_SCOPE_START(...) \
  {                                      \
    STATIC_RECURSION_GUARD(__VA_ARGS__);

#define RECURSION_GUARD_SCOPE_END() }

