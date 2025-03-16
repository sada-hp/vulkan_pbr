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
	glm::dvec4 CameraPositionFP64;
	glm::mat4 ProjectionMatrix;
	glm::mat4 ProjectionMatrixInverse;
	glm::vec4 CameraPosition;
	glm::vec4 SunDirection;
	glm::vec4 CameraUp;
	glm::vec4 CameraRight;
	glm::vec4 CameraForward;
	glm::vec2 Resolution;
	double CameraRadius;
	float Time;
};
/*
* !@brief Struct describing the coverage of volumetric clouds
*/
struct CloudLayerProfile
{
	bool operator==(CloudLayerProfile& other)
	{
		return Coverage == other.Coverage
			&& Density == other.Density
			&& WindSpeed == other.WindSpeed;
	}

	float Coverage = 0.5;
	float Density = 0.015;
	float WindSpeed = 0.25;
};
/*
* !@brief Dummy to inherit from
*/
struct Texture
{

};