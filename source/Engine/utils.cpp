#include "pch.hpp"
#include "utils.hpp"
#include "Vulkan/mesh.hpp"
#include "glfw/glfw3.h"

#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

namespace GR
{
	void Utils::CalculateNormals(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices)
	{
		for (int i = 0; i < indices.size(); i += 3)
		{
			MeshVertex& A = vertices[indices[i]];
			MeshVertex& B = vertices[indices[i + 1]];
			MeshVertex& C = vertices[indices[i + 2]];
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

	void Utils::CalculateTangents(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices, float u_scale, float v_scale)
	{
		for (int i = 0; i < indices.size(); i += 3)
		{
			MeshVertex& A = vertices[indices[i]];
			MeshVertex& B = vertices[indices[i + 1]];
			MeshVertex& C = vertices[indices[i + 2]];

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

	void Utils::ConvertImage_ARMT(const std::string& Roughness, const std::string& Metallic, const std::string& Ambient, const std::string& Transmittance, const std::string& Target)
	{
		assert(Target != "");

		int wr = 0, hr = 0, wm = 0, hm = 0, wa = 0, ha = 0, wt = 0, ht = 0, c = 0;
		unsigned char* rm = nullptr;
		unsigned char* mm = nullptr;
		unsigned char* am = nullptr;
		unsigned char* tm = nullptr;

		bool use_r = Roughness != "";
		bool use_m = Metallic != "";
		bool use_a = Ambient != "";
		bool use_t = Transmittance != "";

		if (!use_r && !use_m && !use_a)
			return;

		if (use_r)
			rm = stbi_load(Roughness.c_str(), &wr, &hr, &c, 1);
		if (use_m)
			mm = stbi_load(Metallic.c_str(), &wm, &hm, &c, 1);
		if (use_a)
			am = stbi_load(Ambient.c_str(), &wa, &ha, &c, 1);
		if (use_t)
			tm = stbi_load(Transmittance.c_str(), &wt, &ht, &c, 1);

		if (!use_r)
		{
			wr = use_m ? wm : wa;
			hr = use_m ? hm : ha;
		}

		if (!use_m)
		{
			wm = use_r ? wr : wa;
			hm = use_r ? hr : ha;
		}

		if (!use_a)
		{
			wa = use_r ? wr : wm;
			ha = use_r ? hr : hm;
		}

		if (!use_t)
		{
			wt = use_r ? wr : wa;
			ht = use_r ? hr : ha;
		}

		assert(wr == wm && wm == wa && wa == wt && hr == hm && hm == ha && ha == ht);

		if (!(wr == wm && wm == wa && wa == wt && hr == hm && hm == ha && ha == ht) || use_r && !rm || use_m && !mm || use_a && !am || use_t && !tm)
		{
			if (rm)
				free(rm);
			if (mm)
				free(mm);
			if (am)
				free(am);
			if (tm)
				free(tm);

			return;
		}

		size_t j = 0;
		size_t is = wr * hr * 4;
		uint8_t* pixels = (uint8_t*)malloc(is);
		for (size_t i = 0; i < is; i += 4, j++)
		{
			pixels[i] = use_a ? am[j] : uint8_t(255);
			pixels[i + 1] = use_r ? rm[j] : uint8_t(255);
			pixels[i + 2] = use_m ? mm[j] : uint8_t(0);
			pixels[i + 3] = use_t ? tm[j] : uint8_t(255);
		}

		stbi_write_png(Target.c_str(), wr, hr, 4, pixels, wr * 4);
		free(pixels);
	}

	void Utils::ConvertImage_NormalHeight(const std::string& Normal, const std::string& Height, const std::string& Target)
	{
		assert(Target != "");

		int wn = 0, hn = 0, wh = 0, hh = 0, c = 0;
		unsigned char* nm = nullptr;
		unsigned char* hm = nullptr;

		bool use_n = Normal != "";
		bool use_h = Height != "";

		if (!use_n && !use_h)
			return;

		if (use_n)
			nm = stbi_load(Normal.c_str(), &wn, &hn, &c, 3);
		if (use_h)
			hm = stbi_load(Height.c_str(), &wh, &hh, &c, 1);

		if (!use_n)
		{
			wn = wh;
			hn = hh;
		}

		if (!use_h)
		{
			wh = wn;
			hh = hn;
		}

		assert(wn == wh && hn == hh);

		if (!(wn == wh && hn == hh) || use_n && !nm || use_h && !hm)
		{
			if (nm)
				free(nm);
			if (hm)
				free(hm);

			return;
		}

		size_t j = 0;
		size_t k = 0;
		size_t is = wn * hn * 4;
		uint8_t* pixels = (uint8_t*)malloc(is);
		for (size_t i = 0; i < is; i += 4, j += 3, ++k)
		{
			pixels[i] = use_n ? nm[j] : uint8_t(127);
			pixels[i + 1] = use_n ? nm[j + 1] : uint8_t(127);
			pixels[i + 2] = use_n ? nm[j + 2] : uint8_t(255);
			pixels[i + 3] = use_h ? hm[k] : uint8_t(255);
		}

		stbi_write_png(Target.c_str(), wn, hn, 4, pixels, wn * 4);
		free(pixels);
	}

	double Utils::GetTime()
	{
		return glfwGetTime();
	}
};