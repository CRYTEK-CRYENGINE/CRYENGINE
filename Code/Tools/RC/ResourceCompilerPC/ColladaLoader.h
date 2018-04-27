// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   ColladaLoader.h
//  Version:     v1.00
//  Created:     3/4/2006 by Michael Smith
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __COLLADALOADER_H__
#define __COLLADALOADER_H__

#include <Cry3DEngine/CGF/CGFContent.h>
#include "../../CryXML/ICryXML.h"
#include <CrySystem/XML/IXml.h>
#include "Export/HelperData.h"
#include "Export/MeshUtils.h"

#include <cctype>

class ColladaNode;
class ColladaScene;
class CInternalSkinningInfo;

class IColladaLoaderListener
{
public:
	enum MessageType
	{
		MESSAGE_INFO,
		MESSAGE_WARNING,
		MESSAGE_ERROR
	};

	virtual ~IColladaLoaderListener()
	{
	}

	virtual void OnColladaLoaderMessage(MessageType type, const char* szMessage) = 0;
};

enum ColladaArrayType
{
	TYPE_FLOAT,
	TYPE_MATRIX,
	TYPE_INT,
	TYPE_NAME,
	TYPE_BOOL,
	TYPE_ID
};

enum ColladaNodeType
{
	NODE_NULL,
	NODE_CAMERA,
	NODE_CONTROLLER,
	NODE_GEOMETRY,
	NODE_LIGHT,
	NODE_NODE,
};

enum ColladaInterpType
{
	INTERP_CONST,
	INTERP_LINEAR,
	INTERP_BEZIER
};

enum ColladaTransformType
{
	TRANS_TRANSLATION,
	TRANS_ROTATION_X,
	TRANS_ROTATION_Y,
	TRANS_ROTATION_Z,
	TRANS_SCALE,
	TRANS_NEAR,
};

enum ExportType
{
	EXPORT_CGF,
	EXPORT_CGA,
	EXPORT_CHR,
	EXPORT_ANM,
	EXPORT_CAF,
	EXPORT_CGA_ANM,
	EXPORT_CHR_CAF,
	EXPORT_SKIN,
	EXPORT_INTERMEDIATE_CAF,
	EXPORT_UNKNOWN,
};

#define CRYEXPORTNODE_LOWERCASE "cryexportnode_"
#define CRYEXPORTNODE_LENGTH 14
#define CRYPHYSBONESUFFIX "phys"
#define CRYPHYSBONESUFFIX_LENGTH 4
#define CRYPHYSPARENTFRAMESUFFIX "phys parentframe"
#define CRYPHYSPARENTFRAMESUFFIX_LENGTH 16
#define ROOT_JOINT "Bip01"

#define LINE_LOG "#%d"

void ReportWarning(IColladaLoaderListener* pListener, const char* szFormat, ...);
void ReportInfo(IColladaLoaderListener* pListener, const char* szFormat, ...);
void ReportError(IColladaLoaderListener* pListener, const char* szFormat, ...);

// float reader (same as std::strtod() but faster)
float read_float(const char*& buf, char** endptr);

template <typename T> class ResourceMap : public std::map<string, T*>
{
public:
	~ResourceMap()
	{
		for (iterator itEntry = this->begin(); itEntry != this->end(); ++itEntry)
			delete (*itEntry).second;
	}
};

template <typename T> class ResourceVector : public std::vector<T*>
{
public:
	~ResourceVector()
	{
		for (iterator itEntry = this->begin(); itEntry != this->end(); ++itEntry)
			delete (*itEntry);
	}
};

typedef std::map<ColladaNode*, Matrix34> NodeTransformMap;

#pragma region Array

class IColladaArray
{
public:
	virtual ~IColladaArray() {}

	virtual bool ReadAsFloat(std::vector<float>& values, int stride, int offset, IColladaLoaderListener* pListener) const = 0;
	virtual bool ReadAsInt(std::vector<int>& values, int stride, int offset, IColladaLoaderListener* pListener) const = 0;
	virtual bool ReadAsName(std::vector<string>& values, int stride, int offset, IColladaLoaderListener* pListener) const = 0;
	virtual bool ReadAsBool(std::vector<bool>& values, int stride, int offset, IColladaLoaderListener* pListener) const = 0;
	virtual bool ReadAsID(std::vector<string>& values, int stride, int offset, IColladaLoaderListener* pListener) const = 0;

	virtual void Reserve(int size) = 0;
	virtual bool Parse(const char* buf, IColladaLoaderListener* pListener) = 0;
	virtual int GetSize() const = 0;
};

typedef ResourceMap<IColladaArray> ArrayMap;

template <ColladaArrayType TypeCode> class ArrayType {};

template <> class ArrayType<TYPE_FLOAT>
{
public: 
	typedef float Type;
	static const char* GetName() {return "FLOAT";}
	static float Parse(const char*& buf, bool& success)
	{
		char* end;
		float value = read_float(buf, &end);
		if (end == buf)
			success = false;
		else
			buf = end;
		return value;
	}
};

template <> class ArrayType<TYPE_INT>
{
public: 
	typedef int Type; 
	static const char* GetName() {return "INT";}
	static int Parse(const char*& buf, bool& success)
	{
		char* end;
		int value = int(std::strtol(buf, &end, 0));
		if (end == buf)
			success = false;
		else
			buf = end;
		return value;
	}
};

template <> class ArrayType<TYPE_NAME>
{
public:
	typedef string Type;
	static const char* GetName() {return "NAME";}
	static string Parse(const char*& buf, bool& success)
	{
		const char* p = buf;
		while (std::isspace(*p))
		{
			++p;
			if (*p == 0)
			{
				success = false;
				return string();
			}
		}

		string s;
		while (!std::isspace(*p) && *p != 0)
			s += *p++;

		buf = p;
		return s;
	}
};

