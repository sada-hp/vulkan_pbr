#pragma once
#include "core.hpp"
#include <string>

struct MeshVertex;

namespace GR
{
	namespace Utils
	{
		/*
		*
		*/
		GRAPI void CalculateNormals(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices);
		/*
		*
		*/
		GRAPI void CalculateTangents(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices, float u_scale, float v_scale);
		/*
		* !@brief Convert roughness, metallic, ao, transmittance maps to more tightly packed ARM image
		*
		* @param[in] Roughness - local path to roughness map image file
		* @param[in] Metallic - local path to metallic map image file
		* @param[in] Ambient - local path to ambient (ao) map image file
		* @param[in] Target - local path to output file (create/override), should use .png format
		*/
		GRAPI void ConvertImage_ARMT(const std::string& Roughness, const std::string& Metallic, const std::string& Ambient, const std::string& Transmittance, const std::string& Target);
		/*
		* !@brief Convert normal, height maps to more tightly packed NH image
		*
		* @param[in] Normal - local path to normal map file
		* @param[in] Height - local path to height map file
		* @param[in] Target - local path to output file (create/override), should use .png format
		*/
		GRAPI void ConvertImage_NormalHeight(const std::string& Normal, const std::string& Height, const std::string& Target);

		GRAPI double GetTime();
	};
};