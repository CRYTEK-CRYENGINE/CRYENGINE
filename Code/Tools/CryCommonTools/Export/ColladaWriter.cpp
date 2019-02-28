// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ColladaWriter.h"
#include "IExportSource.h"
#include "ISettings.h"
#include "XMLWriter.h"
#include "SkeletonData.h"
#include "AnimationData.h"
#include "ProgressRange.h"
#include "ModelData.h"
#include "IExportContext.h"
#include "GeometryFileData.h"
#include "GeometryData.h"
#include "MaterialData.h"
#include "SkinningData.h"
#include <CryMath/Cry_Math.h>
#include "LocaleChanger.h"
#include "MorphData.h"
#include "StringHelpers.h"
#include "GeometryMaterialData.h"
#include <cstdio>
#include <ctime>
#include <cctype>
#include <locale.h>
#include "PathHelpers.h"

namespace
{
	bool FloatingPointHasPrecisionIssues()
	{
		Matrix44 m;

		m.m00 = 0.729367f;
		m.m01 = -0.143863f;
		m.m02 = -0.668825f;
		m.m03 = 0.595435f;

		m.m10 = -0.573746f;
		m.m11 = 0.403844f;
		m.m12 = -0.712549f;
		m.m13 = 1.14523f;

		m.m20 = 0.37261f;
		m.m21 = 0.903445f;
		m.m22 = 0.21201f;
		m.m23 = 0.0669039f;

		m.m30 = 0;
		m.m31 = 0;
		m.m32 = 0;
		m.m33 = 1;

		m.Invert();

		return m.m33 <= 0.999f || m.m33 >= 1.001f;

		// For testing with www.wolframalpha.com:
		// string a = StringHelpers::Format("inverse{{%g,%g,%g,%g},{%g,%g,%g,%g},{%g,%g,%g,%g},{%g,%g,%g,%g}}",
		//	m.m00, m.m01, m.m02, m.m03,
		//	m.m10, m.m11, m.m12, m.m13,
		//	m.m20, m.m21, m.m22, m.m23,
		//	m.m30, m.m31, m.m32, m.m33);
	}
}


namespace
{
	void DecomposeTransform(Vec3& translation, CryQuat& rotation, Vec3& scale, const Matrix34& transform)
	{
		translation = transform.GetTranslation();
		Matrix33 orientation(transform);
		scale.x = Vec3(orientation.m00,orientation.m10,orientation.m20).GetLength();
		scale.y = Vec3(orientation.m01,orientation.m11,orientation.m21).GetLength();
		scale.z = Vec3(orientation.m02,orientation.m12,orientation.m22).GetLength();
		orientation.OrthonormalizeFast();
		rotation = !CryQuat(orientation);
	}

	float FPS = 30.0f;

	struct BoneEntry
	{
		std::string name;
		std::string physName;
		std::string parentFrameName;
	};

	typedef std::map<std::pair<int, int>, SkeletonData> SkeletonDataMap;
	typedef std::map<std::pair<int, int>, MorphData> MorphDataMap;
	typedef std::map<std::pair<int, int>, std::vector<BoneEntry> > BoneDataMap;

	struct GeometryEntry
	{
		GeometryEntry(const std::string& name, int geometryFileIndex, int modelIndex)
			: name(name), geometryFileIndex(geometryFileIndex), modelIndex(modelIndex) {}
		std::string name;
		int geometryFileIndex;
		int modelIndex;
	};

	struct BoneGeometryEntry
	{
		BoneGeometryEntry(const std::string& name, int geometryFileIndex, int modelIndex, int boneIndex)
			: name(name), geometryFileIndex(geometryFileIndex), modelIndex(modelIndex), boneIndex(boneIndex) {}
		std::string name;
		int geometryFileIndex;
		int modelIndex;
		int boneIndex;
	};

	struct MorphGeometryEntry
	{
		MorphGeometryEntry(const std::string& name, const std::string& morphName, int geometryFileIndex, int modelIndex, int morphIndex)
			: name(name), morphName(morphName), geometryFileIndex(geometryFileIndex), modelIndex(modelIndex), morphIndex(morphIndex) {}
		std::string name;
		std::string morphName;
		int geometryFileIndex;
		int modelIndex;
		int morphIndex;
	};

	struct EffectsEntry
	{
		explicit EffectsEntry(const std::string& name): name(name) {}

		std::string name;
	};

	struct MaterialEntry
	{
		explicit MaterialEntry(const std::string& name): name(name) {}

		std::string name;
	};

	struct SkinControllerEntry
	{
		std::string name;
		int geometryFileIndex;
		int modelIndex;
	};

	struct MorphControllerEntry
	{
		std::string name;
		int geometryFileIndex;
		int modelIndex;
	};

	typedef std::map<std::pair<int, int>, MorphControllerEntry> MorphControllerMap;

	void BindMaterials(XMLWriter& writer, IExportContext* context, MaterialData& materialData, const ModelData& modelData, int modelIndex, const std::map<int, int>& materialMaterialMap, const std::vector<MaterialEntry>& materials, IExportSource* source)
	{
		// Instance any materials that the node uses.
		GeometryMaterialData geometryMaterialData;
		source->ReadGeometryMaterialData(context, &geometryMaterialData, &modelData, &materialData, modelIndex);

		std::set<int> usedMaterialIndices;
		for (int materialIndex = 0, materialCount = geometryMaterialData.GetUsedMaterialCount(); materialIndex < materialCount; ++materialIndex)
			usedMaterialIndices.insert(geometryMaterialData.GetUsedMaterialIndex(materialIndex));
		if (!usedMaterialIndices.empty())
		{
			XMLWriter::Element bindMaterialElement(writer, "bind_material");
			XMLWriter::Element techniqueCommonElement(writer, "technique_common");
			for (std::set<int>::const_iterator usedMtlPos = usedMaterialIndices.begin(), usedMtlEnd = usedMaterialIndices.end(); usedMtlPos != usedMtlEnd; ++usedMtlPos)
			{
				std::map<int, int>::const_iterator entryMapPos = materialMaterialMap.find(*usedMtlPos);
				int entryIndex = (entryMapPos != materialMaterialMap.end() ? (*entryMapPos).second : -1);
				std::string name = (entryIndex >= 0 ? materials[entryIndex].name : "UNKNOWN_INSTANCED_MATERIAL");

				XMLWriter::Element instanceMaterialElement(writer, "instance_material");
				instanceMaterialElement.Attribute("symbol", name);
				instanceMaterialElement.Attribute("target", "#" + name);
			}
		}
	}

	void BindBoneMaterials(XMLWriter& writer, IExportContext* context, MaterialData& materialData, SkeletonData& skeletonData, int boneIndex, const std::map<int, int>& materialMaterialMap, const std::vector<MaterialEntry>& materials, IExportSource* source)
	{
		// Instance any materials that the node uses.
		GeometryMaterialData geometryMaterialData;
		source->ReadBoneGeometryMaterialData(context, &geometryMaterialData, &skeletonData, boneIndex, &materialData);

		std::set<int> usedMaterialIndices;
		for (int materialIndex = 0, materialCount = geometryMaterialData.GetUsedMaterialCount(); materialIndex < materialCount; ++materialIndex)
			usedMaterialIndices.insert(geometryMaterialData.GetUsedMaterialIndex(materialIndex));
		if (!usedMaterialIndices.empty())
		{
			XMLWriter::Element bindMaterialElement(writer, "bind_material");
			XMLWriter::Element techniqueCommonElement(writer, "technique_common");
			for (std::set<int>::const_iterator usedMtlPos = usedMaterialIndices.begin(), usedMtlEnd = usedMaterialIndices.end(); usedMtlPos != usedMtlEnd; ++usedMtlPos)
			{
				std::map<int, int>::const_iterator entryMapPos = materialMaterialMap.find(*usedMtlPos);
				int entryIndex = (entryMapPos != materialMaterialMap.end() ? (*entryMapPos).second : -1);
				std::string name = (entryIndex >= 0 ? materials[entryIndex].name : "UNKNOWN_INSTANCED_MATERIAL");

				XMLWriter::Element instanceMaterialElement(writer, "instance_material");
				instanceMaterialElement.Attribute("symbol", name);
				instanceMaterialElement.Attribute("target", "#" + name);
			}
		}

		//// Instance any materials that the node uses.
		//std::vector<int> materialIDs;
		//for (int materialIndex = 0, materialCount = materialData.GetMaterialCount(); materialIndex < materialCount; ++materialIndex)
		//{
		//	int parentIndex = materialData.GetParentIndex(materialIndex);
		//	if (parentIndex >= 0 && parentIndex == skeletonData.GetMaterial(boneIndex))
		//		materialIDs.push_back(materialIndex);
		//}

		//if (!materialIDs.empty())
		//{
		//	XMLWriter::Element bindMaterialElement(writer, "bind_material");
		//	XMLWriter::Element techniqueCommonElement(writer, "technique_common");
		//	for (int i = 0, count = int(materialIDs.size()); i < count; ++i)
		//	{
		//		std::map<int, int>::const_iterator entryMapPos = materialMaterialMap.find(materialIDs[i]);
		//		int entryIndex = (entryMapPos != materialMaterialMap.end() ? (*entryMapPos).second : -1);
		//		std::string name = (entryIndex >= 0 ? materials[entryIndex].name : 0);

		//		XMLWriter::Element instanceMaterialElement(writer, "instance_material");
		//		instanceMaterialElement.Attribute("symbol", name);
		//		instanceMaterialElement.Attribute("target", "#" + name);
		//	}
		//}
	}

	void WriteExtraData( XMLWriter& writer, const SHelperData& helperData, const std::string& properties )
	{
		// Write helper data (if it's helper node) and properties
		if ((!properties.empty()) || (helperData.m_eHelperType != SHelperData::eHelperType_UNKNOWN))
		{
			XMLWriter::Element elem(writer, "extra");
			{
				XMLWriter::Element elem(writer, "technique");
				elem.Attribute("profile","CryEngine");
				{
					if (!properties.empty())
					{
						// TODO: Sokov: check for invalid characters in properties string, like '<', '>', <' ', >127 etc.
						XMLWriter::Element elem(writer, "properties");
						elem.Content(properties);
					}

					if (helperData.m_eHelperType != SHelperData::eHelperType_UNKNOWN)
					{
						XMLWriter::Element elem(writer, "helper");
						switch (helperData.m_eHelperType)
						{
						case SHelperData::eHelperType_Point:
							elem.Attribute("type","point");
							break;
						case SHelperData::eHelperType_Dummy:
							elem.Attribute("type","dummy");
							{
								XMLWriter::Element elem(writer, "bound_box_min");
								elem.ContentArrayElement(helperData.m_boundBoxMin[0]);
								elem.ContentArrayElement(helperData.m_boundBoxMin[1]);
								elem.ContentArrayElement(helperData.m_boundBoxMin[2]);
							}
							{
								XMLWriter::Element elem(writer, "bound_box_max");
								elem.ContentArrayElement(helperData.m_boundBoxMax[0]);
								elem.ContentArrayElement(helperData.m_boundBoxMax[1]);
								elem.ContentArrayElement(helperData.m_boundBoxMax[2]);
							}
							break;
						default:
							assert(false);
							elem.Attribute("type","UNKNOWN");
							break;
						}
					}
				}
			}
		}

		// ****** I dont think we need this anymore. These files are not readable by XSI as far as I know. ******
		// ****** Removed 03-Jan-2012
		// Write special properties for importing to the XSI.
		/*{
			XMLWriter::Element elem(writer, "technique");
			elem.Attribute("profile","XSI");
			{
				XMLWriter::Element elem(writer, "XSI_CustomPSet");
				elem.Attribute("name","ObjectProperties");
				{
					XMLWriter::Element elem(writer, "propagation");
					elem.Content("NODE");
				}
				{
					XMLWriter::Element elem(writer, "type");
					elem.Content("CryNodeProperties");
				}
				{
					XMLWriter::Element elem(writer, "XSI_Parameter");
					elem.Attribute("id","Props");
					elem.Attribute("type","Text");
					elem.Attribute("value",properties.c_str());
				}
			}
		}*/
	}

