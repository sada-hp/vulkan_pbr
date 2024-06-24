#include "pch.hpp"
#include "shapes.hpp"

void calculate_normals(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	for (int i = 0; i < indices.size(); i += 3)
	{
		Vertex& A = vertices[indices[i]];
		Vertex& B = vertices[indices[i + 1]];
		Vertex& C = vertices[indices[i + 2]];
		glm::vec3 contributingNormal = glm::cross(glm::vec3(B.position) - glm::vec3(A.position), glm::vec3(C.position) - glm::vec3(A.position));
		float area = glm::length(contributingNormal) / 2.f;
		glm::vec3 contributingNormal2 = glm::normalize(contributingNormal) * area;

		vertices[indices[i]].normal += contributingNormal;
		vertices[indices[i + 1]].normal += contributingNormal;
		vertices[indices[i + 2]].normal += contributingNormal;
	}

	for (int i = 0; i < vertices.size(); i++)
	{
		vertices[i].normal = glm::normalize(vertices[i].normal);
	}
}

void calculate_tangents(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float u_scale, float v_scale)
{
	for (int i = 0; i < indices.size(); i += 3)
	{
		Vertex& A = vertices[indices[i]];
		Vertex& B = vertices[indices[i + 1]];
		Vertex& C = vertices[indices[i + 2]];

		glm::vec2 uv0 = (A.uv - 0.5f) * u_scale + (0.5f * v_scale);
		glm::vec2 uv1 = (B.uv - 0.5f) * u_scale + (0.5f * v_scale);
		glm::vec2 uv2 = (C.uv - 0.5f) * u_scale + (0.5f * v_scale);

		glm::vec3 diff1;
		glm::vec3 diff2;
		glm::vec2 delta1;
		glm::vec2 delta2;

		diff1 = B.position - A.position;
		diff2 = C.position - A.position;
		delta1 = glm::vec2(uv1.x, 1.f - uv1.y) - glm::vec2(uv0.x, 1.f - uv0.y);
		delta2 = glm::vec2(uv2.x, 1.f - uv2.y) - glm::vec2(uv0.x, 1.f - uv0.y);

		float f = 1.0f / (delta1.x * delta2.y - delta1.y * delta2.x);
		glm::vec3 tangent;
		glm::vec3 bitangent;

		tangent.x = f * (delta2.y * diff1.x - delta1.y * diff2.x);
		tangent.y = f * (delta2.y * diff1.y - delta1.y * diff2.y);
		tangent.z = f * (delta2.y * diff1.z - delta1.y * diff2.z);

		vertices[indices[i]].tangent = tangent;
		vertices[indices[i + 1]].tangent = tangent;
		vertices[indices[i + 2]].tangent = tangent;
	}
}