template <> class ArrayType<TYPE_BOOL>
{
public: 
	typedef bool Type;
	static const char* GetName() {return "BOOL";}
	static bool Parse(const char*& buf, bool& success)
	{
		const char* p = buf;
		while (std::isspace(*p))
		{
			++p;
			if (*p == 0)
			{
				success = false;
				return false;
			}
		}

		string s;
		while (!std::isspace(*p) && *p != 0)
			s += tolower(*p++);

		buf = p;
		return s != "false";
	}
};

template <> class ArrayType<TYPE_ID>
{
public:
	typedef string Type;
	static const char* GetName() {return "ID";}
	static string Parse(const char*& buf, bool& success)
	{
		const char* p = buf;
		while (std::isspace(*p))
		{
			++p;
			if (*p == 0)
			{
				success = false;
				return string();
			}
		}

		string s;
		while (!std::isspace(*p) && *p != 0)
			s += *p++;

		buf = p;
		return s;
	}
};

template <ColladaArrayType TypeCode1, ColladaArrayType TypeCode2> class Converter
{
public:
	typedef typename ArrayType<TypeCode1>::Type DataType1;
	typedef typename ArrayType<TypeCode2>::Type DataType2;

	static void Convert(DataType1& out, DataType2 in) {assert(0);}
	static bool CheckConversion(IColladaLoaderListener* pListener)
	{
		ReportError(pListener, "Cannot convert values of type %s to type %s.", ArrayType<TypeCode2>::GetName(), ArrayType<TypeCode1>::GetName());
		return false;
	}
};

template <ColladaArrayType TypeCode1, ColladaArrayType TypeCode2> class ValidConverter
{
public:
	static bool CheckConversion(IColladaLoaderListener* pListener)
	{
		ReportInfo(pListener, "Converting values of type %s to %s.", ArrayType<TypeCode2>::GetName(), ArrayType<TypeCode1>::GetName());
		return true;
	}
};

template <ColladaArrayType TypeCode> class Converter<TypeCode, TypeCode>
{
public:
	typedef typename ArrayType<TypeCode>::Type DataType;

	static void Convert(DataType& out, DataType in)
	{
		out = in;
	}

	static bool CheckConversion(IColladaLoaderListener* pListener)
	{
		return true;
	}
};

template <> class Converter<TYPE_BOOL, TYPE_INT> : public ValidConverter<TYPE_BOOL, TYPE_INT>
{
public:
	static void Convert(bool& out, int in) {out = (in != 0);}
};

template <> class Converter<TYPE_INT, TYPE_BOOL> : public ValidConverter<TYPE_INT, TYPE_BOOL>
{
public:
	static void Convert(int& out, bool in) {out = int(in);}
};

template <> class Converter<TYPE_INT, TYPE_FLOAT> : public ValidConverter<TYPE_INT, TYPE_FLOAT>
{
public:
	static void Convert(int& out, float in) {out = int(in);}
};

template <> class Converter<TYPE_FLOAT, TYPE_INT> : public ValidConverter<TYPE_FLOAT, TYPE_INT>
{
public:
	static void Convert(float& out, int in) {out = float(in);}
};

template <ColladaArrayType TypeCode1, ColladaArrayType TypeCode2> class ArrayReader
{
public:
	typedef typename ArrayType<TypeCode1>::Type OutputType;
	typedef typename ArrayType<TypeCode2>::Type InputType;

	static bool Read(std::vector<OutputType>& values, int stride, int offset, IColladaLoaderListener* pListener, const std::vector<InputType>& data)
	{
		if (!Converter<TypeCode1, TypeCode2>::CheckConversion(pListener))
			return false;
		values.reserve(data.size() / stride + 1);
		for (int index = offset; index < int(data.size()); index += stride)
		{
			OutputType value = OutputType();
			Converter<TypeCode1, TypeCode2>::Convert(value, data[index]);
			values.push_back(value);
		}
		return true;
	}
};

template <ColladaArrayType TypeCode> class ColladaArray : public IColladaArray
{
public:
	typedef typename ArrayType<TypeCode>::Type DataType;

	virtual bool ReadAsFloat(std::vector<float>& values, int stride, int offset, IColladaLoaderListener* pListener) const
	{
		return ArrayReader<TYPE_FLOAT, TypeCode>::Read(values, stride, offset, pListener, this->data);
	}

	virtual bool ReadAsInt(std::vector<int>& values, int stride, int offset, IColladaLoaderListener* pListener) const
	{
		return ArrayReader<TYPE_INT, TypeCode>::Read(values, stride, offset, pListener, this->data);
	}

	virtual bool ReadAsName(std::vector<string>& values, int stride, int offset, IColladaLoaderListener* pListener) const
	{
		return ArrayReader<TYPE_NAME, TypeCode>::Read(values, stride, offset, pListener, this->data);
	}

	virtual bool ReadAsBool(std::vector<bool>& values, int stride, int offset, IColladaLoaderListener* pListener) const
	{
		return ArrayReader<TYPE_BOOL, TypeCode>::Read(values, stride, offset, pListener, this->data);
	}

	virtual bool ReadAsID(std::vector<string>& values, int stride, int offset, IColladaLoaderListener* pListener) const
	{
		return ArrayReader<TYPE_ID, TypeCode>::Read(values, stride, offset, pListener, this->data);
	}

	virtual void Reserve(int size)
	{
		this->data.reserve(size);
	}

	virtual bool Parse(const char* buf, IColladaLoaderListener* pListener)
	{
		this->data.clear();
		this->data.resize(100);

		// Loop until we have consumed the whole buffer.
		const char* p = buf;
		bool success = true;
		int index;
		for (index = 0; ; ++index)
		{
			while (isspace(*p))
				p++;

			if (*p == 0)
				break;

			if (size_t(index) >= this->data.size())
				this->data.resize(this->data.size() * 2);

			// Parse the next value.
			DataType value = ArrayType<TypeCode>::Parse(p, success);
			this->data[index] = value;
			if (!success)
				return false;
		}
		this->data.resize(index);
		return true;
	}

	virtual int GetSize() const
	{
		return int(this->data.size());
	}

private:
	std::vector<DataType> data;
};

