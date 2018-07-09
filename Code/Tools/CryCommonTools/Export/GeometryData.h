// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GEOMETRYDATA_H__
#define __GEOMETRYDATA_H__

#include "IGeometryData.h"
#include <vector>

class GeometryData : public IGeometryData
{
public:
	GeometryData();

	// IGeometryData
	virtual int AddPosition(float x, float y, float z);
	virtual int AddNormal(float x, float y, float z);
	virtual int AddTextureCoordinate(float u, float v);
	virtual int AddVertexColor(float r, float g, float b, float a);

	virtual int AddPolygon(const int* indices, int mtlID);

	virtual int GetNumberOfPositions() const;
	virtual int GetNumberOfNormals() const;
	virtual int GetNumberOfTextureCoordinates() const;
	virtual int GetNumberOfVertexColors() const;

	virtual int GetNumberOfPolygons() const;

	struct Vector
	{
		Vector(float x, float y, float z): x(x), y(y), z(z) {}
		float x, y, z;
	};

	struct TextureCoordinate
	{
		TextureCoordinate(float u, float v): u(u), v(v) {}
		float u, v;
	};

	struct VertexColor
	{
		VertexColor(float r, float g, float b, float a): r(r), g(g), b(b), a(a) {}
		float r, g, b, a;
	};

	struct Polygon
	{
		struct Vertex
		{
			Vertex() {}
			Vertex(int positionIndex, int normalIndex, int textureCoordinateIndex, int vertexColorIndex)
				: positionIndex(positionIndex), normalIndex(normalIndex), textureCoordinateIndex(textureCoordinateIndex), vertexColorIndex(vertexColorIndex) {}
			int positionIndex, normalIndex, textureCoordinateIndex, vertexColorIndex;
		};

		Polygon(int mtlID, const Vertex& v0, const Vertex& v1, const Vertex& v2)
			: mtlID(mtlID)
		{
			v[0] = v0; v[1] = v1; v[2] = v2;
		}

		int mtlID;
		Vertex v[3];
	};

	std::vector<Vector> positions;
	std::vector<Vector> normals;
	std::vector<TextureCoordinate> textureCoordinates;
	std::vector<VertexColor> vertexColors;
	std::vector<Polygon> polygons;
};

#endif //__GEOMETRYDATA_H__
