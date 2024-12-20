#include "pch.hpp"
#include "shapes.hpp"
#include "utils.hpp"	
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace GR
{
	std::unique_ptr<VulkanMesh> Shapes::Cube::Generate(const RenderScope& Scope) const
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;

		float dt = m_Scale / float(m_EdgeSplits + 1);
		float h = 0.5 * m_Scale;

		int row_span = 2 + m_EdgeSplits;

		//face 1
		uint32_t k1 = vertices.size();
		uint32_t k2 = k1 + row_span;
		for (float x = -h; x <= h; x += dt)
		{
			for (float z = -h; z <= h; z += dt, k1++, k2++)
			{
				MeshVertex v{};
				v.position = { float(x), h, float(z) };
				v.uv = { (x + h) / m_Scale, 1.0 - (z + h) / m_Scale };
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
				MeshVertex v{};
				v.position = { float(x), -h, float(z) };
				v.uv = { (x + h) / m_Scale, (z + h) / m_Scale };
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
				MeshVertex v{};
				v.position = { float(x), float(y), h };
				v.uv = { (x + h) / m_Scale, (y + h) / m_Scale };
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
				MeshVertex v{};
				v.position = { float(x), float(y), -h };
				v.uv = { (x + h) / m_Scale, 1.0 - (y + h) / m_Scale };
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
				MeshVertex v{};
				v.position = { h, float(y), float(z) };
				v.uv = { (z + h) / m_Scale, 1.0 - (y + h) / m_Scale };
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
				MeshVertex v{};
				v.position = { -h, float(y), float(z) };
				v.uv = { (z + h) / m_Scale, (y + h) / m_Scale };
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

		Utils::CalculateNormals(vertices, indices);
		Utils::CalculateTangents(vertices, indices, 1.f, 1.f);

		return std::make_unique<VulkanMesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
	}

	std::unique_ptr<VulkanMesh> Shapes::Plane::Generate(const RenderScope& Scope) const
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;

		float dt = m_Scale / float(m_EdgeSplits + 1);
		float h = 0.5 * m_Scale;

		int row_span = 2 + m_EdgeSplits;

		uint32_t k1 = vertices.size();
		uint32_t k2 = k1 + row_span;
		for (float x = -h; x <= h; x += dt)
		{
			for (float z = -h; z <= h; z += dt, k1++, k2++)
			{
				MeshVertex v{};
				v.position = { float(x), 0.0, float(z) };
				v.uv = { (x + h) / m_Scale, 1.0 - (z + h) / m_Scale };
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

		Utils::CalculateNormals(vertices, indices);
		Utils::CalculateTangents(vertices, indices, 1.f, 1.f);

		return std::make_unique<VulkanMesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
	}

	std::unique_ptr<VulkanMesh> Shapes::Sphere::Generate(const RenderScope& Scope) const
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;

		MeshVertex top;
		top.position = glm::vec3(0.0, 0.0, m_Radius);
		top.normal = glm::vec3(0.0, 0.0, 1.0);
		top.uv = glm::vec2(0.5f + glm::atan(top.normal.z, top.normal.x) * glm::one_over_two_pi<float>(), 1.0 - (0.5f + glm::asin(top.normal.y) * glm::one_over_pi<float>()));
		vertices.push_back(top);

		for (uint32_t i = 1; i < m_Rings; i++)
		{
			double phi = glm::pi<float>() * float(i) / float(m_Rings);
			double cosphi = glm::cos(phi);
			double sinphi = glm::sin(phi);

			for (uint32_t j = 0; j < m_Slices; j++)
			{
				double theta = glm::two_pi<float>() * float(j) / float(m_Slices);
				double cosTheta = glm::cos(theta);
				double sinTheta = glm::sin(theta);

				MeshVertex v;
				v.position = m_Radius * glm::vec3(cosTheta * sinphi, sinTheta * sinphi, cosphi);
				v.normal = glm::normalize(v.position);
				v.uv = glm::vec2(0.5f + glm::atan(v.normal.z, v.normal.x) * glm::one_over_two_pi<float>(), 1.0 - (0.5f + glm::asin(v.normal.y) * glm::one_over_pi<float>()));
				vertices.push_back(v);
			}
		}

		MeshVertex bottom;
		bottom.position = glm::vec3(0.0, 0.0, -m_Radius);
		bottom.normal = glm::vec3(0.0, 0.0, -1.0);
		bottom.uv = glm::vec2(0.5f + glm::atan(bottom.normal.z, bottom.normal.x) * glm::one_over_two_pi<float>(), 1.0 - (0.5f + glm::asin(bottom.normal.y) * glm::one_over_pi<float>()));
		vertices.push_back(bottom);

		for (uint32_t j = 1; j < m_Slices; ++j)
		{
			indices.insert(indices.end(), { 0, j, j + 1 });
		}
		indices.insert(indices.end(), { 0, m_Slices, 1 });

		for (uint32_t i = 0; i < m_Rings - 2; ++i)
		{
			uint32_t topRowOffset = (i * m_Slices) + 1;
			uint32_t bottomRowOffset = ((i + 1) * m_Slices) + 1;

			for (uint32_t j = 0; j < m_Slices - 1; ++j)
			{
				indices.insert(indices.end(), { bottomRowOffset + j, bottomRowOffset + j + 1, topRowOffset + j + 1 });
				indices.insert(indices.end(), { bottomRowOffset + j, topRowOffset + j + 1, topRowOffset + j });
			}
			indices.insert(indices.end(), { bottomRowOffset + m_Slices - 1, bottomRowOffset, topRowOffset });
			indices.insert(indices.end(), { bottomRowOffset + m_Slices - 1, topRowOffset, topRowOffset + m_Slices - 1 });
		}

		uint32_t lastPosition = vertices.size() - 1;
		for (uint32_t j = lastPosition - 1; j > lastPosition - m_Slices; --j)
		{
			indices.insert(indices.end(), { lastPosition, j, j - 1 });
		}
		indices.insert(indices.end(), { lastPosition, lastPosition - m_Slices, lastPosition - 1 });

		Utils::CalculateNormals(vertices, indices);
		Utils::CalculateTangents(vertices, indices, 1.0, 1.0);

		return std::make_unique<VulkanMesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
	}

	std::unique_ptr<VulkanMesh> Shapes::Mesh::Generate(const RenderScope& Scope) const
	{
		std::unordered_map<MeshVertex, uint32_t> uniqueVertices{};
		std::vector<uint32_t> indices;
		std::vector<MeshVertex> vertices;
		Assimp::Importer importer;
		std::string file = path;
		const aiScene* model = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_FixInfacingNormals);
		auto err = importer.GetErrorString();

		assert(model != VK_NULL_HANDLE);

		if (!model)
			return VK_NULL_HANDLE;

		for (uint32_t submesh_ind = 0; submesh_ind < model->mNumMeshes; submesh_ind++)
		{
			auto num_vert = model->mMeshes[submesh_ind]->mNumVertices;
			auto cur_mesh = model->mMeshes[submesh_ind];
			auto name3 = model->mMeshes[submesh_ind]->mName;
			auto uv_ind = submesh_ind;

			for (int vert_ind = 0; vert_ind < num_vert; vert_ind++)
			{
				MeshVertex vertex{};
				vertex.position = { cur_mesh->mVertices[vert_ind].x, cur_mesh->mVertices[vert_ind].y, cur_mesh->mVertices[vert_ind].z };
				vertex.uv = { cur_mesh->mTextureCoords[uv_ind][vert_ind].x, 1.0 - cur_mesh->mTextureCoords[uv_ind][vert_ind].y };
				vertex.submesh = submesh_ind;

				if (cur_mesh->HasNormals())
					vertex.normal = { cur_mesh->mNormals[vert_ind].x, cur_mesh->mNormals[vert_ind].y, cur_mesh->mNormals[vert_ind].z };

				if (cur_mesh->HasTangentsAndBitangents())
					vertex.tangent = { cur_mesh->mTangents[vert_ind].x, cur_mesh->mTangents[vert_ind].y, cur_mesh->mTangents[vert_ind].z };

				if (uniqueVertices.count(vertex) == 0)
				{
					int index = uniqueVertices.size();
					uniqueVertices[vertex] = index;

					indices.push_back(index);
					vertices.push_back(std::move(vertex));
				}
				else
				{
					indices.push_back(uniqueVertices[vertex]);
				}
			}
		}

		return std::make_unique<VulkanMesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
	}

	std::unique_ptr<VulkanMesh> Shapes::GeoClipmap::Generate(const RenderScope& Scope) const
	{
		uint32_t m = (m_VerPerRing + 1) / 4;
		std::vector<uint32_t> indices{};
		std::vector<TerrainVertex> vertices{};

		std::unordered_map<glm::vec3, uint32_t> uniquePositions{};

		indices.reserve(m * m * 24u * m_Rings);
		vertices.reserve(m * m * 9u * m_Rings);

		float MaxRadius = float((1u << (m_Rings - 1u)) * (m / 2 + 1));
		for (uint32_t level = 0u; level < m_Rings; ++level)
		{
			int32_t step = (1u << level);
			int32_t prev_step = level > 0 ? (1u << (level - 1u)) : 0u;
			int32_t half_step = prev_step;
			int32_t g = m / 2;
			int32_t padding = 1u;
			int32_t radius = step * (g + padding);

			for (int32_t z = -radius; z < radius; z += step)
			{
				uint32_t A = 0u, B = 0u, C = 0u, D = 0u, E = 0u, F = 0u, G = 0u, H = 0u, I = 0u;
				for (int32_t x = -radius; x < radius; x += step)
				{
					if (glm::max(glm::abs(x + half_step), glm::abs(z + half_step)) >= g * prev_step)
					{
#if 0
						float L = x == -radius || x + step >= radius
							|| z == -radius || z + step >= radius
							? float(level + 1) : float(level);
#else
						float L = float(level);
#endif
						glm::vec3 Ap = glm::vec3(m_Scale * float(x), L, m_Scale * float(z));
						glm::vec3 Cp = glm::vec3(m_Scale * float(x + step), L, m_Scale * float(z));
						glm::vec3 Gp = glm::vec3(m_Scale * float(x), L, m_Scale * float(z + step));
						glm::vec3 Ip = glm::vec3(m_Scale * float(x + step), L, m_Scale * float(z + step));

						glm::vec3 Bp = (Ap + Cp) * 0.5f;
						glm::vec3 Dp = (Ap + Gp) * 0.5f;
						glm::vec3 Fp = (Cp + Ip) * 0.5f;
						glm::vec3 Hp = (Gp + Ip) * 0.5f;
						glm::vec3 Ep = (Ap + Ip) * 0.5f;

						if (!uniquePositions.contains(Ap * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Ap);
							uniquePositions[Ap * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						A = uniquePositions[Ap * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Bp * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Bp);
							uniquePositions[Bp * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						B = uniquePositions[Bp * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Cp * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Cp);
							uniquePositions[Cp * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						C = uniquePositions[Cp * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Dp * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Dp);
							uniquePositions[Dp * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						D = uniquePositions[Dp * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Ep * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Ep);
							uniquePositions[Ep * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						E = uniquePositions[Ep * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Fp * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Fp);
							uniquePositions[Fp * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						F = uniquePositions[Fp * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Gp * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Gp);
							uniquePositions[Gp * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						G = uniquePositions[Gp * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Hp * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Hp);
							H = vertices.size() - 1;
							uniquePositions[Hp * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						H = uniquePositions[Hp * glm::vec3(1, 0, 1)];

						if (!uniquePositions.contains(Ip * glm::vec3(1, 0, 1)))
						{
							vertices.emplace_back(Ip);
							uniquePositions[Ip * glm::vec3(1, 0, 1)] = vertices.size() - 1;
						}
						I = uniquePositions[Ip * glm::vec3(1, 0, 1)];

						if (x == -radius)
						{
							indices.insert(indices.end(), { E, A, G });
						}
						else
						{
							indices.insert(indices.end(), { E, A, D, E, D, G });
						}

						if (x + step >= radius)
						{
							indices.insert(indices.end(), { E, I, C });
						}
						else
						{
							indices.insert(indices.end(), { E, I, F, E, F, C });
						}

						if (z == -radius)
						{
							indices.insert(indices.end(), { E, C, A });
						}
						else
						{
							indices.insert(indices.end(), { E, C, B, E, B, A });
						}

						if (z + step >= radius)
						{
							indices.insert(indices.end(), { E, G, I });
						}
						else
						{
							indices.insert(indices.end(), { E, G, H, E, H, I });
						}
					}
					else
					{
						A = 0u, B = 0u, C = 0u, D = 0u, E = 0u, F = 0u, G = 0u, H = 0u, I = 0u;
					}
				}
			}
		}

		indices.shrink_to_fit();
		vertices.shrink_to_fit();

		return std::make_unique<VulkanMesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
	}
};