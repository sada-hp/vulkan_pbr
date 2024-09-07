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
	glm::mat4 ProjectionMatrix;
	glm::mat4 ProjectionMatrixInverse;
	glm::dvec4 CameraPositionFP64;
	glm::vec4 CameraPosition;
	glm::vec3 SunDirection;
	float Time;
	glm::vec2 Resolution;
};
/*
* !@brief Struct describing the coverage of volumetric clouds
*/
struct CloudLayerProfile
{
	bool operator==(CloudLayerProfile& other)
	{
		return Coverage == other.Coverage
			&& VerticalSpan == other.VerticalSpan
			&& Absorption == other.Absorption
			&& WindSpeed == other.WindSpeed;
	}

	float Coverage = 0.175;
	float VerticalSpan = 0.25;
	float Absorption = 0.025;
	float WindSpeed = 0.25;
};
/*
* !@brief Dummy to inherit from
*/
struct Texture
{

};