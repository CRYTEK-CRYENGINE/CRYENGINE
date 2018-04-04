#pragma once

enum ESerializationTarget { eST_SaveGame,	eST_Network };

struct SSerializeString {};
struct ISerializeUpdateFunction {};
struct ISerialize {
	virtual bool Serialize(void *val, int sz) { return Write((const void*)val,sz); }
	virtual bool Write(const void *val, int sz) { return false; }
	virtual bool OptionalGroup(bool present) { return present; }
};
struct ScriptAnyValue {};
struct CTimeValue {};
struct SNetObjectID {};
struct XmlNodeRef {};

template<class TSer> class CSerializeWrapper {
	TSer *ser;
public:
	CSerializeWrapper(TSer *ser) : ser(ser) {}
  static ESerializationTarget GetSerializationTarget() { return eST_SaveGame; }
	static void FlagPartialRead() {}
	template<class T> bool Value(const char*, T& val, int compr=0) { return ser->Serialize(&val,sizeof(T)); }
	template<class T> bool Value(const char*, const T& val, int compr=0) { return ser->Write(&val,sizeof(T)); }
	bool Value(const char*, const char*buf, int compr=0) { return ser->Write(buf,strlen(buf)+1); }
	bool BeginOptionalGroup(const char*, bool present) { return ser->OptionalGroup(present); }
	static void BeginGroup(const char*) {}
	static void EndGroup() {}
	static bool ShouldCommitValues() { return true; }
};

