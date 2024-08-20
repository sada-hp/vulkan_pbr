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
	glm::mat4 ViewProjection;
	glm::mat4 ProjectionMatrixInverse;
	glm::mat4 ViewMatrixInverse;
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
	float VerticalSpan = 0.2;
	float Absorption = 0.05;
	float WindSpeed = 0.25;
};
/*
* !@brief Dummy to inherit from
*/
struct Texture
{

};