template class ColladaArray<TYPE_FLOAT>;
typedef ColladaArray<TYPE_FLOAT> ColladaFloatArray;

template class ColladaArray<TYPE_INT>;
typedef ColladaArray<TYPE_INT> ColladaIntArray;

template class ColladaArray<TYPE_NAME>;
typedef ColladaArray<TYPE_NAME> ColladaNameArray;

template class ColladaArray<TYPE_BOOL>;
typedef ColladaArray<TYPE_BOOL> ColladaBoolArray;

template class ColladaArray<TYPE_ID>;
typedef ColladaArray<TYPE_ID> ColladaIDArray;

#pragma endregion Array

class IntStream
{
public:
	explicit IntStream(const char* text);
	int Read();

private:
	const char* position;
};

struct ColladaColor
{
	ColladaColor()
	{
		r = g = b = a = 0.0f;
	}
	ColladaColor(float r, float g, float b, float a)
	{
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
	}

	float r,g,b,a;
};

enum ColladaAuthorTool
{
	TOOL_UNKNOWN,
	TOOL_SOFTIMAGE_XSI,
	TOOL_AUTODESK_MAX,
	TOOL_MOTIONBUILDER,
	TOOL_MAYA
};

class ColladaAssetMetaData
{
public:
	enum EUpAxis 
	{
		eUA_X,
		eUA_Y,
		eUA_Z
	};

public:
	ColladaAssetMetaData()
		: m_upAxis(eUA_Z)
		, m_unitSizeInMeters(0.01f)
		, m_authorTool(TOOL_UNKNOWN)
		, m_authorToolVersion(0)
	{
	}
	ColladaAssetMetaData(const ColladaAssetMetaData &from)
	{
		set(from);
	}
	ColladaAssetMetaData& operator=(const ColladaAssetMetaData &from)
	{
		set(from);
		return *this;
	}
	void set(const ColladaAssetMetaData &from)
	{
		m_upAxis = from.m_upAxis;
		m_unitSizeInMeters = from.m_unitSizeInMeters;
		m_authorTool = from.m_authorTool;
		m_authorToolVersion = from.m_authorToolVersion;
	}
	void set(EUpAxis const upAxis, float const unitSizeInMeters, const ColladaAuthorTool& tool, const int toolVersion) 
	{ 
		m_upAxis = upAxis;
		m_authorTool = tool; 
		m_authorToolVersion = toolVersion;
		m_unitSizeInMeters = unitSizeInMeters; 
	}
	ColladaAuthorTool GetAuthorTool() const
	{ 
		return m_authorTool; 
	}
	int GetAuthorToolVersion() const
	{ 
		return m_authorToolVersion; 
	}
	const EUpAxis GetUpAxis() const 
	{
		return m_upAxis;
	}
	float GetUnitSizeInMeters() const 
	{
		return m_unitSizeInMeters;
	}

private:
	EUpAxis m_upAxis;
	float m_unitSizeInMeters;
	ColladaAuthorTool m_authorTool;
	int m_authorToolVersion;
};


struct SceneTransform
{
	ColladaAssetMetaData::EUpAxis upAxis;
	float scale;
};


class ColladaDataAccessor
{
public:
	ColladaDataAccessor(const IColladaArray* array, int stride, int offset, int size);
	void AddParameter(const string& name, ColladaArrayType type, int offset);

	int GetSize(const string& param) const;
	bool ReadAsFloat(const string& param, std::vector<float>& values, IColladaLoaderListener* pListener) const;
	bool ReadAsMatrix(const string& param, std::vector<Matrix34>& values, IColladaLoaderListener* pListener) const;
	bool ReadAsInt(const string& param, std::vector<int>& values, IColladaLoaderListener* pListener) const;
	bool ReadAsName(const string& param, std::vector<string>& values, IColladaLoaderListener* pListener) const;
	bool ReadAsBool(const string& param, std::vector<bool>& values, IColladaLoaderListener* pListener) const;
	bool ReadAsID(const string& param, std::vector<string>& values, IColladaLoaderListener* pListener) const;

private:
	struct Parameter
	{
		string name;
		ColladaArrayType type;
		int offset;
	};

	const IColladaArray* array;
	typedef std::map<string, Parameter> ParameterMap;
	ParameterMap parameters;
	int stride;
	int offset;
	int size;
};

// ------------------------------------------------------
class ColladaDataSource
{
public:
	explicit ColladaDataSource(ColladaDataAccessor* pDataAccessor);
	~ColladaDataSource();

	ColladaDataAccessor* GetDataAccessor() {return this->pDataAccessor;}

private:
	ColladaDataAccessor* pDataAccessor;
};
typedef ResourceMap<ColladaDataSource> DataSourceMap;

// ------------------------------------------------------
class ColladaVertexStream
{
public:
	explicit ColladaVertexStream(ColladaDataSource* pPositionDataSource);
	bool GetVertexPositions(std::vector<Vec3>& positions, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener) const;
	int GetVertexPositionCount() const;

private:
	ColladaDataSource* m_pPositionDataSource;
};
typedef ResourceMap<ColladaVertexStream> VertexStreamMap;

// ------------------------------------------------------
class ColladaImage
{
public:
	explicit ColladaImage(const string& name);
	const string& GetName() const { return this->name; }
	const string& GetFileName() const { return this->filename; }
	const string& GetRelativeFileName() const { return this->rel_filename; }