	void WriteSkeletonRecurse(XMLWriter& writer, IExportContext* context, const std::string& modelName, SkeletonData& skeletonData, int boneIndex, const std::string& name, const std::vector<BoneEntry>& bones, std::map<std::pair<std::pair<int, int>, int>, int>& boneGeometryMap, std::vector<BoneGeometryEntry>& boneGeometries, int geometryFileIndex, int modelIndex, MaterialData& materialData, const std::map<int, int>& materialMaterialMap, const std::vector<MaterialEntry>& materials, IExportSource* source, ProgressRange& progressRange)
	{
		XMLWriter::Element nodeElement(writer, "node");
		nodeElement.Attribute("id", name); // The ID must be unique.
		nodeElement.Attribute("name", name); // The name must not include model name as prefix, so it can match the skeleton.

		// Calculate the transforms for the bone and its parent. This could be made a lot simpler by using proper
		// transforms in the skeleton data.
		Matrix34 transform;
		{
			Matrix44 transforms[2];
			int boneIndices[2] = {boneIndex, skeletonData.GetBoneParentIndex(boneIndex)};
			for (int i = 0; i < 2; ++i)
			{
				transforms[i] = IDENTITY;

				if (boneIndices[i] >= 0)
				{
					Vec3 scaleParams;
					skeletonData.GetScale((float*)&scaleParams, boneIndices[i]);
					Matrix44 scale = Matrix33::CreateScale(scaleParams);

					Ang3 rotationParams;
					skeletonData.GetRotation((float*)&rotationParams, boneIndices[i]);
					Matrix44 rotation = Matrix33::CreateRotationXYZ(rotationParams);

					Vec3 translationParams;
					skeletonData.GetTranslation((float*)&translationParams, boneIndices[i]);
					Matrix44 translation(IDENTITY);
					translation.SetTranslation(translationParams);

					transforms[i] = translation * (rotation * scale);
				}
			}
			transform = Matrix34(transforms[1].GetInverted() * transforms[0]);
		}

		Vec3 translation, scaling;
		CryQuat orientation;
		DecomposeTransform(translation, orientation, scaling, transform);
		Ang3 rotation = Ang3::GetAnglesXYZ(orientation);

		// Write translation element.
		{
			XMLWriter::Element translateElement(writer, "translate");
			translateElement.Attribute("sid", "translation");
			translateElement.ContentArrayElement(translation[0]);
			translateElement.ContentArrayElement(translation[1]);
			translateElement.ContentArrayElement(translation[2]);
		}

		// Write rotation elements.
		for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
		{
			XMLWriter::Element rotateElement(writer, "rotate");
			const std::string sidBuffer = StringHelpers::Format("rotation_%c", 'z' - axisIndex);
			rotateElement.Attribute("sid", sidBuffer.c_str());
			rotateElement.ContentArrayElement(axisIndex == 2 ? 1.0f : 0.0f);
			rotateElement.ContentArrayElement(axisIndex == 1 ? 1.0f : 0.0f);
			rotateElement.ContentArrayElement(axisIndex == 0 ? 1.0f : 0.0f);
			rotateElement.ContentArrayElement(rotation[2 - axisIndex] * 180.0f / 3.14159f);
		}

		// Write scale elements
		{
			XMLWriter::Element scaleElement(writer, "scale");
			scaleElement.Attribute("sid", "scale");
			scaleElement.ContentArrayElement(scaling[0]);
			scaleElement.ContentArrayElement(scaling[1]);
			scaleElement.ContentArrayElement(scaling[2]);
		}

		// If the node has geometry, write out the reference to it.
		std::map<std::pair<std::pair<int, int>, int>, int>::iterator boneGeometryMapPos = boneGeometryMap.find(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), boneIndex));
		if (boneGeometryMapPos != boneGeometryMap.end())
		{
			const std::string& boneGeometryName = boneGeometries[(*boneGeometryMapPos).second].name;
			XMLWriter::Element instanceGeometryElement(writer, "instance_geometry");
			instanceGeometryElement.Attribute("url", std::string("#") + boneGeometryName);

			BindBoneMaterials(writer, context, materialData, skeletonData, boneIndex, materialMaterialMap, materials, source);
		}

		SHelperData dummyHelperData;
		WriteExtraData( writer, dummyHelperData, skeletonData.GetBoneProperties(boneIndex) );

		int childIndexCount = skeletonData.GetChildCount(boneIndex);
		float progressRangeSlice = 1.0f / (childIndexCount > 0 ? float(childIndexCount) : 1.0f);
		for (int childIndexIndex = 0; childIndexIndex < childIndexCount; ++childIndexIndex)
		{
			int childIndex = skeletonData.GetChildIndex(boneIndex, childIndexIndex);
			WriteSkeletonRecurse(writer, context, modelName, skeletonData, childIndex, bones[childIndex].name, bones, boneGeometryMap, boneGeometries, geometryFileIndex, modelIndex, materialData, materialMaterialMap, materials, source, ProgressRange(progressRange, progressRangeSlice));
		}
	}

	void WritePhysSkeletonRecurse(XMLWriter& writer, const std::string& modelName, const SkeletonData& skeletonData, int boneIndex, const std::vector<BoneEntry>& bones, ProgressRange& progressRange, const Matrix34& physFrameTM, const Matrix34& parentTM)
	{
		Matrix34 currentPhysFrameTM = physFrameTM;

		// Output a node for the parent frame.
		bool shouldWriteParentFrame = skeletonData.HasParentFrame(boneIndex);
		XMLWriter::Element parentFrameElement(writer, "node", shouldWriteParentFrame);
		if (shouldWriteParentFrame)
		{
			parentFrameElement.Attribute("id", bones[boneIndex].parentFrameName); // The ID must be unique.
			parentFrameElement.Attribute("name", bones[boneIndex].parentFrameName); // The name must not include model name as prefix, so it can match the skeleton.

			// Write translation element.
			float translation[3];
			skeletonData.GetParentFrameTranslation(boneIndex, translation);
			{
				XMLWriter::Element translateElement(writer, "translate");
				translateElement.Attribute("sid", "translation");
				translateElement.ContentArrayElement(translation[0]);
				translateElement.ContentArrayElement(translation[1]);
				translateElement.ContentArrayElement(translation[2]);
			}

			// Write rotation elements.
			float rotation[3];
			skeletonData.GetParentFrameRotation(boneIndex, rotation);
			for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
			{
				XMLWriter::Element rotateElement(writer, "rotate");
				const std::string sidBuffer = StringHelpers::Format("rotation_%c", 'z' - axisIndex);
				rotateElement.Attribute("sid", sidBuffer.c_str());
				rotateElement.ContentArrayElement(axisIndex == 2 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(axisIndex == 1 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(axisIndex == 0 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(rotation[2 - axisIndex] * 180.0f / 3.14159f);
			}

			// Write scale elements
			float scaling[3];
			skeletonData.GetParentFrameScale(boneIndex, scaling);
			{
				XMLWriter::Element scaleElement(writer, "scale");
				scaleElement.Attribute("sid", "scale");
				scaleElement.ContentArrayElement(scaling[0]);
				scaleElement.ContentArrayElement(scaling[1]);
				scaleElement.ContentArrayElement(scaling[2]);
			}

			Matrix34 tm(IDENTITY);
			Matrix34 translationTM(IDENTITY);
			translationTM.SetTranslation(Vec3(translation[0], translation[1], translation[2]));
			Matrix34 rotationTM = Matrix33::CreateRotationXYZ(Ang3(rotation[0], rotation[1], rotation[2]));
			Matrix34 scaleTM = Matrix33::CreateScale(Vec3(scaling[0], scaling[1], scaling[2]));
			Matrix34 transform = translationTM * (rotationTM * scaleTM);

			currentPhysFrameTM = transform * currentPhysFrameTM;
		}

		Matrix34 worldTM;
		{
			float translation[3];
			skeletonData.GetTranslation(translation, boneIndex);
			float rotation[3];
			skeletonData.GetRotation(rotation, boneIndex);
			float scaling[3];
			skeletonData.GetScale(scaling, boneIndex);
			Matrix34 tm(IDENTITY);
			Matrix34 translationTM(IDENTITY);
			translationTM.SetTranslation(Vec3(translation[0], translation[1], translation[2]));
			Matrix34 rotationTM = Matrix33::CreateRotationXYZ(Ang3(rotation[0], rotation[1], rotation[2]));
			Matrix34 scaleTM = Matrix33::CreateScale(Vec3(scaling[0], scaling[1], scaling[2]));
			worldTM = translationTM * (rotationTM * scaleTM);
		}
		Matrix34 transform = parentTM.GetInverted() * worldTM;

		XMLWriter::Element nodeElement(writer, "node", skeletonData.GetPhysicalized(boneIndex));
		if (skeletonData.GetPhysicalized(boneIndex))
		{
			Matrix34 physTM = currentPhysFrameTM.GetInverted() * worldTM;
			Vec3 translation, scaling;
			CryQuat orientation;
			DecomposeTransform(translation, orientation, scaling, physTM);
			Ang3 rotation = Ang3::GetAnglesXYZ(orientation);

			nodeElement.Attribute("id", bones[boneIndex].physName); // The ID must be unique.
			nodeElement.Attribute("name", bones[boneIndex].physName); // The name must not include model name as prefix, so it can match the skeleton.

			// Write translation element.
			{
				XMLWriter::Element translateElement(writer, "translate");
				translateElement.Attribute("sid", "translation");
				translateElement.ContentArrayElement(translation[0]);
				translateElement.ContentArrayElement(translation[1]);
				translateElement.ContentArrayElement(translation[2]);
			}

			// Write rotation elements.
			for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
			{
				XMLWriter::Element rotateElement(writer, "rotate");
				const std::string sidBuffer = StringHelpers::Format("rotation_%c", 'z' - axisIndex);
				rotateElement.Attribute("sid", sidBuffer.c_str());
				rotateElement.ContentArrayElement(axisIndex == 2 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(axisIndex == 1 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(axisIndex == 0 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(rotation[2 - axisIndex] * 180.0f / 3.14159f);
			}

			// Write scale elements
			{
				XMLWriter::Element scaleElement(writer, "scale");
				scaleElement.Attribute("sid", "scale");
				scaleElement.ContentArrayElement(scaling[0]);
				scaleElement.ContentArrayElement(scaling[1]);
				scaleElement.ContentArrayElement(scaling[2]);
			}

			currentPhysFrameTM = worldTM;
		}

		SHelperData dummyHelperData;
		WriteExtraData( writer, dummyHelperData, skeletonData.GetBoneGeomProperties(boneIndex) );

		int childIndexCount = skeletonData.GetChildCount(boneIndex);
		float progressRangeSlice = 1.0f / (childIndexCount > 0 ? float(childIndexCount) : 1.0f);
		for (int childIndexIndex = 0; childIndexIndex < childIndexCount; ++childIndexIndex)
		{
			int childIndex = skeletonData.GetChildIndex(boneIndex, childIndexIndex);
			WritePhysSkeletonRecurse(writer, modelName, skeletonData, childIndex, bones, ProgressRange(progressRange, progressRangeSlice), currentPhysFrameTM, worldTM);
		}
	}

	void WriteGeometryData(XMLWriter& writer, const std::string& id, const std::string& name, GeometryData& geometryData, MaterialData& materialData, std::map<int, int>& materialMaterialMap, std::vector<MaterialEntry>& materials)
	{
		XMLWriter::Element geometryElement(writer, "geometry");
		geometryElement.Attribute("id", id);
		if (!name.empty())
			geometryElement.Attribute("name", name);
		XMLWriter::Element meshElement(writer, "mesh");

		// Write out the positions.
		std::string posSourceName = id + "-pos";
		{
			XMLWriter::Element sourceElement(writer, "source");
			sourceElement.Attribute("id", posSourceName);

			std::string arrayName = posSourceName + "-array";
			{
				XMLWriter::Element arrayElement(writer, "float_array");
				arrayElement.Attribute("id", arrayName);
				arrayElement.Attribute("count", int(geometryData.positions.size()) * 3);
				for (int positionIndex = 0, positionCount = int(geometryData.positions.size()); positionIndex < positionCount; ++positionIndex)
				{
					arrayElement.ContentArrayElement(geometryData.positions[positionIndex].x);
					arrayElement.ContentArrayElement(geometryData.positions[positionIndex].y);
					arrayElement.ContentArrayElement(geometryData.positions[positionIndex].z);
				}
			}

			XMLWriter::Element techniqueCommonElement(writer, "technique_common");
			XMLWriter::Element accessorElement(writer, "accessor");
			accessorElement.Attribute("source", std::string("#") + arrayName);
			accessorElement.Attribute("count", int(geometryData.positions.size()));
			accessorElement.Attribute("stride", 3);
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "X");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "Y");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "Z");
				paramElement.Attribute("type", "float");
			}
		}

		// Write out the normals.
		std::string normalSourceName = id + "-normal";
		{
			XMLWriter::Element sourceElement(writer, "source");
			sourceElement.Attribute("id", normalSourceName);

			std::string arrayName = normalSourceName + "-array";
			{
				XMLWriter::Element arrayElement(writer, "float_array");
				arrayElement.Attribute("id", arrayName);
				arrayElement.Attribute("count", int(geometryData.normals.size()) * 3);
				for (int normalIndex = 0, normalCount = int(geometryData.normals.size()); normalIndex < normalCount; ++normalIndex)
				{
					arrayElement.ContentArrayElement(geometryData.normals[normalIndex].x);
					arrayElement.ContentArrayElement(geometryData.normals[normalIndex].y);
					arrayElement.ContentArrayElement(geometryData.normals[normalIndex].z);
				}
			}

			XMLWriter::Element techniqueCommonElement(writer, "technique_common");
			XMLWriter::Element accessorElement(writer, "accessor");
			accessorElement.Attribute("source", std::string("#") + arrayName);
			accessorElement.Attribute("count", int(geometryData.normals.size()));
			accessorElement.Attribute("stride", 3);
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "X");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "Y");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "Z");
				paramElement.Attribute("type", "float");
			}
		}

		// Write out the texture coordinates.
		std::string textureCoordinateSourceName = id + "-uvs";
		if (!geometryData.textureCoordinates.empty())
		{
			XMLWriter::Element sourceElement(writer, "source");
			sourceElement.Attribute("id", textureCoordinateSourceName);

			std::string arrayName = textureCoordinateSourceName + "-array";
			{
				XMLWriter::Element arrayElement(writer, "float_array");
				arrayElement.Attribute("id", arrayName);
				arrayElement.Attribute("count", int(geometryData.textureCoordinates.size()) * 2);
				for (int textureCoordinateIndex = 0, textureCoordinateCount = int(geometryData.textureCoordinates.size()); textureCoordinateIndex < textureCoordinateCount; ++textureCoordinateIndex)
				{
					arrayElement.ContentArrayElement(geometryData.textureCoordinates[textureCoordinateIndex].u);
					arrayElement.ContentArrayElement(geometryData.textureCoordinates[textureCoordinateIndex].v);
				}
			}

			XMLWriter::Element techniqueCommonElement(writer, "technique_common");
			XMLWriter::Element accessorElement(writer, "accessor");
			accessorElement.Attribute("source", std::string("#") + arrayName);
			accessorElement.Attribute("count", int(geometryData.textureCoordinates.size()));
			accessorElement.Attribute("stride", 2);
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "S");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "T");
				paramElement.Attribute("type", "float");
			}
		}

		// Write out the vertex colors.
		std::string vertexColorSourceName = id + "-vcol";
		if (!geometryData.vertexColors.empty())
		{
			XMLWriter::Element sourceElement(writer, "source");
			sourceElement.Attribute("id", vertexColorSourceName);

			std::string arrayName = vertexColorSourceName + "-array";
			{
				XMLWriter::Element arrayElement(writer, "float_array");
				arrayElement.Attribute("id", arrayName);
				arrayElement.Attribute("count", int(geometryData.vertexColors.size()) * 4);
				for (int vertexColorIndex = 0, vertexColorCount = int(geometryData.vertexColors.size()); vertexColorIndex < vertexColorCount; ++vertexColorIndex)
				{
					arrayElement.ContentArrayElement(geometryData.vertexColors[vertexColorIndex].r);
					arrayElement.ContentArrayElement(geometryData.vertexColors[vertexColorIndex].g);
					arrayElement.ContentArrayElement(geometryData.vertexColors[vertexColorIndex].b);
					arrayElement.ContentArrayElement(geometryData.vertexColors[vertexColorIndex].a);
				}
			}

			XMLWriter::Element techniqueCommonElement(writer, "technique_common");
			XMLWriter::Element accessorElement(writer, "accessor");
			accessorElement.Attribute("source", std::string("#") + arrayName);
			accessorElement.Attribute("count", int(geometryData.vertexColors.size()));
			accessorElement.Attribute("stride", 4);
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "R");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "G");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "B");
				paramElement.Attribute("type", "float");
			}
			{
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "A");
				paramElement.Attribute("type", "float");
			}
		}

		// Write out the vertex elements.
		std::string vertexName = id + "-vtx";
		{
			XMLWriter::Element vertexElement(writer, "vertices");
			vertexElement.Attribute("id", vertexName);
			XMLWriter::Element inputElement(writer, "input");
			inputElement.Attribute("semantic", "POSITION");
			inputElement.Attribute("source", std::string("#") + posSourceName);
		}

		// Sort the triangles by material.
		std::vector<std::vector<GeometryData::Polygon> > polygonsByMaterial(materialData.GetMaterialCount() + 1);
		for (int polygonIndex = 0, polygonCount = int(geometryData.polygons.size()); polygonIndex < polygonCount; ++polygonIndex)
			polygonsByMaterial[geometryData.polygons[polygonIndex].mtlID + 1].push_back(geometryData.polygons[polygonIndex]);

		// Write out the triangles.
		for (int materialIndex = -1, materialCount = materialData.GetMaterialCount(); materialIndex < materialCount; ++materialIndex)
		{
			if (!polygonsByMaterial[materialIndex + 1].empty())
			{
				std::map<int, int>::iterator materialMapPos = materialMaterialMap.find(materialIndex);
				int materialEntryIndex = (materialMapPos != materialMaterialMap.end() ? (*materialMapPos).second : -1);

				std::vector<GeometryData::Polygon>& polygons = polygonsByMaterial[materialIndex + 1];

				XMLWriter::Element trianglesElement(writer, "triangles");
				trianglesElement.Attribute("count", int(polygons.size()));
				if (materialEntryIndex >= 0)
					trianglesElement.Attribute("material", materials[materialEntryIndex].name);
				int offset = 0;
				bool hasPositions = false;
				if (!geometryData.positions.empty())
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "VERTEX");
					inputElement.Attribute("source", std::string("#") + vertexName);
					inputElement.Attribute("offset", offset++);
					hasPositions = true;
				}
				bool hasNormals = false;
				if (!geometryData.normals.empty())
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "NORMAL");
					inputElement.Attribute("source", std::string("#") + normalSourceName);
					inputElement.Attribute("offset", offset++);
					hasNormals = true;
				}
				bool hasUVs = false;
				if (!geometryData.textureCoordinates.empty())
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "TEXCOORD");
					inputElement.Attribute("source", std::string("#") + textureCoordinateSourceName);
					inputElement.Attribute("offset", offset++);
					hasUVs = true;
				}
				bool hasColors = false;
				if (!geometryData.vertexColors.empty())
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "COLOR");
					inputElement.Attribute("source", std::string("#") + vertexColorSourceName);
					inputElement.Attribute("offset", offset++);
					hasColors = true;
				}

				XMLWriter::Element pElement(writer, "p");
				for (int polygonIndex = 0, polygonCount = int(polygons.size()); polygonIndex < polygonCount; ++polygonIndex)
				{
					for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
					{
						if (hasPositions && polygons[polygonIndex].v[vertexIndex].positionIndex >= 0)
							pElement.ContentArrayElement(polygons[polygonIndex].v[vertexIndex].positionIndex);
						if (hasNormals && polygons[polygonIndex].v[vertexIndex].normalIndex >= 0)
							pElement.ContentArrayElement(polygons[polygonIndex].v[vertexIndex].normalIndex);
						if (hasUVs && polygons[polygonIndex].v[vertexIndex].textureCoordinateIndex >= 0)
							pElement.ContentArrayElement(polygons[polygonIndex].v[vertexIndex].textureCoordinateIndex);
						if (hasColors && polygons[polygonIndex].v[vertexIndex].vertexColorIndex >= 0)
							pElement.ContentArrayElement(polygons[polygonIndex].v[vertexIndex].vertexColorIndex);
					}
				}
			}
		}
	}

	bool WriteGeometries(IExportContext* context, XMLWriter& writer, std::vector<GeometryEntry>& geometries, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData, MorphDataMap& morphData, MaterialData& materialData, std::vector<MaterialEntry>& materials, std::map<int, int>& materialMaterialMap, SkeletonDataMap& skeletonData, std::vector<BoneGeometryEntry>& boneGeometries, std::map<std::pair<std::pair<int, int>, int>, int>& boneGeometryMap, std::map<std::pair<std::pair<int, int>, int>, int>& morphGeometryMap, std::vector<MorphGeometryEntry>& morphGeometries, IExportSource* source, ProgressRange& progressRange)
	{
		XMLWriter::Element libraryGeometriesElement(writer, "library_geometries");

		// Loop through all the geometries.
		for (int geometryIndex = 0, geometryCount = int(geometries.size()); geometryIndex < geometryCount; ++geometryIndex)
		{
			GeometryEntry& geometryEntry = geometries[geometryIndex];

			// Read in the geometry data.
			GeometryData geometryData;
			bool ok = source->ReadGeometry(context, &geometryData, &modelData[geometryEntry.geometryFileIndex], &materialData, geometryEntry.modelIndex);
			if (!ok)
			{
				return false;
			}
			
			WriteGeometryData(writer, geometryEntry.name, "", geometryData, materialData, materialMaterialMap, materials);
		}

		// Loop through all the bone geometries.
		for (int boneGeometryIndex = 0, boneGeometryCount = int(boneGeometries.size()); boneGeometryIndex < boneGeometryCount; ++boneGeometryIndex)
		{
			BoneGeometryEntry& boneGeometryEntry = boneGeometries[boneGeometryIndex];

			GeometryData geometryData;
			SkeletonDataMap::iterator skeletonDataPos = skeletonData.find(std::make_pair(boneGeometryEntry.geometryFileIndex, boneGeometryEntry.modelIndex));
			if (skeletonDataPos != skeletonData.end())
				source->ReadBoneGeometry(context, &geometryData, &(*skeletonDataPos).second, boneGeometryEntry.boneIndex, &materialData);

			WriteGeometryData(writer, boneGeometryEntry.name, "", geometryData, materialData, materialMaterialMap, materials);
		}

		// Loop through all the morph geometries.
		for (int morphGeometryIndex = 0, morphGeometryCount = int(morphGeometries.size()); morphGeometryIndex < morphGeometryCount; ++morphGeometryIndex)
		{
			MorphGeometryEntry& morphGeometryEntry = morphGeometries[morphGeometryIndex];

			GeometryData geometryData;
			MorphDataMap::iterator morphDataPos = morphData.find(std::make_pair(morphGeometryEntry.geometryFileIndex, morphGeometryEntry.modelIndex));
			if (morphDataPos != morphData.end())
				source->ReadMorphGeometry(context, &geometryData, &modelData[morphGeometryEntry.geometryFileIndex], morphGeometryEntry.modelIndex, &(*morphDataPos).second, morphGeometryEntry.morphIndex, &materialData);

			WriteGeometryData(writer, morphGeometryEntry.name, morphGeometryEntry.morphName, geometryData, materialData, materialMaterialMap, materials);
		}

		return true;
	}

	void WriteExportNodeProperties(const IExportSource& source,  XMLWriter& writer, const char *geomFilename,const IGeometryFileData::SProperties& properties )
	{
		XMLWriter::Element elem(writer, "extra");
		{
			XMLWriter::Element elem(writer, "technique");
			elem.Attribute("profile","CryEngine");
			{
				std::string const filetypeStr = ExportFileTypeHelpers::CryFileTypeToString(properties.filetypeInt);
				std::string props = std::string("fileType=") + filetypeStr;
				if (properties.bDoNotMerge)
				{
					props += std::string("\r\n") + "DoNotMerge";
				}
				if (properties.bUseCustomNormals)
				{
					props += std::string("\r\n") + "UseCustomNormals";
				}
				if (properties.filetypeInt == CRY_FILE_TYPE_SKIN && properties.b8WeightsPerVertex)
				{
					props += std::string("\r\n") + "EightWeightsPerVertex";
				}
				if (properties.filetypeInt == CRY_FILE_TYPE_SKIN && properties.bVClothPreProcess)
				{
					props += std::string("\r\n") + "VClothPreProcess";
				}
				if (properties.bUseF32VertexFormat)
				{
					props += std::string("\r\n") + "UseF32VertexFormat";
				}

				// properties.customExportPath is an absolute path, constructed from the path to the Maya scene and a relative path in the export node.
				// We translate this to a relative path to the .dae file to be easier to debug it in the future
				if (!properties.customExportPath.empty())
				{
					const string customExportPath = StringHelpers::ConvertAnsiToAscii(PathHelpers::GetShortestRelativePath(source.GetExportDirectory(), properties.customExportPath).c_str(), '_');
					props += std::string("\r\n") + std::string("CustomExportPath=") + customExportPath;
				}

				XMLWriter::Element elem(writer, "properties");
				elem.Content(props);
			}
		}

		// Write special properties for importing to the XSI.
		{
			XMLWriter::Element elem(writer, "technique");
			elem.Attribute("profile","XSI");
			{
				XMLWriter::Element elem(writer, "XSI_CustomPSet");
				elem.Attribute("name","ExportProperties");
				{
					XMLWriter::Element elem(writer, "propagation");
					elem.Content("NODE");
				}
				{
					XMLWriter::Element elem(writer, "type");
					elem.Content("CryExportNodeProperties");
				}
				{
					XMLWriter::Element elem(writer, "XSI_Parameter");
					elem.Attribute("id","Filetype");
					elem.Attribute("type","Integer");
					elem.Attribute("value",properties.filetypeInt);
				}
				{
					XMLWriter::Element elem(writer, "XSI_Parameter");
					elem.Attribute("id","Filename");
					elem.Attribute("type","Text");
					elem.Attribute("value",geomFilename);
				}
				{
					XMLWriter::Element elem(writer, "XSI_Parameter");
					elem.Attribute("id","Exportable");
					elem.Attribute("type","Boolean");
					elem.Attribute("value","1");
				}
				{
					XMLWriter::Element elem(writer, "XSI_Parameter");
					elem.Attribute("id","MergeObjects");
					elem.Attribute("type","Boolean");
					elem.Attribute("value",(!properties.bDoNotMerge) );
				}
			}
		}
	}


	void WriteHierarchyRecurse(
		XMLWriter& writer, 
		IExportContext* context, 
		int geometryFileIndex, 
		MaterialData& materialData, 
		const std::map<int, int>& materialMaterialMap, 
		const std::vector<MaterialEntry>& materials, 
		const ModelData& modelData, 
		int const modelIndex, 
		std::map<std::pair<int, int>, int>& modelGeometryMap, 
		std::vector<GeometryEntry>& geometries, 
		std::map<std::pair<int, int>, int>& modelControllerMap, 
		std::vector<SkinControllerEntry>& controllers, 
		std::map<std::pair<int, int>, int>& modelMorphControllerMap, 
		std::vector<MorphControllerEntry>& morphControllers, 
		IExportSource* source, 
		ProgressRange& progressRange)
	{
		XMLWriter::Element nodeElement(writer, "node");
		nodeElement.Attribute("id", modelData.GetModelName(modelIndex)); // The ID must be unique.

		{
			float translation[3];
			float rotation[3];
			float scaling[3];
			modelData.GetTranslationRotationScale(modelIndex, translation, rotation, scaling);

			// Write translation element.
			{
				XMLWriter::Element translateElement(writer, "translate");
				translateElement.Attribute("sid", "translation");
				translateElement.ContentArrayElement(translation[0]);
				translateElement.ContentArrayElement(translation[1]);
				translateElement.ContentArrayElement(translation[2]);
			}

			// Write rotation elements.
			for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
			{
				XMLWriter::Element rotateElement(writer, "rotate");
				const std::string sidBuffer = StringHelpers::Format("rotation_%c", 'z' - axisIndex);
				rotateElement.Attribute("sid", sidBuffer.c_str());
				rotateElement.ContentArrayElement(axisIndex == 2 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(axisIndex == 1 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(axisIndex == 0 ? 1.0f : 0.0f);
				rotateElement.ContentArrayElement(rotation[2 - axisIndex] * 180.0f / 3.14159f);
			}

			// Write scale elements
			{
				XMLWriter::Element scaleElement(writer, "scale");
				scaleElement.Attribute("sid", "scale");
				scaleElement.ContentArrayElement(scaling[0]);
				scaleElement.ContentArrayElement(scaling[1]);
				scaleElement.ContentArrayElement(scaling[2]);
			}
		}

		// If the node has a controller, write out the reference to it.
		std::map<std::pair<int, int>, int>::iterator modelControllerMapPos = modelControllerMap.find(std::make_pair(geometryFileIndex, modelIndex));
		std::map<std::pair<int, int>, int>::iterator modelMorphControllerMapPos = modelMorphControllerMap.find(std::make_pair(geometryFileIndex, modelIndex));
		if (modelControllerMapPos != modelControllerMap.end())
		{
			const std::string& controllerName = controllers[(*modelControllerMapPos).second].name;
			XMLWriter::Element instanceControllerElement(writer, "instance_controller");
			instanceControllerElement.Attribute("url", "#" + controllerName);

			BindMaterials(writer, context, materialData, modelData, modelIndex, materialMaterialMap, materials, source);
		}
		else if (modelMorphControllerMapPos != modelMorphControllerMap.end())
		{
			const std::string& controllerName = morphControllers[(*modelMorphControllerMapPos).second].name;
			XMLWriter::Element instanceControllerElement(writer, "instance_controller");
			instanceControllerElement.Attribute("url", "#" + controllerName);

			BindMaterials(writer, context, materialData, modelData, modelIndex, materialMaterialMap, materials, source);
		}
		else
		{
			// If the node has geometry, write out the reference to it.
			std::map<std::pair<int, int>, int>::iterator modelGeometryMapPos = modelGeometryMap.find(std::make_pair(geometryFileIndex, modelIndex));
			if (modelGeometryMapPos != modelGeometryMap.end())
			{
				const std::string& geometryName = geometries[(*modelGeometryMapPos).second].name;
				XMLWriter::Element instanceGeometryElement(writer, "instance_geometry");
				instanceGeometryElement.Attribute("url", std::string("#") + geometryName);

				BindMaterials(writer, context, materialData, modelData, modelIndex, materialMaterialMap, materials, source);
			}
		}

		// Recurse to the child nodes.
		int childIndexCount = modelData.GetChildCount(modelIndex);
		float progressRangeSlice = 1.0f / (childIndexCount > 0 ? float(childIndexCount) : 1.0f);
		for (int childIndexIndex = 0; childIndexIndex < childIndexCount; ++childIndexIndex)
		{
			int childIndex = modelData.GetChildIndex(modelIndex, childIndexIndex);
			WriteHierarchyRecurse(writer, context, geometryFileIndex, materialData, materialMaterialMap, materials, modelData, childIndex, modelGeometryMap, geometries, modelControllerMap, controllers, modelMorphControllerMap, morphControllers, source, ProgressRange(progressRange, progressRangeSlice));
		}

		// Write properties, HelperData
		WriteExtraData(writer, modelData.GetHelperData(modelIndex), modelData.GetProperties(modelIndex));
	}

	void WriteHierarchy(
		XMLWriter& writer, 
		IExportContext* context, 
		const GeometryFileData& geometryFileData, 
		MaterialData& materialData, 
		const std::map<int, int>& materialMaterialMap, 
		const std::vector<MaterialEntry>& materials, 
		const std::vector<ModelData>& modelData, 
		SkeletonDataMap& skeletonData, 
		std::map<std::pair<int, int>, int>& modelGeometryMap, 
		std::vector<GeometryEntry>& geometries, 
		std::map<std::pair<int, int>, int>& modelControllerMap, 
		std::vector<SkinControllerEntry>& controllers, 
		BoneDataMap& boneDataMap, 
		std::map<std::pair<std::pair<int, int>, int>, int>& boneGeometryMap, 
		std::vector<BoneGeometryEntry>& boneGeometries, 
		std::map<std::pair<int, int>, int>& modelMorphControllerMap, 
		std::vector<MorphControllerEntry>& morphControllers, 
		IExportSource* source, 
		ProgressRange& progressRange)
	{
		XMLWriter::Element libraryVisualScenesElement(writer, "library_visual_scenes");
		XMLWriter::Element visualSceneElement(writer, "visual_scene");
		visualSceneElement.Attribute("id", "visual_scene_0");
		visualSceneElement.Attribute("name", "untitled");

		int geometryFileCount = geometryFileData.GetGeometryFileCount();
		float geometryFileRangeSlice = 1.0f / (geometryFileCount > 0 ? float(geometryFileCount) : 1.0f);
		for (int geometryFileIndex = 0; geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			ProgressRange geometryFileRange(progressRange, geometryFileRangeSlice);

			// Make sure to write out a CryExportNode - this is expected by the RC.
			std::string nodeName;

			nodeName = std::string("CryExportNode_") + geometryFileData.GetGeometryFileName(geometryFileIndex);
			XMLWriter::Element nodeElement(writer, "node");
			nodeElement.Attribute("id", nodeName);

			// Write translation element.
			{
				XMLWriter::Element translateElement(writer, "translate");
				translateElement.Attribute("sid", "translation");
				translateElement.Content("0 0 0");
			}

			// Write rotation elements.
			{
				XMLWriter::Element rotateElement(writer, "rotate");
				rotateElement.Attribute("sid", "rotation_z");
				rotateElement.Content("0 0 1 0");
			}

			{
				XMLWriter::Element rotateElement(writer, "rotate");
				rotateElement.Attribute("sid", "rotation_y");
				rotateElement.Content("0 1 0 0");
			}

			{
				XMLWriter::Element rotateElement(writer, "rotate");
				rotateElement.Attribute("sid", "rotation_x");
				rotateElement.Content("1 0 0 0");
			}

			// Write scale elements
			{
				XMLWriter::Element scaleElement(writer, "scale");
				scaleElement.Attribute("sid", "scale");
				scaleElement.Content("1 1 1");
			}

			{
				ProgressRange modelProgressRange(geometryFileRange, 0.5f);
				int rootIndexCount = modelData[geometryFileIndex].GetRootCount();
				float progressRangeSlice = 1.0f / (rootIndexCount > 0 ? float(rootIndexCount) : 1.0f);
				for (int rootIndexIndex = 0; rootIndexIndex < rootIndexCount; ++rootIndexIndex)
				{
					int rootModelIndex = modelData[geometryFileIndex].GetRootIndex(rootIndexIndex);
					WriteHierarchyRecurse(writer, context, geometryFileIndex, materialData, materialMaterialMap, materials, modelData[geometryFileIndex], rootModelIndex, modelGeometryMap, geometries, modelControllerMap, controllers, modelMorphControllerMap, morphControllers, source, ProgressRange(modelProgressRange, progressRangeSlice));
				}
			}

			{
				ProgressRange skeletonProgressRange(geometryFileRange, 0.5f);
				//write the skeleton for the first model in the geometry file only
				//for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
				if ( modelData[geometryFileIndex].GetModelCount() > 0 )
				{
					int modelIndex = 0;
					int modelCount = 1;
					SkeletonDataMap::iterator skeletonDataPos = skeletonData.find(std::make_pair(geometryFileIndex, modelIndex));
					if (skeletonDataPos != skeletonData.end())
					{
						SkeletonData& skeletonDataInstance = (*skeletonDataPos).second;
						const std::vector<BoneEntry>& bones = (*boneDataMap.find(std::make_pair(geometryFileIndex, modelIndex))).second;
						int rootIndexCount = skeletonDataInstance.GetRootCount();
						float progressRangeSlice = 1.0f / ((rootIndexCount > 0 ? float(rootIndexCount) : 1.0f) * modelCount);
						for (int rootIndexIndex = 0; rootIndexIndex < rootIndexCount; ++rootIndexIndex)
						{
							int rootBoneIndex = skeletonDataInstance.GetRootIndex(rootIndexIndex);
							WriteSkeletonRecurse(writer, context, geometryFileData.GetGeometryFileName(geometryFileIndex), skeletonDataInstance, rootBoneIndex, bones[rootBoneIndex].name, bones, boneGeometryMap, boneGeometries, geometryFileIndex, modelIndex, materialData, materialMaterialMap, materials, source, ProgressRange(skeletonProgressRange, progressRangeSlice * 0.5f));
							WritePhysSkeletonRecurse(writer, geometryFileData.GetGeometryFileName(geometryFileIndex), skeletonDataInstance, rootBoneIndex, bones, ProgressRange(skeletonProgressRange, progressRangeSlice * 0.5f), Matrix34(IDENTITY), Matrix34(IDENTITY));
						}
					}
				}
			}

			// Write properties if they exist
			WriteExportNodeProperties(*source, writer, geometryFileData.GetGeometryFileName(geometryFileIndex),geometryFileData.GetProperties(geometryFileIndex));
		}
	}

	void WriteMetaData( IExportSource *pExportSource,XMLWriter& writer, ProgressRange& progressRange )
	{
		SExportMetaData metaData;
		pExportSource->GetMetaData(metaData);

		XMLWriter::Element assetElement(writer, "asset");
		{
			XMLWriter::Element contributorElement(writer, "contributor");
			contributorElement.Child("author", metaData.author );
			contributorElement.Child("authoring_tool", metaData.authoring_tool );
			contributorElement.Child("source_data", metaData.source_data );
		}

		std::time_t time = std::time(0);
		std::tm dateTime = *std::localtime(&time);
		char timeBuffer[1024];
		std::strftime(timeBuffer, sizeof(timeBuffer) / sizeof(timeBuffer[0]), "%Y-%m-%dT%H:%M:%SZ", &dateTime);
		assetElement.Child("created", timeBuffer);
		assetElement.Child("modified", timeBuffer);
		assetElement.Child("revision", metaData.revision);
		{
			XMLWriter::Element unitElement(writer, "unit");
			unitElement.Attribute("meter", metaData.fMeterUnit);
			unitElement.Attribute("name", "meter");
		}

		switch (metaData.up_axis)
		{
		case SExportMetaData::X_UP:
			assetElement.Child("up_axis", "X_UP");
			break;
		case SExportMetaData::Y_UP:
			assetElement.Child("up_axis", "Y_UP");
			break;
		case SExportMetaData::Z_UP:
		default:
			assetElement.Child("up_axis", "Z_UP");
			break;
		}
	}

	enum AnimationBoneParameter
	{
		AnimationBoneParameter_TransX,
		AnimationBoneParameter_TransY,
		AnimationBoneParameter_TransZ,
		AnimationBoneParameter_RotX,
		AnimationBoneParameter_RotY,
		AnimationBoneParameter_RotZ,
		AnimationBoneParameter_SclX,
		AnimationBoneParameter_SclY,
		AnimationBoneParameter_SclZ,
	};
	const char* parameterStrings[] = {
		"posx",
		"posy",
		"posz",
		"rotx",
		"roty",
		"rotz",
		"sclx",
		"scly",
		"sclz"
	};
	const char* parameterTargetStrings[] = {
		"translation.X",
		"translation.Y",
		"translation.Z",
		"rotation_x.ANGLE",
		"rotation_y.ANGLE",
		"rotation_z.ANGLE",
		"scale.X",
		"scale.Y",
		"scale.Z"
	};
	struct AnimationBoneParameterEntry
	{
		std::string name;
		int boneIndex;
		AnimationBoneParameter parameter;
	};
	struct AnimationEntry
	{
		std::string name;
		int geometryFileIndex;
		int modelIndex;
		int animationIndex;
		float start;
		float stop;
		std::vector<AnimationBoneParameterEntry> parameters;
	};
	void AddParametersRecursive(AnimationEntry& animation, AnimationData& animationData, const SkeletonData& skeletonData, int boneIndex, ProgressRange& progressRange)
	{
		for (AnimationBoneParameter parameter = AnimationBoneParameter_TransX; parameter <= AnimationBoneParameter_SclZ; parameter = AnimationBoneParameter(parameter + 1))
		{
			animation.parameters.push_back(AnimationBoneParameterEntry());
			AnimationBoneParameterEntry& parameterEntry = animation.parameters.back();
			parameterEntry.boneIndex = boneIndex;
			parameterEntry.parameter = parameter;
			parameterEntry.name = animation.name + "-" + skeletonData.GetSafeName(boneIndex) + "_" + parameterStrings[parameter] + "-anim";

			// Encode the flags as naming conventions.
			unsigned modelFlags = animationData.GetModelFlags(boneIndex);
			if (modelFlags & IAnimationData::ModelFlags_NoExport)
				parameterEntry.name += "-NoExport";
		}

		int childIndexCount = skeletonData.GetChildCount(boneIndex);
		float progressRangeSlice = 1.0f / (childIndexCount > 0 ? float(childIndexCount) : 1.0f);
		for (int childIndexIndex = 0; childIndexIndex < childIndexCount; ++childIndexIndex)
			AddParametersRecursive(animation, animationData, skeletonData, skeletonData.GetChildIndex(boneIndex, childIndexIndex), ProgressRange(progressRange, progressRangeSlice));
	}

	void AddParametersForNoSkeleton(AnimationEntry& animation, int modelIndex, const std::string& modelName, AnimationBoneParameter whichParameter)
	{
		for (AnimationBoneParameter parameter = whichParameter; parameter < whichParameter + 3; parameter = AnimationBoneParameter(parameter + 1))
		{
			animation.parameters.push_back(AnimationBoneParameterEntry());
			AnimationBoneParameterEntry& parameterEntry = animation.parameters.back();
			parameterEntry.boneIndex = modelIndex;
			parameterEntry.parameter = parameter;
			parameterEntry.name = animation.name + "-" + modelName + "_" + parameterStrings[parameter] + "-anim";
		}
	}

	void AddAnimationEntry( std::vector<AnimationEntry> &animations, int animationIndex, int geometryFileIndex, int modelIndex, IExportSource* source, GeometryFileData &geometryFileData )
	{
		animations.push_back(AnimationEntry());
		AnimationEntry& animation = animations.back();
		animation.animationIndex = animationIndex;
		animation.geometryFileIndex = geometryFileIndex;
		animation.modelIndex = modelIndex;

		std::string safeAnimationName = source->GetAnimationName(&geometryFileData, geometryFileIndex, animationIndex);
		std::replace(safeAnimationName.begin(), safeAnimationName.end(), ' ', '_');
		animation.name = safeAnimationName + std::string("-") + geometryFileData.GetGeometryFileName(geometryFileIndex);
		source->GetAnimationTimeSpan(animation.start, animation.stop, animationIndex);
	}

	void GenerateAnimationList(IExportContext* context, std::vector<AnimationEntry>& animations, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData, SkeletonDataMap& skeletonData, IExportSource* source, ProgressRange& progressRange)
	{
		int geometryFileCount = geometryFileData.GetGeometryFileCount();
		float geometryFileProgressRangeSlice = 1.0f / (geometryFileCount > 0 ? float(geometryFileCount) : 1.0f);
		for (int geometryFileIndex = 0; geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			ProgressRange geometryFileProgressRange(progressRange, geometryFileProgressRangeSlice);

			int modelCount = modelData[geometryFileIndex].GetModelCount();
			float modelProgressRangeSlice = 1.0f / (modelCount > 0 ? float(modelCount) : 1.0f);

			int animationCount = source->GetAnimationCount();
			float animProgressRangeSlice = 1.0f / (animationCount > 0 ? float(animationCount) : 1.0f);
			for (int animationIndex = 0; animationIndex < animationCount; ++animationIndex)
			{
				ProgressRange animationProgressRange(geometryFileProgressRange, animProgressRangeSlice);

				if(skeletonData.empty()) // Non-skeletal mesh
					AddAnimationEntry(animations, animationIndex, geometryFileIndex, -1, source, geometryFileData);
				else                     // Skeletal mesh
					AddAnimationEntry(animations, animationIndex, geometryFileIndex, 0, source, geometryFileData);
				AnimationEntry& animation = animations.back();
				for (int modelIndex = 0; modelIndex < modelCount; ++modelIndex)
				{
					ProgressRange modelProgressRange(animationProgressRange, modelProgressRangeSlice);

					if(skeletonData.empty())				// Non-skeletal mesh
					{
						const bool hasPos = source->HasValidPosController(&modelData[geometryFileIndex], modelIndex);
						const bool hasRot = source->HasValidRotController(&modelData[geometryFileIndex], modelIndex);
						const bool hasScl = source->HasValidSclController(&modelData[geometryFileIndex], modelIndex);
						if (hasPos || hasRot || hasScl)
						{
							std::string modelName = modelData[geometryFileIndex].GetModelName(modelIndex);
							std::replace_if(modelName.begin(), modelName.end(), std::isspace, '_');
							if (hasPos || hasRot)
							{
								AddParametersForNoSkeleton(animation, modelIndex, modelName, AnimationBoneParameter_TransX);
							}
							if (hasPos || hasRot)
							{
								AddParametersForNoSkeleton(animation, modelIndex, modelName, AnimationBoneParameter_RotX);
							}
							if (hasScl)
							{
								AddParametersForNoSkeleton(animation, modelIndex, modelName, AnimationBoneParameter_SclX);
							}
						}
					}
					else														// Skeletal mesh
					{
						ProgressRange animationProgressRange(modelProgressRange, animProgressRangeSlice);

						// Read the animation flags.
						SkeletonDataMap::const_iterator skeletonDataPos = skeletonData.find(std::make_pair(geometryFileIndex, modelIndex));
						if (skeletonDataPos != skeletonData.end())
						{
							const SkeletonData& skeletonDataInstance = (*skeletonDataPos).second;
							AnimationData animationData(skeletonDataInstance.GetBoneCount(), FPS, 0);
							source->ReadAnimationFlags(context, &animationData, &geometryFileData, &modelData[geometryFileIndex], modelIndex, &skeletonDataInstance, animationIndex);

							int rootIndexCount = skeletonDataInstance.GetRootCount();
							float rootProgressRangeSlice = 1.0f / (rootIndexCount > 0 ? float(rootIndexCount) : 1.0f);
							for (int rootIndexIndex = 0; rootIndexIndex < rootIndexCount; ++rootIndexIndex)
							{
								ProgressRange rootProgressRange(animationProgressRange, rootProgressRangeSlice);

								int rootBoneIndex = skeletonDataInstance.GetRootIndex(rootIndexIndex);

								// Generate the list of animated parameters for this animation - by generating the names in one place, we can
								// use them in both passes to refer to each other.
								AddParametersRecursive(animation, animationData, skeletonDataInstance, rootBoneIndex, rootProgressRange);
							}
						}
					}
				}
			}
		}
	}

	void GenerateSkinControllerList(IExportContext* context, std::vector<SkinControllerEntry>& controllers, std::map<std::pair<int, int>, int>& modelControllerMap, SkeletonDataMap& skeletonData, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData, std::map<std::pair<int, int>, int>& modelGeometryMap, std::vector<GeometryEntry>& geometries, ProgressRange& progressRange)
	{
		for (int geometryFileIndex = 0, geometryFileCount = geometryFileData.GetGeometryFileCount(); geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
			{
				std::map<std::pair<int, int>, int>::iterator modelGeometryPos = modelGeometryMap.find(std::make_pair(geometryFileIndex, modelIndex));
				SkeletonDataMap::const_iterator skeletonDataPos = skeletonData.find(std::make_pair(geometryFileIndex, modelIndex));
				if (skeletonDataPos != skeletonData.end() && modelGeometryPos != modelGeometryMap.end())
				{
					int controllerIndex = int(controllers.size());
					controllers.resize(controllers.size() + 1);

					SkinControllerEntry& entry = controllers.back();
					entry.name = StringHelpers::Format("controller_%d", controllerIndex);
					entry.geometryFileIndex = geometryFileIndex;
					entry.modelIndex = modelIndex;

					modelControllerMap.insert(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), controllerIndex));
				}
			}
		}
	}

	void GenerateMorphControllerList(IExportContext* context, std::vector<MorphControllerEntry>& morphControllers, std::map<std::pair<int, int>, int>& modelMorphControllerMap, MorphDataMap& morphData, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData, std::map<std::pair<int, int>, int>& modelGeometryMap, std::vector<GeometryEntry>& geometries, ProgressRange& progressRange)
	{
		for (int geometryFileIndex = 0, geometryFileCount = geometryFileData.GetGeometryFileCount(); geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
			{
				std::map<std::pair<int, int>, int>::iterator modelGeometryPos = modelGeometryMap.find(std::make_pair(geometryFileIndex, modelIndex));
				MorphDataMap::const_iterator morphDataPos = morphData.find(std::make_pair(geometryFileIndex, modelIndex));
				if (morphDataPos != morphData.end() && modelGeometryPos != modelGeometryMap.end())
				{
					int controllerIndex = int(morphControllers.size());
					morphControllers.resize(morphControllers.size() + 1);

					MorphControllerEntry& entry = morphControllers.back();
					entry.name = StringHelpers::Format("morphController_%d", controllerIndex);
					entry.geometryFileIndex = geometryFileIndex;
					entry.modelIndex = modelIndex;

					modelMorphControllerMap.insert(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), controllerIndex));
				}
			}
		}
	}

	void GenerateEffectsList(IExportContext* context, std::map<int, int>& materialFXMap, std::vector<EffectsEntry>& effects, MaterialData& materialData)
	{
		for (int materialIndex = 0, materialCount = materialData.GetMaterialCount(); materialIndex < materialCount; ++materialIndex)
		{
			std::string name;

			std::string mtlName = materialData.GetName(materialIndex);
			assert(!mtlName.empty());

			name = mtlName;

			const int id = materialData.GetID(materialIndex);
			assert(id>=0);

			name += StringHelpers::Format("-%d", id + 1);

			name += "-submat";

			name += "-effect";

			const int effectIndex = int(effects.size());
			effects.push_back(EffectsEntry(name));

			materialFXMap.insert(std::make_pair(materialIndex, effectIndex));
		}
	}

	void GenerateMaterialList(IExportContext* context, std::map<int, int>& materialMaterialMap, std::map<int, int>& materialFXMap, std::vector<EffectsEntry>& effects, std::vector<MaterialEntry>& materials, MaterialData& materialData)
	{
		for (int materialIndex = 0, materialCount = materialData.GetMaterialCount(); materialIndex < materialCount; ++materialIndex)
		{
			// Material needs to be named according to a specific format - this communicates information to
			// the resource compiler about the settings to be used for the material.
			// Format is: <Library>__<ID>__<Name>[__<param>...]

			std::string name;

			std::string mtlName = materialData.GetName(materialIndex);
			assert(!mtlName.empty());

			std::string mtlProperties = materialData.GetProperties(materialIndex);
			assert(!mtlProperties.empty());

			name = mtlName;

			const int id = materialData.GetID(materialIndex);
			assert(id>=0);

			name += StringHelpers::Format("__%d", id + 1);

			name += "__";
			name += materialData.GetSubMatName(materialIndex);

			name += mtlProperties;

			const int index = int(materials.size());
			materials.push_back(MaterialEntry(name));

			materialMaterialMap.insert(std::make_pair(materialIndex, index));
		}
	}

	void GenerateGeometryList(IExportContext* context, std::map<std::pair<int, int>, int>& modelGeometryMap, std::vector<GeometryEntry>& geometries, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData)
	{
		for (int geometryFileIndex = 0, geometryFileCount = geometryFileData.GetGeometryFileCount(); geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
			{
				if (modelData[geometryFileIndex].HasGeometry(modelIndex))
				{
					std::string geometryName = std::string(geometryFileData.GetGeometryFileName(geometryFileIndex)) + "_" + modelData[geometryFileIndex].GetModelName(modelIndex) + "_geometry";
					int geometryIndex = int(geometries.size());
					geometries.push_back(GeometryEntry(geometryName, geometryFileIndex, modelIndex));
					modelGeometryMap.insert(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), geometryIndex));
				}
			}
		}
	}

	void GenerateBoneGeometryList(IExportContext* context, std::map<std::pair<std::pair<int, int>, int>, int>& boneGeometryMap, std::vector<BoneGeometryEntry>& boneGeometries, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData, SkeletonDataMap& skeletonData)
	{
		for (int geometryFileIndex = 0, geometryFileCount = geometryFileData.GetGeometryFileCount(); geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
			{
				SkeletonDataMap::iterator skeletonDataPos = skeletonData.find(std::make_pair(geometryFileIndex, modelIndex));
				if (skeletonDataPos != skeletonData.end())
				{
					SkeletonData& modelSkeletonData = (*skeletonDataPos).second;
					for (int boneIndex = 0, boneCount = modelSkeletonData.GetBoneCount(); boneIndex < boneCount; ++boneIndex)
					{
						if (modelSkeletonData.HasGeometry(boneIndex))
						{
							std::string geometryName = std::string(geometryFileData.GetGeometryFileName(geometryFileIndex)) + "_" + modelData[geometryFileIndex].GetModelName(modelIndex) + "_" + modelSkeletonData.GetSafeName(boneIndex) + "_boneGeometry";
							int geometryIndex = int(boneGeometries.size());
							boneGeometries.push_back(BoneGeometryEntry(geometryName, geometryFileIndex, modelIndex, boneIndex));
							boneGeometryMap.insert(std::make_pair(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), boneIndex), geometryIndex));
						}
					}
				}
			}
		}
	}

	void GenerateMorphGeometryList(IExportContext* context, std::map<std::pair<std::pair<int, int>, int>, int>& morphGeometryMap, std::vector<MorphGeometryEntry>& morphGeometries, GeometryFileData& geometryFileData, const std::vector<ModelData>& modelData, MorphDataMap& morphData)
	{
		for (int geometryFileIndex = 0, geometryFileCount = geometryFileData.GetGeometryFileCount(); geometryFileIndex < geometryFileCount; ++geometryFileIndex)
		{
			for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
			{
				MorphDataMap::iterator morphDataPos = morphData.find(std::make_pair(geometryFileIndex, modelIndex));
				if (morphDataPos != morphData.end())
				{
					MorphData& modelMorphData = (*morphDataPos).second;
					for (int morphIndex = 0, morphCount = modelMorphData.GetMorphCount(); morphIndex < morphCount; ++morphIndex)
					{
						std::string geometryName = std::string(geometryFileData.GetGeometryFileName(geometryFileIndex)) + "_" + modelData[geometryFileIndex].GetModelName(modelIndex) + "_" + modelMorphData.GetMorphFullName(morphIndex) + "_morphGeometry";
						int geometryIndex = int(morphGeometries.size());
						morphGeometries.push_back(MorphGeometryEntry(geometryName, modelMorphData.GetMorphName(morphIndex), geometryFileIndex, modelIndex, morphIndex));
						morphGeometryMap.insert(std::make_pair(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), morphIndex), geometryIndex));
					}
				}
			}
		}
	}

	void GenerateIKPropertyList(const SkeletonData& skeletonData, int boneIndex, std::vector<std::pair<std::string, std::string> >& propertyList)
	{
		// Loop through each axis, adding the properties for this axis to the list.
		for (int axis = 0; axis < 3; ++axis)
		{
			// Add the limit properties for this axis.
			const char* extremeNames[] = {"min", "max"};
			for (int extreme = 0; extreme < 2; ++extreme)
			{
				std::string key;
				key += ('x' + axis);
				key += extremeNames[extreme];
				if (skeletonData.HasLimit(boneIndex, ISkeletonData::Axis(axis), ISkeletonData::Limit(extreme)))
				{
					float limit = skeletonData.GetLimit(boneIndex, ISkeletonData::Axis(axis), ISkeletonData::Limit(extreme));
					std::string val = StringHelpers::Format("%f", limit * 180.0f / 3.14159f); // Convert to degrees.
					propertyList.push_back(std::make_pair(key, val));
				}
			}

			// Add the remaining properties.
			const char* propNames[] = {"damping", "springangle", "springtension"};
			typedef bool (SkeletonData::*HasMember)(int boneIndex, ISkeletonData::Axis axis) const;
			typedef float (SkeletonData::*GetMember)(int boneIndex, ISkeletonData::Axis axis) const;
			HasMember hasMembers[] = {&SkeletonData::HasAxisDamping, &SkeletonData::HasSpringAngle, &SkeletonData::HasSpringTension};
			GetMember getMembers[] = {&SkeletonData::GetAxisDamping, &SkeletonData::GetSpringAngle, &SkeletonData::GetSpringTension};
			for (int propIndex = 0; propIndex < 3; ++propIndex)
			{
				std::string key;
				key += ('x' + axis);
				key += propNames[propIndex];
				if ((skeletonData.*(hasMembers[propIndex]))(boneIndex, ISkeletonData::Axis(axis)))
				{
					float value = (skeletonData.*(getMembers[propIndex]))(boneIndex, ISkeletonData::Axis(axis));
					std::string val = StringHelpers::Format("%f", value);
					propertyList.push_back(std::make_pair(key, val));
				}
			}
		}
	}

	void GenerateBoneList(IExportContext* context, BoneDataMap& boneDataMap, const SkeletonDataMap& skeletonData, const std::vector<ModelData>& modelData)
	{
		for (SkeletonDataMap::const_iterator skeletonDataPos = skeletonData.begin(), skeletonDataEnd = skeletonData.end(); skeletonDataPos != skeletonDataEnd; ++skeletonDataPos)
		{
			int geometryFileIndex = (*skeletonDataPos).first.first;
			int modelIndex = (*skeletonDataPos).first.second;
			const SkeletonData& skeleton = (*skeletonDataPos).second;

			BoneDataMap::iterator boneDataPos = boneDataMap.insert(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), std::vector<BoneEntry>())).first;
			std::vector<BoneEntry>& bones = (*boneDataPos).second;
			bones.resize(skeleton.GetBoneCount());

			std::string modelName = modelData[geometryFileIndex].GetModelName(modelIndex);

			for (int boneIndex = 0, boneCount = skeleton.GetBoneCount(); boneIndex < boneCount; ++boneIndex)
			{
				typedef std::string BoneEntry::*BoneNamePtr;
				BoneNamePtr boneNames[3] = {&BoneEntry::name, &BoneEntry::physName, &BoneEntry::parentFrameName};
				const char* suffixes[3] = {"", " Phys", " Phys ParentFrame"};
				std::vector<std::pair<std::string, std::string> > properties[3];

				// Add the IK properties to the phys bone.
				GenerateIKPropertyList(skeleton, boneIndex, properties[1]);
				
				for (int nameIndex = 0; nameIndex < 3; ++nameIndex)
				{
					std::string unsafeName = skeleton.GetName(boneIndex);
					unsafeName += suffixes[nameIndex];
					bool containsSpaces = (unsafeName.find_first_of(" \t") != std::string::npos);
					std::string name = unsafeName;
					if (containsSpaces)
					{
						std::string overrideName = unsafeName;
						std::replace_if(overrideName.begin(), overrideName.end(), std::isspace, '*');

						std::string safeName = unsafeName;
						std::replace_if(safeName.begin(), safeName.end(), std::isspace, '_');

						name = safeName + "%" + modelName + "%" + "--PRprops_name=" + overrideName;

						// Add all the properties.
						for (size_t propIndex = 0, propCount = properties[nameIndex].size(); propIndex < propCount; ++propIndex)
							name += "_" + properties[nameIndex][propIndex].first + "=" + properties[nameIndex][propIndex].second;
						
						name += "__";
					}
					else
					{
						name = unsafeName + "%" + modelName + "%";
					}
					(bones[boneIndex].*(boneNames[nameIndex])) = name;
				}
			}
		}
	}

	void WriteAnimationList(XMLWriter& writer, std::vector<AnimationEntry>& animations, ProgressRange& progressRange)
	{
		// Write out all the animations in the library_animation_clips element. Each animation lists the name and timespan
		// of the clip, and the controllers for each model parameter. The actual animation data is written out in a separate pass.
		XMLWriter::Element libraryAnimationClipsElement(writer, "library_animation_clips");

		for (int animationEntryIndex = 0, animationEntryCount = int(animations.size()); animationEntryIndex < animationEntryCount; ++animationEntryIndex)
		{
			AnimationEntry& entry = animations[animationEntryIndex];

			XMLWriter::Element animationClipElement(writer, "animation_clip");
			animationClipElement.Attribute("start", entry.start);
			animationClipElement.Attribute("end", entry.stop);
			animationClipElement.Attribute("id", entry.name);

			// For each parameter write out a reference to the controller for that parameter.
			for (int parameterEntryIndex = 0, parameterEntryCount = int(entry.parameters.size()); parameterEntryIndex < parameterEntryCount; ++parameterEntryIndex)
			{
				AnimationBoneParameterEntry& parameter = entry.parameters[parameterEntryIndex];

				XMLWriter::Element instanceAnimationElement(writer, "instance_animation");
				instanceAnimationElement.Attribute("url", std::string("#") + parameter.name);
			}
		}
	}

	void WriteAnimationTags( 
		ProgressRange& animationEntryProgressRange, 
		const AnimationEntry& entry,
		const IAnimationData * animationData,
		XMLWriter& writer,
		const std::vector<BoneEntry> *pBones,
		const IModelData *pModelData)
	{
		// Loop through all the parameters of all the models.
		ProgressRange writeAnimProgressRange(animationEntryProgressRange, 0.5f);
		for (int parameterEntryIndex = 0, parameterEntryCount = int(entry.parameters.size()); parameterEntryIndex < parameterEntryCount; ++parameterEntryIndex)
		{
			const AnimationBoneParameterEntry& parameter = entry.parameters[parameterEntryIndex];

			int frameCount = 0;
			switch (parameter.parameter)
			{
			case AnimationBoneParameter_TransX:
			case AnimationBoneParameter_TransY:
			case AnimationBoneParameter_TransZ: 
				frameCount = animationData->GetFrameCountPos(parameter.boneIndex);
				break;
			case AnimationBoneParameter_RotX:
			case AnimationBoneParameter_RotY:
			case AnimationBoneParameter_RotZ:   
				frameCount = animationData->GetFrameCountRot(parameter.boneIndex);
				break;
			case AnimationBoneParameter_SclX:
			case AnimationBoneParameter_SclY:
			case AnimationBoneParameter_SclZ:   
				frameCount = animationData->GetFrameCountScl(parameter.boneIndex);
				break;
			}

			XMLWriter::Element animationElement(writer, "animation");
			animationElement.Attribute("id", parameter.name);

			std::string inputID = parameter.name + "-input";
			std::string outputID = parameter.name + "-output";
			std::string interpID = parameter.name + "-interp";
			std::string tcbID = parameter.name + "-tcb";
			std::string easeinoutID = parameter.name + "-easeinout";

			// Write out the times.
			{
				XMLWriter::Element inputElement(writer, "source");
				inputElement.Attribute("id", inputID);
				std::string arrayID = inputID + "-array";
				{
					XMLWriter::Element array(writer, "float_array");
					array.Attribute("count", frameCount);
					array.Attribute("id", arrayID);

					float floatBuffer[24];
					int bufferCount = 0;
					for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
					{
						switch (parameter.parameter)
						{
						case AnimationBoneParameter_TransX:
						case AnimationBoneParameter_TransY:
						case AnimationBoneParameter_TransZ:
							floatBuffer[bufferCount++] = animationData->GetFrameTimePos(parameter.boneIndex, frameIndex);
							break;
						case AnimationBoneParameter_RotX:
						case AnimationBoneParameter_RotY:
						case AnimationBoneParameter_RotZ: 
							floatBuffer[bufferCount++] = animationData->GetFrameTimeRot(parameter.boneIndex, frameIndex);
							break;
						case AnimationBoneParameter_SclX:
						case AnimationBoneParameter_SclY:
						case AnimationBoneParameter_SclZ:   
							floatBuffer[bufferCount++] = animationData->GetFrameTimeScl(parameter.boneIndex, frameIndex);
							break;
						}
						if( bufferCount == 24 )
						{
							array.ContentArrayFloat24( floatBuffer, bufferCount );
							bufferCount = 0;
						}
					}
					if( bufferCount > 0 )
					{
						array.ContentArrayFloat24( floatBuffer, bufferCount );
						bufferCount = 0;
					}
				}

				XMLWriter::Element techniqueCommonElement(writer, "technique_common");
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("source", arrayID);
				accessorElement.Attribute("count", frameCount);
				accessorElement.Attribute("stride", 1);
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "TIME");
				paramElement.Attribute("type", "float");
			}

			// Write out the values.
			{
				XMLWriter::Element inputElement(writer, "source");
				inputElement.Attribute("id", outputID);
				std::string arrayID = outputID + "-array";
				{
					XMLWriter::Element array(writer, "float_array");
					array.Attribute("count", frameCount);
					array.Attribute("id", arrayID);

					float floatBuffer[24];
					int bufferCount = 0;
					for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
					{
						const float* translation, *rotation, *scale;
						switch (parameter.parameter)
						{
						case AnimationBoneParameter_TransX:
						case AnimationBoneParameter_TransY:
						case AnimationBoneParameter_TransZ:
							animationData->GetFrameDataPos(parameter.boneIndex, frameIndex, translation);
							floatBuffer[bufferCount++] = translation[parameter.parameter-AnimationBoneParameter_TransX];
							break;
						case AnimationBoneParameter_RotX:
						case AnimationBoneParameter_RotY:
						case AnimationBoneParameter_RotZ:   
							animationData->GetFrameDataRot(parameter.boneIndex, frameIndex, rotation);
							floatBuffer[bufferCount++] = rotation[parameter.parameter-AnimationBoneParameter_RotX];
							break;
						case AnimationBoneParameter_SclX:
						case AnimationBoneParameter_SclY:
						case AnimationBoneParameter_SclZ:   
							animationData->GetFrameDataScl(parameter.boneIndex, frameIndex, scale);
							floatBuffer[bufferCount++] = scale[parameter.parameter-AnimationBoneParameter_SclX];
							break;
						}
						if( bufferCount == 24 )
						{
							array.ContentArrayFloat24( floatBuffer, bufferCount );
							bufferCount = 0;
						}
					}
					if( bufferCount > 0 )
					{
						array.ContentArrayFloat24( floatBuffer, bufferCount );
						bufferCount = 0;
					}
				}

				XMLWriter::Element techniqueCommonElement(writer, "technique_common");
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("source", arrayID);
				accessorElement.Attribute("count", frameCount);
				accessorElement.Attribute("stride", 1);
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "VALUE");
				paramElement.Attribute("type", "float");
			}

			// Write out the interpolation method.
			{
				XMLWriter::Element inputElement(writer, "source");
				inputElement.Attribute("id", interpID);
				std::string arrayID = interpID + "-array";
				{
					XMLWriter::Element array(writer, "Name_array");
					array.Attribute("count", frameCount);
					array.Attribute("id", arrayID);
					for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
						array.WriteDirectText(" CONSTANT");
				}

				XMLWriter::Element techniqueCommonElement(writer, "technique_common");
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("source", arrayID);
				accessorElement.Attribute("count", frameCount);
				accessorElement.Attribute("stride", 1);
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("name", "INTERPOLATION");
				paramElement.Attribute("type", "Name");
			}

			if (pModelData)				// only for the non-skeletal animation
			{
			// Write out the TCB values.
			{
				const int stride = 3;
				XMLWriter::Element inputElement(writer, "source");
				inputElement.Attribute("id", tcbID);
				std::string arrayID = tcbID + "-array";
				{
					XMLWriter::Element array(writer, "float_array");
					array.Attribute("count", frameCount*stride);
					array.Attribute("id", arrayID);
					for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
					{
						IAnimationData::TCB tcb;
						switch (parameter.parameter)
						{
						case AnimationBoneParameter_TransX:
						case AnimationBoneParameter_TransY:
						case AnimationBoneParameter_TransZ: 
							animationData->GetFrameTCBPos(parameter.boneIndex, frameIndex, tcb);
							break;
						case AnimationBoneParameter_RotX:
						case AnimationBoneParameter_RotY:
						case AnimationBoneParameter_RotZ:   
							animationData->GetFrameTCBRot(parameter.boneIndex, frameIndex, tcb);
							break;
						case AnimationBoneParameter_SclX:
						case AnimationBoneParameter_SclY:
						case AnimationBoneParameter_SclZ:   
							animationData->GetFrameTCBScl(parameter.boneIndex, frameIndex, tcb);
							break;
						}
						array.ContentArrayElement(tcb.tension);
						array.ContentArrayElement(tcb.continuity);
						array.ContentArrayElement(tcb.bias);
					}
				}

				XMLWriter::Element techniqueCommonElement(writer, "technique_common");
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("source", arrayID);
				accessorElement.Attribute("count", frameCount);
				accessorElement.Attribute("stride", stride);
				{
					XMLWriter::Element paramElement(writer, "param");
					paramElement.Attribute("name", "TENSION");
					paramElement.Attribute("type", "float");
				}
				{
					XMLWriter::Element paramElement(writer, "param");
					paramElement.Attribute("name", "CONTINUITY");
					paramElement.Attribute("type", "float");
				}
				{
					XMLWriter::Element paramElement(writer, "param");
					paramElement.Attribute("name", "BIAS");
					paramElement.Attribute("type", "float");
				}
			}

			// Write out the ease-in/-out values.
			{
				const int stride = 2;
				XMLWriter::Element inputElement(writer, "source");
				inputElement.Attribute("id", easeinoutID);
				std::string arrayID = easeinoutID + "-array";
				{
					XMLWriter::Element array(writer, "float_array");
					array.Attribute("count", frameCount*stride);
					array.Attribute("id", arrayID);
					for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
					{
						IAnimationData::Ease ease;
						switch (parameter.parameter)
						{
						case AnimationBoneParameter_TransX:
						case AnimationBoneParameter_TransY:
						case AnimationBoneParameter_TransZ: 
							animationData->GetFrameEaseInOutPos(parameter.boneIndex, frameIndex, ease);
							break;
						case AnimationBoneParameter_RotX:
						case AnimationBoneParameter_RotY:
						case AnimationBoneParameter_RotZ:   
							animationData->GetFrameEaseInOutRot(parameter.boneIndex, frameIndex, ease);
							break;
						case AnimationBoneParameter_SclX:
						case AnimationBoneParameter_SclY:
						case AnimationBoneParameter_SclZ:   
							animationData->GetFrameEaseInOutScl(parameter.boneIndex, frameIndex, ease);
							break;
						}
						array.ContentArrayElement(ease.in);
						array.ContentArrayElement(ease.out);
					}
				}

				XMLWriter::Element techniqueCommonElement(writer, "technique_common");
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("source", arrayID);
				accessorElement.Attribute("count", frameCount);
				accessorElement.Attribute("stride", stride);
				{
					XMLWriter::Element paramElement(writer, "param");
					paramElement.Attribute("name", "EASE_IN");
					paramElement.Attribute("type", "float");
				}
				{
					XMLWriter::Element paramElement(writer, "param");
					paramElement.Attribute("name", "EASE_OUT");
					paramElement.Attribute("type", "float");
				}
			}
			}

			// Write out the sampler element.
			std::string samplerID = parameter.name + "-sampler";
			{
				XMLWriter::Element samplerElement(writer, "sampler");
				samplerElement.Attribute("id", samplerID);
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "INPUT");
					inputElement.Attribute("source", std::string("#") + inputID);
				}
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "OUTPUT");
					inputElement.Attribute("source", std::string("#") + outputID);
				}
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "INTERPOLATION");
					inputElement.Attribute("source", std::string("#") + interpID);
				}
				if (pModelData)			// only for the non-skeletal animation
				{
					{
						XMLWriter::Element inputElement(writer, "input");
						inputElement.Attribute("semantic", "TCB");
						inputElement.Attribute("source", std::string("#") + tcbID);
					}
					{
						XMLWriter::Element inputElement(writer, "input");
						inputElement.Attribute("semantic", "EASE_IN_OUT");
						inputElement.Attribute("source", std::string("#") + easeinoutID);
					}
				}
			}

			// Write out the channel element.
			XMLWriter::Element channelElement(writer, "channel");
			channelElement.Attribute("source", std::string("#") + samplerID);
			std::string targetName;
			if (pBones)
				targetName = (*pBones)[parameter.boneIndex].name;
			else
			{
				assert(pModelData);
				targetName = pModelData->GetModelName(parameter.boneIndex);
			}
			targetName = targetName + "/" + parameterTargetStrings[parameter.parameter];
			channelElement.Attribute("target", targetName);
		}
	}

	void WriteAnimationData(
		IExportContext* context, 
		XMLWriter& writer, 
		std::vector<AnimationEntry>& animations, 
		GeometryFileData& geometryFileData, 
		const std::vector<ModelData>& modelData, 
		SkeletonDataMap& skeletonData, 
		const BoneDataMap& boneDataMap, 
		IExportSource* source, 
		ProgressRange& progressRange)
	{
		XMLWriter::Element libraryAnimationsElement(writer, "library_animations");

		int animationEntryCount = int(animations.size());
		float animationEntryProgressSlice = (1.0f / (animationEntryCount ? float(animationEntryCount) : 1.0f));
		for (int animationEntryIndex = 0; animationEntryIndex < animationEntryCount; ++animationEntryIndex)
		{
			ProgressRange animationEntryProgressRange(progressRange, animationEntryProgressSlice);

			AnimationEntry& entry = animations[animationEntryIndex];

			if (skeletonData.empty())				// Non-skeletal mesh
			{
				IAnimationData *animationData = NULL;
				{
					ProgressRange readAnimProgressRange(animationEntryProgressRange, 0.5f);
					animationData = source->ReadAnimation(context, &geometryFileData, &modelData[0], -1, NULL, animationEntryIndex, FPS);
				}

				if( animationData )
				{
					WriteAnimationTags(animationEntryProgressRange, entry, animationData, writer, NULL, &modelData[0]);
					delete animationData;
				}
			}
			else														// Skeletal mesh
			{
				// Read the animation data.
				SkeletonDataMap::const_iterator skeletonDataPos = skeletonData.find(std::make_pair(entry.geometryFileIndex, entry.modelIndex));
				if (skeletonDataPos != skeletonData.end())
				{
					IAnimationData *animationData = NULL;
					const SkeletonData& skeletonDataInstance = (*skeletonDataPos).second;
					{
						ProgressRange readAnimProgressRange(animationEntryProgressRange, 0.5f);

						animationData = source->ReadAnimation(context, &geometryFileData, &modelData[entry.modelIndex], entry.modelIndex, &skeletonDataInstance, entry.animationIndex, FPS);
					}

					// Look up the bone entries for this model.
					BoneDataMap::const_iterator boneDataPos = boneDataMap.find(std::make_pair(entry.geometryFileIndex, entry.modelIndex));
					const std::vector<BoneEntry>& bones = (*boneDataPos).second;

					if( animationData )
					{
						WriteAnimationTags(animationEntryProgressRange, entry, animationData, writer, &bones, NULL);
						delete animationData;
					}
				}
			}
		}
	}

	void WriteEffects(XMLWriter& writer, std::vector<EffectsEntry>& effects, ProgressRange& progressRange)
	{
		XMLWriter::Element libraryEffectsElement(writer, "library_effects");
		for (int effectIndex = 0, effectCount = int(effects.size()); effectIndex < effectCount; ++effectIndex)
		{
			XMLWriter::Element effectElement(writer, "effect");
			effectElement.Attribute("id", effects[effectIndex].name);

			// Write out dummy effects values.
			XMLWriter::Element profileElement(writer, "profile_COMMON");
			XMLWriter::Element techniqueElement(writer, "technique");
			techniqueElement.Attribute("sid", "default");
			XMLWriter::Element phongElement(writer, "phong");
			{
				XMLWriter::Element emissionElement(writer, "emission");
				XMLWriter::Element colorElement(writer, "color");
				colorElement.Attribute("sid", "emission");
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(1.0f);
			}
			{
				XMLWriter::Element ambientElement(writer, "ambient");
				XMLWriter::Element colorElement(writer, "color");
				colorElement.Attribute("sid", "ambient");
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(1.0f);
			}
			{
				XMLWriter::Element diffuseElement(writer, "diffuse");
				XMLWriter::Element colorElement(writer, "color");
				colorElement.Attribute("sid", "diffuse");
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(1.0f);
			}
			{
				XMLWriter::Element specularElement(writer, "specular");
				XMLWriter::Element colorElement(writer, "color");
				colorElement.Attribute("sid", "specular");
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(1.0f);
			}
			{
				XMLWriter::Element shininessElement(writer, "shininess");
				XMLWriter::Element floatElement(writer, "float");
				floatElement.Attribute("sid", "shininess");
				floatElement.ContentArrayElement(0.0f);
			}
			{
				XMLWriter::Element reflectiveElement(writer, "reflective");
				XMLWriter::Element colorElement(writer, "color");
				colorElement.Attribute("sid", "reflective");
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(1.0f);
			}
			{
				XMLWriter::Element reflectivityElement(writer, "reflectivity");
				XMLWriter::Element floatElement(writer, "float");
				floatElement.Attribute("sid", "reflectivity");
				floatElement.ContentArrayElement(0.0f);
			}
			{
				XMLWriter::Element transparentElement(writer, "transparent");
				transparentElement.Attribute("opaque", "RGB_ZERO");
				XMLWriter::Element colorElement(writer, "color");
				colorElement.Attribute("sid", "transparent");
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
				colorElement.ContentArrayElement(0.0f);
			}
			{
				XMLWriter::Element transparencyElement(writer, "transparency");
				XMLWriter::Element floatElement(writer, "float");
				floatElement.Attribute("sid", "transparency");
				floatElement.ContentArrayElement(0.0f);
			}
			{
				XMLWriter::Element refractionElement(writer, "index_of_refraction");
				XMLWriter::Element floatElement(writer, "float");
				floatElement.Attribute("sid", "index_of_refraction");
				floatElement.ContentArrayElement(0.0f);
			}
		}
	}

	void WriteControllers(
		XMLWriter& writer, 
		IExportContext* context, 
		IExportSource* exportSource, 
		std::vector<SkinControllerEntry>& skinControllers, 
		std::vector<MorphControllerEntry>& morphControllers, 
		std::map<std::pair<int, int>, int> modelMorphControllerMap, 
		GeometryFileData& geometryFileData, 
		const std::vector<ModelData>& modelData, 
		SkeletonDataMap& skeletonData, 
		MorphDataMap& morphData, 
		std::vector<MorphGeometryEntry>& morphGeometries, 
		std::map<std::pair<std::pair<int, int>, int>, int>& morphGeometryMap, 
		std::vector<GeometryEntry>& geometries, 
		std::map<std::pair<int, int>, int>& modelGeometryMap, 
		const BoneDataMap& boneDataMap, 
		ProgressRange& progressRange)
	{
		//<library_controllers>
		XMLWriter::Element libraryControllersElement(writer, "library_controllers");
		for (int controllerIndex = 0, controllerCount = int(skinControllers.size()); controllerIndex < controllerCount; ++controllerIndex)
		{
			SkinControllerEntry& controller = skinControllers[controllerIndex];
			SkeletonDataMap::iterator skeletonDataPos = skeletonData.find(std::make_pair(controller.geometryFileIndex, controller.modelIndex));
			if (skeletonDataPos == skeletonData.end())
				continue;

			SkeletonData skeleton = (*skeletonDataPos).second;

		//	<controller id="controllers_0">
			XMLWriter::Element controllerElement(writer, "controller");
			controllerElement.Attribute("id", controller.name);

		//		<skin source="#geometries_60">
			std::string geometryName;
			bool sourceFound = false;
			{
				std::map<std::pair<int, int>, int>::const_iterator modelMorphControllerMapPos = modelMorphControllerMap.find(std::make_pair(controller.geometryFileIndex, controller.modelIndex));
				int morphControllerIndex = (modelMorphControllerMapPos != modelMorphControllerMap.end() ? (*modelMorphControllerMapPos).second : -1);
				geometryName = (morphControllerIndex >= 0 ? morphControllers[morphControllerIndex].name : "MISSING MORPH CONTROLLER NAME");
				sourceFound = (morphControllerIndex >= 0);
			}
			if (!sourceFound)
			{
				std::map<std::pair<int, int>, int>::const_iterator geometryMapPos = modelGeometryMap.find(std::make_pair(controller.geometryFileIndex, controller.modelIndex));
				int geometryIndex = (geometryMapPos != modelGeometryMap.end() ? (*geometryMapPos).second : -1);
				geometryName = (geometryIndex >= 0 ? geometries[geometryIndex].name : "MISSING GEOMETRY NAME");
				sourceFound = (geometryIndex >= 0);
			}
			XMLWriter::Element skinElement(writer, "skin");
			skinElement.Attribute("source", "#" + geometryName);

		//			<bind_shape_matrix>
		//				1.000000 0.000000 0.000000 0.000000 
		//				0.000000 1.000000 0.000000 0.000000 
		//				0.000000 0.000000 1.000000 0.000000 
		//				0.000000 0.000000 0.000000 1.000000
		//			</bind_shape_matrix>
			{
				XMLWriter::Element bindMatrixElement(writer, "bind_shape_matrix");
				bindMatrixElement.ContentArrayElement(1.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(1.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(1.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(0.0f);
				bindMatrixElement.ContentArrayElement(1.0f);
			}

		//			<source id="controllers_0-Joints">
			std::string jointsSourceName = controller.name + "_joints";
			{
				XMLWriter::Element jointsSourceElement(writer, "source");
				jointsSourceElement.Attribute("id", jointsSourceName);

		//				<IDREF_array count="61" id="controllers_0-Joints-array"></IDREF_array>
				const std::vector<BoneEntry>& bones = (*boneDataMap.find(std::make_pair(controller.geometryFileIndex, controller.modelIndex))).second;
				std::string arrayName = jointsSourceName + "_array";
				{
					XMLWriter::Element idArrayElement(writer, "IDREF_array");
					idArrayElement.Attribute("id", arrayName);
					idArrayElement.Attribute("count", skeleton.GetBoneCount());
					for (int boneIndex = 0, boneCount = skeleton.GetBoneCount(); boneIndex < boneCount; ++boneIndex)
						idArrayElement.ContentArrayElement(bones[boneIndex].name);
				}
			
		//				<technique_common>
				{
					XMLWriter::Element techniqueCommonElement(writer, "technique_common");
		//					<accessor count="61" stride="1" source="#controllers_0-Joints-array">
					XMLWriter::Element accessorElement(writer, "accessor");
					accessorElement.Attribute("count", skeleton.GetBoneCount());
					accessorElement.Attribute("stride", 1);
					accessorElement.Attribute("source", "#" + arrayName);

		//						<param type="IDREF"/>
					XMLWriter::Element paramElement(writer, "param");
					paramElement.Attribute("type", std::string("IDREF"));
		//					</accessor>
		//				</technique_common>
		//			</source>
				}
			}

		//			<source id="controllers_0-Matrices">
			std::string matricesSourceName = controller.name + "_matrices";
			{
				XMLWriter::Element sourceElement(writer, "source");
				sourceElement.Attribute("id", matricesSourceName);

		//				<float_array count="976" id="controllers_0-Matrices-array"></float_array>
				std::string arrayName = matricesSourceName + "_array";
				{
					XMLWriter::Element arrayElement(writer, "float_array");
					arrayElement.Attribute("id", arrayName);
					arrayElement.Attribute("count", skeleton.GetBoneCount() * 16);
					arrayElement.ContentLine("");
					for (int boneIndex = 0, boneCount = skeleton.GetBoneCount(); boneIndex < boneCount; ++boneIndex)
					{
						Vec3 scaleParams;
						skeleton.GetScale((float*)&scaleParams, boneIndex);
						Matrix44 scale = Matrix33::CreateScale(scaleParams);

						Ang3 rotationParams;
						skeleton.GetRotation((float*)&rotationParams, boneIndex);
						Matrix44 rotation = Matrix33::CreateRotationXYZ(rotationParams);

						Vec3 translationParams;
						skeleton.GetTranslation((float*)&translationParams, boneIndex);
						Matrix44 translation(IDENTITY);
						translation.SetTranslation(translationParams);

						Matrix44 transform = translation * (rotation * scale);
						transform.Invert();
						for (int i = 0; i < 4; ++i)
						{
							for (int j = 0; j < 4; ++j)
								arrayElement.ContentArrayElement(transform(i, j));
							arrayElement.ContentLine("");
						}
					}
				}

		//				<technique_common>
				XMLWriter::Element techniqueCommonElement(writer, "technique_common");

		//					<accessor count="61" stride="16" source="#controllers_0-Matrices-array">
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("count", skeleton.GetBoneCount());
				accessorElement.Attribute("stride", 16);
				accessorElement.Attribute("source", "#" + arrayName);

		//						<param type="float4x4"/>
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("type", std::string("float4x4"));

		//					</accessor>
		//				</technique_common>
		//			</source>
			}

			// Read in the skinning info.
			SkinningData skinningData;
			exportSource->ReadSkinning(context, &skinningData, &modelData[controller.geometryFileIndex], controller.modelIndex, &skeleton);

			// Build a single array of weights.
			std::vector<float> weightsArray;
			std::vector<std::vector<int> > weightIndexArray;
			int vertexCount = skinningData.GetVertexCount();
			weightIndexArray.resize(vertexCount);
			for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
			{
				int linkCount = skinningData.GetBoneLinkCount(vertexIndex);
				weightIndexArray[vertexIndex].resize(linkCount);
				for (int linkIndex = 0; linkIndex < linkCount; ++linkIndex)
				{
					int weightIndex = int(weightsArray.size());
					weightsArray.push_back(skinningData.GetWeight(vertexIndex, linkIndex));
					weightIndexArray[vertexIndex][linkIndex] = weightIndex;
				}
			}

		//			<source id="controllers_0-Weights">
			std::string weightsSourceName = controller.name + "_weights";
			{
				XMLWriter::Element weightsSourceElement(writer, "source");
				weightsSourceElement.Attribute("id", weightsSourceName);

		//				<float_array count="3957" id="controllers_0-Weights-array"></float_array>
				int weightCount = int(weightsArray.size());
				std::string arrayName = weightsSourceName + "_array";
				{

					XMLWriter::Element floatArrayElement(writer, "float_array");
					floatArrayElement.Attribute("count", weightCount);
					floatArrayElement.Attribute("id", arrayName);

					for (int weightIndex = 0; weightIndex < weightCount; ++weightIndex)
						floatArrayElement.ContentArrayElement(weightsArray[weightIndex]);
				}

		//				<technique_common>
				XMLWriter::Element techniqueCommonElement(writer, "technique_common");

		//					<accessor count="3957" stride="1" source="#controllers_0-Weights-array">
				XMLWriter::Element accessorElement(writer, "accessor");
				accessorElement.Attribute("count", weightCount);
				accessorElement.Attribute("stride", 1);
				accessorElement.Attribute("source", "#" + arrayName);

		//						<param type="float"/>
				XMLWriter::Element paramElement(writer, "param");
				paramElement.Attribute("type", "float");

		//					</accessor>
		//				</technique_common>
		//			</source>
			}

			{
		//			<joints>
				XMLWriter::Element jointsElement(writer, "joints");

		//				<input  semantic="JOINT" source="#controllers_0-Joints"></input >
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "JOINT");
					inputElement.Attribute("source", "#" + jointsSourceName);
				}

		//				<input  semantic="INV_BIND_MATRIX" source="#controllers_0-Matrices"></input >
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "INV_BIND_MATRIX");
					inputElement.Attribute("source", "#" + matricesSourceName);
				}

		//			</joints>
			}

			{
		//			<vertex_weights count="2157">
				XMLWriter::Element vertexWeightsElement(writer, "vertex_weights");
				int vertexCount = skinningData.GetVertexCount();
				vertexWeightsElement.Attribute("count", vertexCount);

		//				<input  semantic="JOINT" offset="0" source="#controllers_0-Joints"></input >
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "JOINT");
					inputElement.Attribute("offset", 0);
					inputElement.Attribute("source", "#" + jointsSourceName);
				}

		//				<input  semantic="WEIGHT" offset="1" source="#controllers_0-Weights"></input >
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "WEIGHT");
					inputElement.Attribute("offset", 1);
					inputElement.Attribute("source", "#" + weightsSourceName);
				}

		//				<vcount></vcount>
				{
					XMLWriter::Element vcountElement(writer, "vcount");

					for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						vcountElement.ContentArrayElement(int(weightIndexArray[vertexIndex].size()));
				}

		//				<v>
				{
					XMLWriter::Element vElement(writer, "v");
					vElement.ContentLine("");

					for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
					{
						for (int linkIndex = 0, linkCount = int(weightIndexArray[vertexIndex].size()); linkIndex < linkCount; ++linkIndex)
						{
							vElement.ContentArrayElement(skinningData.GetBoneIndex(vertexIndex, linkIndex));
							vElement.ContentArrayElement(weightIndexArray[vertexIndex][linkIndex]);
						}
						vElement.ContentLine("");
					}

		//				</v>
				}
		//			</vertex_weights>
			}

		//		</skin>
		//	</controller>
		//</library_controllers>
		}

		// Write out the morph controllers.
		for (int controllerIndex = 0, controllerCount = int(morphControllers.size()); controllerIndex < controllerCount; ++controllerIndex)
		{
			MorphControllerEntry& controller = morphControllers[controllerIndex];

			// Find the geometry and morphs for the model.
			std::map<std::pair<int, int>, int>::const_iterator modelGeometryMapPos = modelGeometryMap.find(std::make_pair(controller.geometryFileIndex, controller.modelIndex));
			MorphDataMap::const_iterator modelMorphDataPos = morphData.find(std::make_pair(controller.geometryFileIndex, controller.modelIndex));

			if (modelGeometryMapPos != modelGeometryMap.end() && modelMorphDataPos != morphData.end())
			{
				GeometryEntry& geometry = geometries[(*modelGeometryMapPos).second];
				const MorphData& modelMorphData = (*modelMorphDataPos).second;

				XMLWriter::Element controllerElement(writer, "controller");
				controllerElement.Attribute("id", controller.name);
				XMLWriter::Element morphElement(writer, "morph");
				morphElement.Attribute("source", "#" + geometry.name);

				std::string targetsSourceID = controller.name + "-source_targets";
				{
					XMLWriter::Element sourceElement(writer, "source");
					sourceElement.Attribute("id", targetsSourceID);
					std::string arrayID = targetsSourceID + "-array";
					{
						XMLWriter::Element idrefArrayElement(writer, "IDREF_array");
						idrefArrayElement.Attribute("id", arrayID);
						idrefArrayElement.Attribute("count", modelMorphData.GetMorphCount());
						for (int morphIndex = 0, morphCount = modelMorphData.GetMorphCount(); morphIndex < morphCount; ++morphIndex)
						{
							// Look up the geometry for this morph.
							//std::vector<MorphGeometryEntry>& morphGeometries
							std::map<std::pair<std::pair<int, int>, int>, int>::const_iterator morphGeometryMapPos = morphGeometryMap.find(std::make_pair(std::make_pair(controller.geometryFileIndex, controller.modelIndex), morphIndex));
							int morphGeometryIndex = (morphGeometryMapPos != morphGeometryMap.end() ? (*morphGeometryMapPos).second : -1);
							if (morphGeometryIndex >= -1)
							{
								const MorphGeometryEntry& morphGeometry = morphGeometries[morphGeometryIndex];
								idrefArrayElement.ContentArrayElement(morphGeometry.name);
							}
						}
					}
					{
						XMLWriter::Element techniqueCommonElement(writer, "technique_common");
						XMLWriter::Element accessorElement(writer, "accessor");
						accessorElement.Attribute("source", "#" + arrayID);
						accessorElement.Attribute("count", modelMorphData.GetMorphCount());
						accessorElement.Attribute("offset", 0);
						accessorElement.Attribute("stride", 1);
						XMLWriter::Element paramElement(writer, "param");
						paramElement.Attribute("name", "MORPH_TARGET");
						paramElement.Attribute("type", "IDREF");
					}
				}
				std::string weightsSourceID = controller.name + "-source_weights";
				{
					XMLWriter::Element sourceElement(writer, "source");
					sourceElement.Attribute("id", weightsSourceID);
					std::string arrayID = weightsSourceID + "-array";
					{
						XMLWriter::Element floatArrayElement(writer, "float_array");
						floatArrayElement.Attribute("id", arrayID);
						floatArrayElement.Attribute("count", modelMorphData.GetMorphCount());
						for (int morphIndex = 0, morphCount = modelMorphData.GetMorphCount(); morphIndex < morphCount; ++morphIndex)
						{
							// Look up the geometry for this morph.
							//std::vector<MorphGeometryEntry>& morphGeometries
							std::map<std::pair<std::pair<int, int>, int>, int>::const_iterator morphGeometryMapPos = morphGeometryMap.find(std::make_pair(std::make_pair(controller.geometryFileIndex, controller.modelIndex), morphIndex));
							int morphGeometryIndex = (morphGeometryMapPos != morphGeometryMap.end() ? (*morphGeometryMapPos).second : -1);
							if (morphGeometryIndex >= -1)
							{
								floatArrayElement.ContentArrayElement(0);
							}
						}
					}
					{
						XMLWriter::Element techniqueCommonElement(writer, "technique_common");
						XMLWriter::Element accessorElement(writer, "accessor");
						accessorElement.Attribute("source", "#" + arrayID);
						accessorElement.Attribute("count", modelMorphData.GetMorphCount());
						accessorElement.Attribute("offset", 0);
						accessorElement.Attribute("stride", 1);
						XMLWriter::Element paramElement(writer, "param");
						paramElement.Attribute("name", "MORPH_WEIGHT");
						paramElement.Attribute("type", "float");
					}
				}
				XMLWriter::Element targestElement(writer, "targets");
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "MORPH_TARGET");
					inputElement.Attribute("source", "#" + targetsSourceID);
				}
				{
					XMLWriter::Element inputElement(writer, "input");
					inputElement.Attribute("semantic", "MORPH_WEIGHT");
					inputElement.Attribute("source", "#" + weightsSourceID);
				}
			}
		}
	}

	void WriteImages(XMLWriter& writer, ProgressRange& progressRange)
	{
		XMLWriter::Element libraryImagesElement(writer, "library_images");
	}

	void WriteMaterials(XMLWriter& writer, MaterialData& materialData, std::map<int, int>& materialFXMap, std::vector<EffectsEntry>& effects, std::map<int, int>& materialMaterialMap, std::vector<MaterialEntry>& materials, ProgressRange& progressRange)
	{
		XMLWriter::Element libraryMaterialsElement(writer, "library_materials");

		for (int materialIndex = 0, materialCount = materialData.GetMaterialCount(); materialIndex < materialCount; ++materialIndex)
		{
			std::map<int, int>::iterator materialMapPos = materialMaterialMap.find(materialIndex);
			int entryIndex = (materialMapPos != materialMaterialMap.end() ? (*materialMapPos).second : -1);
			std::map<int, int>::iterator effectMapPos = materialFXMap.find(materialIndex);
			int effectIndex = (effectMapPos != materialFXMap.end() ? (*effectMapPos).second : -1);

			if (entryIndex >= 0)
			{
				std::string name = materials[entryIndex].name;
				XMLWriter::Element materialElement(writer, "material");
				materialElement.Attribute("id", name);
				if (effectIndex >= 0)
				{
					XMLWriter::Element effectElement(writer, "instance_effect");
					effectElement.Attribute("url", "#" + effects[effectIndex].name);
				}
			}
		}
	}

	void WriteScene(XMLWriter& writer, ProgressRange& progressRange)
	{
		XMLWriter::Element sceneElement(writer, "scene");
		XMLWriter::Element instanceElement(writer, "instance_visual_scene");
		instanceElement.Attribute("url", "#visual_scene_0");
	}
}

