#pragma once
#include <string>
#include "core.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

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

		template<typename T>
		glm::vec<3, T> CartesianFromGeo(T longitude, T latitude, T height, T radius)
		{
			T lonRad = glm::radians(longitude);
			T latRad = glm::radians(latitude);

			T CosLon = glm::cos(lonRad);
			T SinLon = glm::sin(lonRad);
			T CosLat = glm::cos(latRad);
			T SinLat = glm::sin(latRad);

			glm::vec<3, T> n = glm::vec<3, T>(CosLat * CosLon, SinLat, CosLat * SinLon);
			glm::vec<3, T> k = n * static_cast<T>(radius) * static_cast<T>(radius);
			T gamma = glm::sqrt(k.x * n.x + k.y * n.y + k.z * n.z);

			return k / gamma + (n * height);
		}

		template<typename T>
		glm::vec<4, T> CartesianToGeo(glm::vec<3, T> offset, T radius)
		{
			glm::vec<4, T> Out = glm::vec<4, T>(0,0,0,radius);

			Out.x = glm::degrees(glm::atan(offset.z, offset.x));
			Out.y = glm::degrees(glm::asin(glm::clamp(0.0, 1.0, offset.y / radius)));
			Out.z = glm::length(offset) - radius;

			return Out;
		}

		template<typename T>
		glm::qua<T> OrientationFromNormal(glm::vec<3, T> normal)
		{
			return glm::rotation(glm::vec<3, T>(0, 1, 0), normal);
		}
	};
};