	void SetFileName(const string& name);

private:
	string name;
	string filename;
	string rel_filename;
};
typedef ResourceMap<ColladaImage> ImageMap;

// ------------------------------------------------------
class ColladaEffect
{
public:
	explicit ColladaEffect(const string& name);
	const string& GetName() { return this->name; }
	void SetShader(const string& shader) { this->shader = shader; }
	const string& GetShader() { return this->shader; }

	void SetAmbient(ColladaColor& color) { this->ambient = color; }
	void SetDiffuse(ColladaColor& color) { this->diffuse = color; }
	void SetEmission(ColladaColor& color) { this->emission = color; }
	void SetSpecular(ColladaColor& color) { this->specular = color; }
	void SetReflective(ColladaColor& color) { this->reflective = color; }
	void SetTransparent(ColladaColor& color) { this->transparent = color; }

	void SetShininess(float val) { this->shininess =  val; }
	void SetReflectivity(float val) { this->reflectivity =  val; }
	void SetTransparency(float val) { this->transparency = val; }

	void SetDiffuseImage(const ColladaImage* image) { this->diffuseImage = image; }
	void SetSpecularImage(const ColladaImage* image) { this->specularImage = image; }
	void SetNormalImage(const ColladaImage* image) { this->normalImage = image; }

	const ColladaColor& GetAmbient() const { return this->ambient; }
	const ColladaColor& GetDiffuse() const { return this->diffuse; }
	const ColladaColor& GetEmission() const { return this->emission; }
	const ColladaColor& GetSpecular() const { return this->specular; }
	const ColladaColor& GetReflective() const { return this->reflective; }
	const ColladaColor& GetTransparent() const { return this->transparent; }

	float GetShininess() const { return this->shininess; }
	float GetReflectivity() const { return this->reflectivity; }
	float GetTransparency() const { return this->transparency; }

	const ColladaImage* GetDiffuseImage() const { return this->diffuseImage; }
	const ColladaImage* GetSpecularImage() const { return this->specularImage; }
	const ColladaImage* GetNormalImage() const { return this->normalImage; }

private:
	string name;
	string shader;

	ColladaColor ambient;
	ColladaColor diffuse;
	ColladaColor emission;
	ColladaColor specular;
	ColladaColor reflective;
	ColladaColor transparent;

	float shininess;
	float reflectivity;
	float transparency;

	const ColladaImage* diffuseImage;
	const ColladaImage* specularImage;
	const ColladaImage* normalImage;
};
typedef ResourceMap<ColladaEffect> EffectMap;

// ------------------------------------------------------
class ColladaMaterial
{
public:
	explicit ColladaMaterial(const string& name);
	const string& GetFullName() const { return this->fullname; }
	const string& GetLibrary() const { return this->library; }
	const string& GetMatName() const { return this->matname; }

	void SetEffect(ColladaEffect* effect) { this->effect = effect; }
	ColladaEffect* GetEffect() { return this->effect; }


private:
	string library;
	string fullname;
	string matname;

	ColladaEffect* effect;
};
typedef ResourceMap<ColladaMaterial> MaterialMap;

// ------------------------------------------------------
class CGFMaterialMapEntry
{
public:
	CGFMaterialMapEntry(int id, CMaterialCGF* material) : id(id), material(material) {}
	int id;
	CMaterialCGF* material;
};
typedef std::map<const ColladaMaterial*, CGFMaterialMapEntry> CGFSubMaterialMap;
typedef std::map<CMaterialCGF*, CGFSubMaterialMap> CGFMaterialMap;
typedef std::map<ColladaNode*, CMaterialCGF*> NodeMaterialMap;

// ------------------------------------------------------
struct sTexture
{
	string type;
	string filename;
};

struct sMaterial
{
	string name;
	DWORD flag;
	string shader;
	string surfacetype;
	ColladaColor diffuse;
	ColladaColor specular;
	ColladaColor emissive;
	float shininess;
	float opacity;

	std::vector<sTexture> textures;
};

struct sMaterialLibrary
{
	string library;
	DWORD flag;
	std::vector<sMaterial> materials;
};

// ------------------------------------------------------

class IMeshPiece
{
public:
	virtual ~IMeshPiece() {}

	virtual bool ParseNode(XmlNodeRef& node, const MaterialMap& materials, const VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, IColladaLoaderListener* pListener) = 0;
	virtual ColladaMaterial* GetMaterial() = 0;
	virtual bool CreateMeshUtilsMesh(MeshUtils::Mesh* mesh, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener, std::vector<int>& newToOldPositionMapping) = 0;
};

class BaseMeshPiece : public IMeshPiece
{
public:
	BaseMeshPiece();

	virtual bool ParseNode(XmlNodeRef& node, const MaterialMap& materials, const VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);
	virtual ColladaMaterial* GetMaterial();
	virtual bool CreateMeshUtilsMesh(MeshUtils::Mesh* mesh, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener, std::vector<int>& newToOldPositionMapping);

protected:
	void SetVertexStream(ColladaVertexStream* vertexStream);
	void SetNormalSource(ColladaDataSource* normalSource);
	void SetTexCoordSource(ColladaDataSource* texCoordSource);
	void SetColorSource(ColladaDataSource* colorSource);

	int numIndices;
	std::vector<int> vertexIndices;
	std::vector<int> normalIndices;
	std::vector<int> texcoordIndices;
	std::vector<int> colorIndices;

	int indexGroupSize;  // number of columns of <p> (ex. 3, if the polylist contains "VERTEX", "NORMAL", "TEXCOORD")

	ColladaVertexStream* vertexStream;
	int positionIndexOffset;
	ColladaDataSource* normalSource;
	int normalIndexOffset;
	ColladaDataSource* texCoordSource;
	int texCoordIndexOffset;
	ColladaDataSource* colorSource;
	int colorIndexOffset;