TAuto<Mesh> GRShape::Cube::Generate(const RenderScope& Scope) const
{
	TVector<Vertex> vertices;
	TVector<uint32_t> indices;

	float dt = scale / float(edge_splits + 1);
	float h = 0.5 * scale;

	int row_span = 2 + edge_splits;

	//face 1
	uint32_t k1 = vertices.size();
	uint32_t k2 = k1 + row_span;
	for (float x = -h; x <= h; x += dt)
	{
		for (float z = -h; z <= h; z += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { float(x), h, float(z) };
			v.uv = { (x + h) / scale, (z + h) / scale };
			vertices.push_back(v);

			if (x < h && z < h)
			{
				indices.insert(indices.end(), { 
					k1, k1 + 1, k2,
					k1 + 1, k2 + 1, k2
				});
			}
		}
	}

	//face 2
	k1 = vertices.size();
	k2 = k1 + row_span;
	for (float x = -h; x <= h; x += dt)
	{
		for (float z = -h; z <= h; z += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { float(x), -h, float(z) };
			v.uv = { (x + h) / scale, (z + h) / scale };
			vertices.push_back(v);

			if (x < h && z < h)
			{
				indices.insert(indices.end(), {
					k1, k2, k1 + 1,
					k1 + 1, k2, k2 + 1
				});
			}
		}
	}

	//face 3
	k1 = vertices.size();
	k2 = k1 + row_span;
	for (float x = -h; x <= h; x += dt)
	{
		for (float y = -h; y <= h; y += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { float(x), float(y), h };
			v.uv = { (x + h) / scale, 1.f - (y + h) / scale };
			vertices.push_back(v);

			if (x < h && y < h)
			{
				indices.insert(indices.end(), {
					k1, k2, k1 + 1,
					k1 + 1, k2, k2 + 1
				});
			}
		}
	}

	//face 4
	k1 = vertices.size();
	k2 = k1 + row_span;
	for (float x = -h; x <= h; x += dt)
	{
		for (float y = -h; y <= h; y += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { float(x), float(y), -h };
			v.uv = { (x + h) / scale, (y + h) / scale };
			vertices.push_back(v);

			if (x < h && y < h)
			{
				indices.insert(indices.end(), {
					k1, k1 + 1, k2,
					k1 + 1, k2 + 1, k2
				});
			}
		}
	}

	//face 5
	k1 = vertices.size();
	k2 = k1 + row_span;
	for (float z = -h; z <= h; z += dt)
	{
		for (float y = -h; y <= h; y += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { h, float(y), float(z) };
			v.uv = { (z + h) / scale, (y + h) / scale };
			vertices.push_back(v);

			if (z < h && y < h)
			{
				indices.insert(indices.end(), {
					k1, k1 + 1, k2,
					k1 + 1, k2 + 1, k2
				});
			}
		}
	}

	//face 6
	k1 = vertices.size();
	k2 = k1 + row_span;
	for (float z = -h; z <= h; z += dt)
	{
		for (float y = -h; y <= h; y += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { -h, float(y), float(z) };
			v.uv = { (z + h) / scale, (y + h) / scale };
			vertices.push_back(v);

			if (z < h && y < h)
			{
				indices.insert(indices.end(), {
					k1, k2, k1 + 1,
					k1 + 1, k2, k2 + 1
				});
			}
		}
	}

	calculate_normals(vertices, indices);
	calculate_tangents(vertices, indices, 1.f, 1.f);

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}

TAuto<Mesh> GRShape::Plane::Generate(const RenderScope& Scope) const
{
	TVector<Vertex> vertices;
	TVector<uint32_t> indices;

	float dt = scale / float(edge_splits + 1);
	float h = 0.5 * scale;

	int row_span = 2 + edge_splits;

	uint32_t k1 = vertices.size();
	uint32_t k2 = k1 + row_span;
	for (float x = -h; x <= h; x += dt)
	{
		for (float z = -h; z <= h; z += dt, k1++, k2++)
		{
			Vertex v{};
			v.position = { float(x), 0.0, float(z) };
			v.uv = { (x + h) / scale, (z + h) / scale };
			vertices.push_back(v);

			if (x < h && z < h)
			{
				indices.insert(indices.end(), {
					k1 + 1, k2, k1,
					k2 + 1, k2, k1 + 1
				});
			}
		}
	}

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}

TAuto<Mesh> GRShape::Sphere::Generate(const RenderScope& Scope) const
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	float x, y, z, xy;
	float sectorStep = 2.0 * glm::pi<double>() / rings;
	float stackStep = glm::pi<double>() / slices;
	float sectorAngle, stackAngle;

	for (int i = 0; i <= slices; i++)
	{
		uint32_t k1 = i * (rings + 1);
		uint32_t k2 = k1 + rings + 1;
		stackAngle = glm::pi<double>() / 2 - i * stackStep;
		xy = radius * glm::cos(stackAngle);
		z = radius * glm::sin(stackAngle);

		for (int j = 0; j <= rings; j++, k1++, k2++)
		{
			sectorAngle = j * sectorStep;
			x = xy * glm::cos(sectorAngle);
			y = xy * glm::sin(sectorAngle);

			Vertex v{};
			v.position = glm::vec3(x, y, z);
			v.normal = glm::normalize(v.position);
			v.tangent = glm::normalize(glm::vec3(-glm::sin(sectorAngle), 0.0, glm::cos(sectorAngle)));
			v.uv = glm::vec2(0.5f + glm::atan(v.normal.z, v.normal.x) * glm::one_over_two_pi<float>(), 0.5f + glm::asin(v.normal.y) * glm::one_over_pi<float>());

			vertices.push_back(v);

			if (i != 0)
			{
				indices.insert(indices.end(), { 
					k1, k2, k1 + 1 
				});
			}
			if (i != (slices - 1))
			{
				indices.insert(indices.end(), { 
					k1 + 1, k2, k2 + 1 
				});
			}
		}
	}

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}