bool ColladaWriter::Write(IExportSource* source, IExportContext* context, IXMLSink* sink, ProgressRange& progressRange)
{
	if (FloatingPointHasPrecisionIssues())
	{
		// If you hit this point, please change floating point settings in your VS project (and recompile it):
		// ConfigurationProperties -> C/C++ -> CodeGeneration -> FloatingPointModel: "/fp:strict".
		// Note: using "/fp:precise" doesn't help.
		assert(0);
		context->Log(ILogger::eSeverity_Error, "Cannot write Collada file, because the writer has precision issues. Contact Crytek tools programmers.");
		return false;
	}

	// Temporarily change the current locale so that floats get written out using periods rather than commas.
	LocaleChanger localeChangeToStandard(LC_NUMERIC, "C");

	// Create an object to format the xml.
	XMLWriter writer(sink);

	// Export the animations to the file.
	{
		XMLWriter::Element colladaElement(writer, "COLLADA");
		colladaElement.Attribute("xmlns", "http://www.collada.org/2005/11/COLLADASchema");
		colladaElement.Attribute("version", "1.4.1");

		// Write out the document metadata.
		WriteMetaData(source,writer, ProgressRange(progressRange, 0.01f));

		// Read the skeleton.
		GeometryFileData geometryFileData;
		MaterialData materialData;
		std::vector<ModelData> modelData;
		SkeletonDataMap skeletonData;
		MorphDataMap morphData;
		{
			ProgressRange subProgressRange(progressRange, 0.1f);

			source->ReadGeometryFiles(context, &geometryFileData);

			bool ok = source->ReadMaterials(context, &geometryFileData, &materialData);
			if(!ok)
			{
				return false;
			}

			modelData.resize(geometryFileData.GetGeometryFileCount());

			for (int geometryFileIndex = 0, geometryFileCount = geometryFileData.GetGeometryFileCount(); geometryFileIndex < geometryFileCount; ++geometryFileIndex)
			{
				source->ReadModels(&geometryFileData, geometryFileIndex, &modelData[geometryFileIndex]);

				for (int modelIndex = 0, modelCount = modelData[geometryFileIndex].GetModelCount(); modelIndex < modelCount; ++modelIndex)
				{
					MorphDataMap::iterator morphDataPos = morphData.insert(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), MorphData())).first;
					source->ReadMorphs(context, &(*morphDataPos).second, &modelData[geometryFileIndex], modelIndex);
					if ((*morphDataPos).second.GetMorphCount() == 0)
						morphData.erase(morphDataPos);

					SkeletonDataMap::iterator skeletonDataPos = skeletonData.insert(std::make_pair(std::make_pair(geometryFileIndex, modelIndex), SkeletonData())).first;
					if (!source->ReadSkeleton(&geometryFileData, geometryFileIndex, &modelData[geometryFileIndex], modelIndex, &materialData, &(*skeletonDataPos).second))
						skeletonData.erase(skeletonDataPos);

#if !defined(HACK_HACK_FORCE_PELVIS_TO_BE_BONE_1_BECAUSE_LOADING_CODE_EXPECTS_IT)
#  define HACK_HACK_FORCE_PELVIS_TO_BE_BONE_1_BECAUSE_LOADING_CODE_EXPECTS_IT 0
#endif //!defined(HACK_HACK_FORCE_PELVIS_TO_BE_BONE_1_BECAUSE_LOADING_CODE_EXPECTS_IT)

					skeletonDataPos = skeletonData.find(std::make_pair(geometryFileIndex, modelIndex));
					if (skeletonDataPos != skeletonData.end())
					{
						SkeletonData newData, &oldData = (*skeletonDataPos).second;
						int pelvisIndex = -1;
						for (int i = 0, count = oldData.GetBoneCount(); i < count; ++i)
							pelvisIndex = ((_stricmp(oldData.GetName(i).c_str(), "Bip01 Pelvis") == 0) ? i : pelvisIndex);
						if (pelvisIndex >= 0)
						{
							if( pelvisIndex != 1 )
							{
								context->Log( ILogger::eSeverity_Warning, "`Bip01 Pelvis` should be the second bone.");
							}
#if HACK_HACK_FORCE_PELVIS_TO_BE_BONE_1_BECAUSE_LOADING_CODE_EXPECTS_IT == 1
							std::vector<int> oldToNewMap(oldData.GetBoneCount());
							std::vector<int> newToOldMap(oldData.GetBoneCount());
							for (int i = 0, count = oldData.GetBoneCount(); i < count; ++i)
								oldToNewMap[i] = i, newToOldMap[i] = i;
							std::swap(oldToNewMap[pelvisIndex], oldToNewMap[1]);
							std::swap(newToOldMap[pelvisIndex], newToOldMap[1]);
							for (int i = 0, count = oldData.GetBoneCount(); i < count; ++i)
							{
								int oldIndex = newToOldMap[i];
								void* handle = oldData.GetBoneHandle(oldIndex);
								std::string name = oldData.GetName(oldIndex);
								int oldParentIndex = oldData.GetParentIndex(oldIndex);
								int parentIndex = (oldParentIndex >= 0 ? oldToNewMap[oldParentIndex] : -1);
								float translation[3], rotation[3], scale[3];
								oldData.GetTranslation(translation, oldIndex);
								oldData.GetRotation(rotation, oldIndex);
								oldData.GetScale(scale, oldIndex);

								int boneIndex = newData.AddBone(handle, name.c_str(), parentIndex);
								newData.SetTranslation(boneIndex, translation);
								newData.SetRotation(boneIndex, rotation);
								newData.SetScale(boneIndex, scale);

								newData.SetHasGeometry(boneIndex, oldData.HasGeometry(oldIndex));

								if (oldData.HasParentFrame(oldIndex))
								{
									float parentFrameTranslation[3], parentFrameRotation[3], parentFrameScale[3];
									oldData.GetParentFrameTranslation(oldIndex, parentFrameTranslation);
									oldData.GetParentFrameRotation(oldIndex, parentFrameRotation);
									oldData.GetParentFrameScale(oldIndex, parentFrameScale);
									newData.SetParentFrameTranslation(boneIndex, parentFrameTranslation);
									newData.SetParentFrameRotation(boneIndex, parentFrameRotation);
									newData.SetParentFrameScale(boneIndex, parentFrameScale);
								}
								for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
								{
									ISkeletonData::Axis axis = ISkeletonData::Axis(axisIndex);
									if (oldData.HasLimit(oldIndex, axis, ISkeletonData::LimitMin))
										newData.SetLimit(boneIndex, axis, ISkeletonData::LimitMin, oldData.GetLimit(oldIndex, axis, ISkeletonData::LimitMin));
									if (oldData.HasLimit(oldIndex, axis, ISkeletonData::LimitMax))
										newData.SetLimit(boneIndex, axis, ISkeletonData::LimitMax, oldData.GetLimit(oldIndex, axis, ISkeletonData::LimitMax));
									if (oldData.HasSpringTension(oldIndex, axis))
										newData.SetSpringTension(boneIndex, axis, oldData.GetSpringTension(oldIndex, axis));
									if (oldData.HasSpringAngle(oldIndex, axis))
										newData.SetSpringAngle(boneIndex, axis, oldData.GetSpringAngle(oldIndex, axis));
									if (oldData.HasAxisDamping(oldIndex, axis))
										newData.SetAxisDamping(boneIndex, axis, oldData.GetAxisDamping(oldIndex, axis));
									newData.SetPhysicalized(boneIndex, oldData.GetPhysicalized(oldIndex));
								}
							}
							(*skeletonDataPos).second = newData;
#endif //HACK_HACK_FORCE_PELVIS_TO_BE_BONE_1_BECAUSE_LOADING_CODE_EXPECTS_IT == 1
						}
					}
				}
			}
		}

		// Generate a list of fx to export.
		std::map<int, int> materialFXMap;
		std::vector<EffectsEntry> effects;
		GenerateEffectsList(context, materialFXMap, effects, materialData);

		// Generate a list of geometry to export.
		std::map<std::pair<int, int>, int> modelGeometryMap;
		std::vector<GeometryEntry> geometries;
		GenerateGeometryList(context, modelGeometryMap, geometries, geometryFileData, modelData);

		// Generate a list of bone geometries to export.
		std::map<std::pair<std::pair<int, int>, int>, int> boneGeometryMap;
		std::vector<BoneGeometryEntry> boneGeometries;
		GenerateBoneGeometryList(context, boneGeometryMap, boneGeometries, geometryFileData, modelData, skeletonData);

		// Generate a list of morph geometries to export.
		std::map<std::pair<std::pair<int, int>, int>, int> morphGeometryMap;
		std::vector<MorphGeometryEntry> morphGeometries;
		GenerateMorphGeometryList(context, morphGeometryMap, morphGeometries, geometryFileData, modelData, morphData);

		BoneDataMap boneDataMap;
		GenerateBoneList(context, boneDataMap, skeletonData, modelData);

		// Generate a list of animations to export.
		std::vector<AnimationEntry> animations;
		GenerateAnimationList(context, animations, geometryFileData, modelData, skeletonData, source, ProgressRange(progressRange, 0.025f));

		// Generate a list of morph controllers to export.
		std::vector<MorphControllerEntry> morphControllers;
		std::map<std::pair<int, int>, int> modelMorphControllerMap;
		GenerateMorphControllerList(context, morphControllers, modelMorphControllerMap, morphData, geometryFileData, modelData, modelGeometryMap, geometries, ProgressRange(progressRange, 0.0125f));

		// Generate a list of skin controllers to export.
		std::vector<SkinControllerEntry> controllers;
		std::map<std::pair<int, int>, int> modelControllerMap;
		GenerateSkinControllerList(context, controllers, modelControllerMap, skeletonData, geometryFileData, modelData, modelGeometryMap, geometries, ProgressRange(progressRange, 0.0125f));

		// Write out all the animations.
		WriteAnimationList(writer, animations, ProgressRange(progressRange, 0.025f));
		WriteAnimationData(context, writer, animations, geometryFileData, modelData, skeletonData, boneDataMap, source, ProgressRange(progressRange, 0.475f));

		// Write out all the effects.
		WriteEffects(writer, effects, ProgressRange(progressRange, 0.01f));

		// Write out the materials.
		std::map<int, int> materialMaterialMap;
		std::vector<MaterialEntry> materials;
		GenerateMaterialList(context, materialMaterialMap, materialFXMap, effects, materials, materialData);
		WriteMaterials(writer, materialData, materialFXMap, effects, materialMaterialMap, materials, ProgressRange(progressRange, 0.005f));

		// Write out all the geometries.
		bool ok = WriteGeometries(context, writer, geometries, geometryFileData, modelData, morphData, materialData, materials, materialMaterialMap, skeletonData, boneGeometries, boneGeometryMap, morphGeometryMap, morphGeometries, source, ProgressRange(progressRange, 0.2f));
		if(!ok)
		{
			return false;
		}

		// Write out all the controllers.
		WriteControllers(writer, context, source, controllers, morphControllers, modelMorphControllerMap, geometryFileData, modelData, skeletonData, morphData, morphGeometries, morphGeometryMap, geometries, modelGeometryMap, boneDataMap, ProgressRange(progressRange, 0.005f));

		// Write out the list of models.
		WriteHierarchy(writer, context, geometryFileData, materialData, materialMaterialMap, materials, modelData, skeletonData, modelGeometryMap, geometries, modelControllerMap, controllers, boneDataMap, boneGeometryMap, boneGeometries, modelMorphControllerMap, morphControllers, source, ProgressRange(progressRange, 0.1f));

		// Write out all the other libraries.
		WriteImages(writer, ProgressRange(progressRange, 0.01f));
		WriteScene(writer, ProgressRange(progressRange, 0.01f));
	}

	return true;
}