	ColladaMaterial* material;
};

class BasePolygonMeshPiece : public BaseMeshPiece
{
public:
	virtual bool CreateMeshUtilsMesh(MeshUtils::Mesh* mesh, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener, std::vector<int>& newToOldPositionMapping);

protected:
	std::vector<int> polygonSizes;  // content of <vcount> node
};

class PolylistMeshPiece : public BasePolygonMeshPiece
{
public:
	virtual bool ParseNode(XmlNodeRef& node, const MaterialMap& materials, const VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);
};

class TrianglesMeshPiece : public BasePolygonMeshPiece
{
public:
	virtual bool ParseNode(XmlNodeRef& node, const MaterialMap& materials, const VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);
};

// ------------------------------------------------------
class ColladaGeometry
{
public:
	ColladaGeometry(const string& name, const std::vector<IMeshPiece*>& meshPieces, const ColladaVertexStream* vertexStream);
	~ColladaGeometry();

	CMesh* CreateCMesh(
		const CGFSubMaterialMap& cgfMaterials, 
		const SceneTransform& sceneTransform, 
		IColladaLoaderListener* pListener, 
		std::vector<int>& meshToVertexListIndexMapping, 
		const string& nodeName, 
		const std::vector<MeshUtils::VertexLinks>& boneMapping, 
		const bool isMorph) const;
	bool GetVertexPositions(std::vector<Vec3>& positions, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener) const;
	size_t GetVertexPositionCount() const;
	const string& GetName() const;

private:
	string m_name;
	std::vector<IMeshPiece*> m_meshPieces;
	const ColladaVertexStream* m_vertexStream;
};

typedef ResourceMap<ColladaGeometry> GeometryMap;

// ------------------------------------------------------
class ColladaController
{
public:
	explicit ColladaController(const ColladaGeometry* pGeometry);

	bool ParseVertexWeights(XmlNodeRef node, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);
	bool ParseBoneMap(XmlNodeRef node, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);
	bool ParseBoneMatrices(XmlNodeRef node, const DataSourceMap& dataSources, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	void ReduceBoneMap(XmlNodeRef node, IColladaLoaderListener* pListener);
	void ReIndexBoneIndices(const string& rootJoint);

	void SetShapeMatrix(const Matrix34& m) { shapeMatrix = m; }
	const Matrix34& GetShapeMatrix() const { return shapeMatrix; }

	const Matrix34& GetJointMatrix(int index) const { return this->baseMatrices[index]; }

	const ColladaGeometry* GetGeometry() const { return this->pGeometry; }
	void GetMeshBoneMapping(std::vector<MeshUtils::VertexLinks>& meshbonemap) const { meshbonemap = this->reducedBoneMapping; }
	int GetNumJoints() const { return this->jointList.size(); }
	const string& GetJointID(int index) const { return this->jointList[index]; }

	int GetJointIndex(const string& name) const;

	void AddMorphTarget(const string& name, const ColladaGeometry* geometry) {this->morphTargets.push_back(MorphTarget(name, geometry));}
	int GetMorphTargetCount() const {return this->morphTargets.size();}
	const string& GetMorphTargetName(int morphTargetIndex) const {return this->morphTargets[morphTargetIndex].name;}
	const ColladaGeometry* GetMorphTargetGeometry(int morphTargetIndex) const {return this->morphTargets[morphTargetIndex].geometry;}

private:
	struct MorphTarget
	{
		MorphTarget(const string& name, const ColladaGeometry* geometry)
			: name(name)
			, geometry(geometry)
		{
		}

		string name;
		const ColladaGeometry* geometry;
	};

	int indexGroupSize;

	ColladaDataSource* jointListSource;  // joint list data source (only for error-check the references)

	Matrix34 shapeMatrix;                // shape matrix... for what? [MichaelS] This transforms the skin node into its bind pose position (can often be identity).
	std::vector<string> jointList;       // bone list for mesh
	std::vector<Matrix34> baseMatrices;  // bone base matrices (world->local transforms)
	std::vector<float> weightList;       // weight list
	std::vector<int> weightSizes;        // content of 'vcount' node
	int jointIndexOffset;
	int weightIndexOffset;
	std::vector<MeshUtils::VertexLinks> boneMapping;  // this size equal with positions size in collada mesh
	std::vector<MeshUtils::VertexLinks> reducedBoneMapping;

	const ColladaGeometry* pGeometry;

	std::vector<MorphTarget> morphTargets;
};

typedef ResourceMap<ColladaController> ControllerMap;

// ------------------------------------------------------
class ColladaNode
{
public:
	ColladaNode(const string& id, const string& name, ColladaNode* parent, int nodeIndex);

	void SetProperty(const string& prop) { this->prop = prop; }
	void SetHelperData(const SHelperData& helperData) { this->helperData = helperData; }
	void SetType(ColladaNodeType type) { this->type = type; }
	void SetTransform(Matrix34& transform) { this->transform = transform; }
	void SetGeometry(const ColladaGeometry* geometry) { this->pGeometry = geometry; }
	void SetController(ColladaController* controller) { this->pController = controller; }

	const string& GetName() const { return this->name; }
	const string& GetID() const { return this->id; }
	const string& GetProperty() const { return this->prop; }
	const SHelperData& GetHelperData() const { return this->helperData; }
	ColladaNodeType GetType() const { return this->type; }
	
	ColladaNode* GetParent() { return this->parent; }
	int GetNumChild() const { return this->children.size(); }
	ColladaNode* GetChild(int index) { return this->children[index]; }
	const ColladaNode* GetChild(int index) const { return this->children[index]; }
	void AddChild(ColladaNode* node) { this->children.push_back(node); }

