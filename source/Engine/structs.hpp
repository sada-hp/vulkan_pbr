#pragma once
#include "glm/glm.hpp"
/*
* !@brief Struct describing settings for initial application launch
*/
struct ApplicationSettings
{
	std::string ApplicationName;
	glm::ivec2 WindowExtents;
};
/*
* !@brief General per-frame values for rendering
*/
struct UniformBuffer
{
	glm::dmat4 ViewProjection;
	glm::dmat4 ViewMatrix;
	glm::dmat4 ViewMatrixInverse;
	glm::dmat4 ViewProjectionMatrixInverse;
	glm::dvec4 CameraPositionFP64;
	glm::mat4 PlanetMatrix;
	glm::mat4 ProjectionMatrix;
	glm::mat4 ProjectionMatrixInverse;
	glm::vec4 CameraPosition;
	glm::vec4 SunDirection;
	glm::vec4 WorldUp;
	glm::vec4 CameraUp;
	glm::vec4 CameraRight;
	glm::vec4 CameraForward; 
	glm::vec2 Resolution;
	double CameraRadius;
	float Wind;
	float Time;
	float Gamma;
	float Exposure;
};
/*
* !@brief Struct describing the coverage of volumetric clouds
*/
struct CloudLayerProfile
{
	bool operator==(CloudLayerProfile& other)
	{
		return  memcmp(this, &other, sizeof(CloudLayerProfile)) == 0;
	}

	float Coverage = 0.5;
	float Density = 0.006;
};
/*
* 
*/
struct TerrainLayerProfile
{
	bool operator==(TerrainLayerProfile& other)
	{
		return memcmp(this, &other, sizeof(TerrainLayerProfile)) == 0;
	}

	float AltitudeF = 0.1;
	float SlopeF = 0.375;
	float ConcavityF = 0.5;
	int Octaves = 15;
	
	float Sharpness = 0.0;
	int Op = 0;
	float Frequency = 500.0;
	float Offset;
};
/*
* !@brief Dummy to inherit from
*/
struct Texture
{

};