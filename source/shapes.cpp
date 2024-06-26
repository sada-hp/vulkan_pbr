#include "pch.hpp"
#include "shapes.hpp"

extern void calculate_normals(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

extern void calculate_tangents(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float u_scale, float v_scale);

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
			v.uv = { (x + h) / scale, 1.0 - (z + h) / scale };
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
			v.uv = { (x + h) / scale, (y + h) / scale };
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
			v.uv = { (x + h) / scale, 1.0 - (y + h) / scale };
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
			v.uv = { (z + h) / scale, 1.0 - (y + h) / scale };
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
			v.uv = { (x + h) / scale, 1.0 - (z + h) / scale };
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

	calculate_normals(vertices, indices);
	calculate_tangents(vertices, indices, 1.f, 1.f);

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}

TAuto<Mesh> GRShape::Sphere::Generate(const RenderScope& Scope) const
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	Vertex top;
	top.position = glm::vec3(0.0, 0.0, radius);
	top.normal = glm::vec3(0.0, 0.0, 1.0);
	top.uv = glm::vec2(0.5f + glm::atan(top.normal.z, top.normal.x) * glm::one_over_two_pi<float>(), 1.0 - (0.5f + glm::asin(top.normal.y) * glm::one_over_pi<float>()));
	vertices.push_back(top);

	for (uint32_t i = 1; i < rings; i++)
	{
		double phi = glm::pi<float>() * float(i) / float(rings);
		double cosphi = glm::cos(phi);
		double sinphi = glm::sin(phi);

		for (uint32_t j = 0; j < slices; j++)
		{
			double theta = glm::two_pi<float>() * float(j) / float(slices);
			double cosTheta = glm::cos(theta);
			double sinTheta = glm::sin(theta);

			Vertex v;
			v.position = radius * glm::vec3(cosTheta * sinphi, sinTheta * sinphi, cosphi);
			v.normal = glm::normalize(v.position);
			v.uv = glm::vec2(0.5f + glm::atan(v.normal.z, v.normal.x) * glm::one_over_two_pi<float>(), 1.0 - (0.5f + glm::asin(v.normal.y) * glm::one_over_pi<float>()));
			vertices.push_back(v);
		}
	}

	Vertex bottom;
	bottom.position = glm::vec3(0.0, 0.0, -radius);
	bottom.normal = glm::vec3(0.0, 0.0, -1.0);
	bottom.uv = glm::vec2(0.5f + glm::atan(bottom.normal.z, bottom.normal.x) * glm::one_over_two_pi<float>(), 1.0 - (0.5f + glm::asin(bottom.normal.y) * glm::one_over_pi<float>()));
	vertices.push_back(bottom);

	for (uint32_t j = 1; j < slices; ++j)
	{
		indices.insert(indices.end(), { 0, j, j + 1 });
	}
	indices.insert(indices.end(), { 0, slices, 1 });

	for (uint32_t i = 0; i < rings - 2; ++i)
	{
		uint32_t topRowOffset = (i * slices) + 1;
		uint32_t bottomRowOffset = ((i + 1) * slices) + 1;

		for (uint32_t j = 0; j < slices - 1; ++j)
		{
			indices.insert(indices.end(), { bottomRowOffset + j, bottomRowOffset + j + 1, topRowOffset + j + 1 });
			indices.insert(indices.end(), { bottomRowOffset + j, topRowOffset + j + 1, topRowOffset + j });
		}
		indices.insert(indices.end(), { bottomRowOffset + slices - 1, bottomRowOffset, topRowOffset });
		indices.insert(indices.end(), { bottomRowOffset + slices - 1, topRowOffset, topRowOffset + slices - 1 });
	}

	uint32_t lastPosition = vertices.size() - 1;
	for (uint32_t j = lastPosition - 1; j > lastPosition - slices; --j)
	{
		indices.insert(indices.end(), { lastPosition, j, j - 1 });
	}
	indices.insert(indices.end(), { lastPosition, lastPosition - slices, lastPosition - 1 });

	calculate_normals(vertices, indices);
	calculate_tangents(vertices, indices, 1.0, 1.0);

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}