	void GetDecomposedTransform(
		Vec3& tmTranslation,
		Quat& tmRotationQ,
		Vec4& tmRotationX,
		Vec4& tmRotationY,
		Vec4& tmRotationZ,
		Vec3& tmScale) const;

	const Matrix34& GetTransform() const { return this->transform; }

	int GetIndex() const { return nodeIndex; }

	void AddSubMaterial(ColladaMaterial* material);
	void GetSubMaterials(std::vector<ColladaMaterial*>& materials);
	const ColladaGeometry* GetGeometry() const { return this->pGeometry; }
	const ColladaController* GetController() const { return this->pController; }

	CNodeCGF* CreateNodeCGF(CMaterialCGF* rootMaterial, const CGFSubMaterialMap& cgfMaterials, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener) const;

private:
	string id;
	string name;
	string prop;
	ColladaNodeType type;
	ColladaNode* parent;
	std::vector<ColladaNode*> children;
	Matrix34 transform;

	const ColladaGeometry* pGeometry;
	ColladaController* pController;
	std::vector<ColladaMaterial*> materials;

	SHelperData helperData;

	int nodeIndex;				// each collada node gets a new index
};

typedef std::map<string, ColladaNode*> NameNodeMap;

// ------------------------------------------------------
class ColladaAnimation;

class ColladaAnimClip
{
public:
	ColladaAnimClip(float startTime, float endTime, const string& animClipID, ColladaScene* targetScene);
	~ColladaAnimClip();

	float GetStartTime() const { return startTime; }
	float GetEndTime() const { return endTime; }
	const string& GetAnimClipID() const { return animClipID; }
	ColladaScene* GetTargetScene() { return targetScene; }

	int GetNumAnim() const { return animations.size(); }
	ColladaAnimation* GetAnim(int index) { return animations[index]; }
	const ColladaAnimation* GetAnim(int index) const { return animations[index]; }

	void AddAnim(ColladaAnimation* anim) { this->animations.push_back(anim); }

private:
	std::vector<ColladaAnimation*> animations;

	float startTime;
	float endTime;
	string animClipID;
	ColladaScene* targetScene;
};

typedef ResourceMap<ColladaAnimClip> AnimClipMap;

// ------------------------------------------------------
struct CryExportNodeParams
{
	CryExportNodeParams()
	{
		exportType = EXPORT_UNKNOWN;
		mergeAllNodes = false;
		useCustomNormals = false;
		useF32VertexFormat = false;
		eightWeightsPerVertex = false;
		useVClothPreProcess = false;
	}

	string fullName;
	string nodeName;
	string customExportPath;
	ExportType exportType;
	bool mergeAllNodes;
	bool useCustomNormals;
	bool useF32VertexFormat;
	bool eightWeightsPerVertex;
	bool useVClothPreProcess;
};

enum ContentFlags
{
	ContentFlags_Geometry = 0x01,
	ContentFlags_Animation = 0x02
};

struct PhysBone
{
	enum Axis
	{
		AxisX,
		AxisY,
		AxisZ
	};
	enum Limit
	{
		LimitMin,
		LimitMax
	};
	typedef std::pair<Axis, Limit> AxisLimit;
	typedef std::map<AxisLimit, float> AxisLimitLimitMap;
	typedef std::map<Axis, float> AxisSpringTensionMap;
	typedef std::map<Axis, float> AxisSpringAngleMap;
	typedef std::map<Axis, float> AxisDampingMap;

	explicit PhysBone(const string& name): name(name) {}
	string name;
	AxisLimitLimitMap limits;
	AxisSpringTensionMap springTensions;
	AxisSpringAngleMap springAngles;
	AxisDampingMap dampings;
};

typedef std::map<string, PhysBone> PhysBoneMap;

class ColladaScene
{
public:
	explicit ColladaScene(CryExportNodeParams& a_nodeParams)
		: nodeParams(a_nodeParams)
	{
	}

	~ColladaScene();

	void AddNode(ColladaNode* pNode);

	CContentCGF* CreateContentCGF(
		const string& filename, 
		const NameNodeMap& nodes, 
		const SceneTransform& sceneTransform, 
		NodeTransformMap& nodeTransformMap,
		NodeMaterialMap& nodeMaterialMap, 
		CGFMaterialMap& cgfMaterials,
		IColladaLoaderListener* pListener,
		int contentFlags);
	void CreateMaterial(CGFMaterialMap& cgfMaterials, const NameNodeMap& nameNodeMap, NodeMaterialMap& nodeMaterialMap, IColladaLoaderListener* pListener);
	
	// skinning
	void AddBoneRecursive(ColladaNode *node, int parent_id, CSkinningInfo *pSkinningInfo, const ColladaController* controller, const NameNodeMap& nodes, const std::map<string, int>& boneNameIndexMap);
	bool SetSkinningInfoCGF(CContentCGF* cgf, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener, const NameNodeMap& nameNodeMap);
	bool SetBoneParentFrames(CContentCGF* cgf, IColladaLoaderListener* pListener);
	bool SetIKParams(CContentCGF* cgf, IColladaLoaderListener* pListener);
	bool AddCollisionMeshesCGF(CContentCGF* cgf, const SceneTransform& sceneTransform, const NameNodeMap& nameNodeMap, CGFMaterialMap& cgfMaterials, NodeMaterialMap& nodeMaterialMap, IColladaLoaderListener* pListener);
	bool AddMorphTargetsCGF(CContentCGF* cgf, const SceneTransform& sceneTransform, const NodeTransformMap& nodeTransformMap, IColladaLoaderListener* pListener);
	void GetAnimClipList(AnimClipMap& animclips, std::vector<ColladaAnimClip*>& animClipList, IColladaLoaderListener* pListener);
	CInternalSkinningInfo* CreateControllerTCBSkinningInfo(CContentCGF* cgf, const ColladaAnimClip* animClip, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	CInternalSkinningInfo* CreateControllerSkinningInfo(CContentCGF* cgf, const ColladaAnimClip* animClip, const NameNodeMap& nodes, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);

	NameNodeMap * GetNodesNameMap() { return &nodeNames; }
	const string& GetExportNodeName() const { return nodeParams.nodeName; }
	const string& GetCustomExportPath() const { return nodeParams.customExportPath; }
	ExportType GetExportType() const { return nodeParams.exportType; }
	bool NeedToMerge() const
	{
		// If the export type isn't CGF then this scene is not mergeable
		if (nodeParams.exportType != EXPORT_CGF)
			return false;

		return nodeParams.mergeAllNodes;
	}

	void SetPhysBones(PhysBoneMap& physBones);
	void SetPhysParentFrames(std::map<string, Matrix33>& physParentFrames);

private:
	std::vector<ColladaNode*> nodes;
	NameNodeMap nodeNames;
	CryExportNodeParams nodeParams;
	PhysBoneMap physBones;
	std::map<string, Matrix33> physParentFrames;
};

typedef ResourceMap<ColladaScene> SceneMap;

// ------------------------------------------------------

struct ColladaKey
{
	float time;
	float value;
	Vec2 inTangent;
	Vec2 outTangent;
	ColladaInterpType interp;
	Vec3 tcb;
	Vec2 easeInOut;
};

class ColladaAnimation
{
public:
	ColladaAnimation();
	~ColladaAnimation();

	bool ParseChannel(XmlNodeRef channelNode, XmlNodeRef samplerNode, const SceneMap& scenes, const DataSourceMap& dataSources, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	void ParseName(const string& name);

	int GetKeys() const { return inputData.size(); }
	void GetKey(int index, ColladaKey& colladaKey) const;
	float GetDuration() const;
	float GetValue(float time) const;

	ColladaNode* GetTargetNode() const { return targetNode; }
	ColladaTransformType GetTargetTransform() const { return targetTransform; }
	const string& GetTargetComponent() const { return targetComponent; }

	enum Flags
	{
		Flags_NoExport = 1 << 0
	};
	void SetFlags(unsigned flags) {this->flags = flags;}
	unsigned GetFlags() const {return this->flags;}

private:
	std::vector<float> inputData;               // time (x) datas
	std::vector<float> outputData;              // value (y) datas
	std::vector<Vec2> inTangentData;            // in-tangents (absolute x,y coordinates) - only for bezier interpolation
	std::vector<Vec2> outTangentData;           // out-tangents (absolute x,y coordinates) - only for bezier interpolation
	std::vector<ColladaInterpType> interpData;  // interpolation types
	std::vector<Vec3> tcbData;                  // TCB data
	std::vector<Vec2> easeInOutData;            // Ease-In/-Out data

	std::vector<ColladaKey> colladaKeys;        // compressed key format (time, value, in-out tangents, interpolation)

	ColladaNode* targetNode;                    // animated node
	ColladaTransformType targetTransform;       // animated transformation (S,R,T)
	string targetComponent;                     // animated transformation component (X,Y,Z,Angle)

	unsigned flags;                             // Flags for curve - specified in name of curve, which is otherwise not used.

	bool ParseSamplerSource(XmlNodeRef node, const DataSourceMap& dataSources, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	bool ParseTransformTarget(XmlNodeRef node, const SceneMap& scenes, IColladaLoaderListener* pListener);
};

typedef ResourceMap<ColladaAnimation> AnimationMap;

// ------------------------------------------------------

// Declare a stack to handle the recursion.
class NodeStackEntry
{
public:
	explicit NodeStackEntry(XmlNodeRef a_parentXmlNode)
		: parentXmlNode(a_parentXmlNode)
		, parentColladaNode(nullptr)
		, childIndex(0)
		, cryExportNode(false)
	{
	}

	NodeStackEntry(const NodeStackEntry& other)
		: parentXmlNode(other.parentXmlNode)
		, parentColladaNode(other.parentColladaNode)
		, childIndex(other.childIndex)
		, cryExportNode(other.cryExportNode)
	{
	}
	
	~NodeStackEntry() {}

	NodeStackEntry& operator=(const NodeStackEntry& other)
	{
		parentXmlNode = other.parentXmlNode;
		parentColladaNode = other.parentColladaNode;
		childIndex = other.childIndex;
		cryExportNode = other.cryExportNode;
		return *this;
	}

	XmlNodeRef parentXmlNode;
	ColladaNode* parentColladaNode;
	int childIndex;
	bool cryExportNode;
};

struct ExportFile
{
	ColladaAssetMetaData metaData;
	ExportType type;
	string name;
	string customExportPath;
	CContentCGF* pCGF;
	CInternalSkinningInfo* pCtrlSkinningInfo;
};

typedef std::multimap<string, XmlNodeRef> TagMultimap;

class ColladaLoaderImpl
{
public:
	bool Load(std::vector<ExportFile>& exportFiles, std::vector<sMaterialLibrary>& materialLibraryList, const char* szFileName, ICryXML* pCryXML, IPakSystem* pPakSystem, IColladaLoaderListener* pListener);

private:
	XmlNodeRef LoadXML(const char* szFileName, IPakSystem* pPakSystem, ICryXML* pCryXML, IColladaLoaderListener* pListener);

	bool ParseColladaVector(XmlNodeRef node, Vec2& param, IColladaLoaderListener* pListener);
	bool ParseColladaVector(XmlNodeRef node, Vec3& param, IColladaLoaderListener* pListener);
	bool ParseColladaVector(XmlNodeRef node, Vec4& param, IColladaLoaderListener* pListener);
	bool ParseColladaMatrix(XmlNodeRef node, Matrix34& param, IColladaLoaderListener* pListener);
	bool ParseColladaFloatList(XmlNodeRef node, int listSize, std::vector<float>& values, IColladaLoaderListener* pListener);
	bool ParseColladaColor(XmlNodeRef node, const string& parentTag, ColladaColor& color, IColladaLoaderListener* pListener);
	bool ParseColladaFloatParam(XmlNodeRef node, const string& parentTag, float& param, IColladaLoaderListener* pListener);

	bool ParseElementIdsAndTags(XmlNodeRef node, std::map<string, XmlNodeRef>& idMap, TagMultimap& tagMap, IColladaLoaderListener* pListener);

	void ParseMetaData(XmlNodeRef node, ColladaAssetMetaData* pMetadata);

	bool ParseImages(ImageMap& images, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaImage* ParseImage(XmlNodeRef node, IColladaLoaderListener* pListener);

	bool ParseEffects(EffectMap& effects, const ImageMap& images, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaEffect* ParseEffect(XmlNodeRef node, const ImageMap& images, IColladaLoaderListener* pListener);

	bool ParseMaterials(MaterialMap& materials, const EffectMap& effects, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaMaterial* ParseMaterial(XmlNodeRef node, const EffectMap& effects, IColladaLoaderListener* pListener);

	bool ParseArrays(ArrayMap& arrays, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	IColladaArray* ParseArray(XmlNodeRef node, ColladaArrayType type, IColladaLoaderListener* pListener);

	bool ParseDataSources(DataSourceMap& sources, const ArrayMap& arrays, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaDataSource* ParseDataSource(XmlNodeRef node, const ArrayMap& arrays, IColladaLoaderListener* pListener);

	bool ParseVertexStreams(VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaVertexStream* ParseVertexStream(XmlNodeRef node, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);

	bool ParseGeometries(GeometryMap& geometries, const MaterialMap& materials, const VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaGeometry* ParseGeometry(XmlNodeRef node, const MaterialMap& materials, const VertexStreamMap& vertexStreams, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);

	bool ParseMorphControllers(ControllerMap& morphControllers, const GeometryMap& geometries, const DataSourceMap& dataSources, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaController* ParseMorphController(XmlNodeRef node, const GeometryMap& geometries, const DataSourceMap& dataSources, IColladaLoaderListener* pListener);

	bool ParseSkinControllers(ControllerMap& skinControllers, const ControllerMap& morphControllers, const GeometryMap& geometries, const DataSourceMap& dataSources, const TagMultimap& tagMap, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	ColladaController* ParseSkinController(XmlNodeRef node, const ControllerMap& morphControllers, const GeometryMap& geometries, const DataSourceMap& dataSources, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);

	bool ParseSceneDefinitions(SceneMap& scenes, const GeometryMap& geometries, const ControllerMap& controllers, const MaterialMap& materials, const TagMultimap& tagMap, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	void ParseSceneDefinition(SceneMap& scenes, XmlNodeRef sceneNode, const GeometryMap& geometries, const ControllerMap& controllers, const MaterialMap& materials, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);

	bool ParseAnimations(AnimationMap& animations, const SceneMap& scenes, const DataSourceMap& dataSources, const TagMultimap& tagMap, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);
	ColladaAnimation* ParseAnimation(XmlNodeRef node, const SceneMap& scenes, const DataSourceMap& dataSources, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);

	bool ParseAnimClips(AnimClipMap& animclips, const AnimationMap& animations, const SceneMap& scenes, const TagMultimap& tagMap, IColladaLoaderListener* pListener);
	ColladaAnimClip* ParseAnimClip(XmlNodeRef node, const AnimationMap& animations, const SceneMap& scenes, IColladaLoaderListener* pListener);

	void ReadXmlNodeTransform(ColladaNode* colladaNode, XmlNodeRef node, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);

	string ReadXmlNodeUserProperties(XmlNodeRef node, IColladaLoaderListener* pListener);
	SHelperData ReadXmlNodeHelperData(XmlNodeRef node, const SceneTransform& sceneTransform, IColladaLoaderListener* pListener);

	bool ParseInstanceNull(ColladaNode* colladaNode, XmlNodeRef& instanceNode, XmlNodeRef& nodeNode, IColladaLoaderListener* pListener);
	bool ParseInstanceCamera(ColladaNode* colladaNode, XmlNodeRef& instanceNode, XmlNodeRef& nodeNode, IColladaLoaderListener* pListener);
	bool ParseInstanceController(ColladaNode* colladaNode, XmlNodeRef& instanceNode, XmlNodeRef& nodeNode, const ControllerMap& controllers, const MaterialMap& materials, IColladaLoaderListener* pListener);
	bool ParseInstanceGeometry(ColladaNode* colladaNode, XmlNodeRef& instanceNode, XmlNodeRef& nodeNode, const GeometryMap& geometries, const MaterialMap& materials, IColladaLoaderListener* pListener);
	bool ParseInstanceLight(ColladaNode* colladaNode, XmlNodeRef& instanceNode, XmlNodeRef& nodeNode, IColladaLoaderListener* pListener);
	bool ParseInstanceNode(ColladaNode* colladaNode, XmlNodeRef& instanceNode, XmlNodeRef& nodeNode, IColladaLoaderListener* pListener);

	ColladaImage* GetImageFromTextureParam(XmlNodeRef profileNode, XmlNodeRef textureNode, const ImageMap& images, IColladaLoaderListener* pListener);

	void CreateMaterialLibrariesContent(std::vector<sMaterialLibrary>& materialLibraryList, const MaterialMap& materials);
};


bool LoadColladaFile(std::vector<ExportFile>& exportFiles, std::vector<sMaterialLibrary>& materialLibraryList, const char* szFileName, ICryXML* pCryXML, IPakSystem* pPakSystem, IColladaLoaderListener* pListener);

#endif //__COLLADALOADER_H__
