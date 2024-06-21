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

struct UniformBuffer
{
	glm::mat4 ViewProjection;
	glm::mat4 ProjectionMatrixInverse;
	glm::mat4 ViewMatrixInverse;
	glm::vec3 SunDirection;
	float Time;
};

struct ViewBuffer
{
	glm::vec4 CameraPosition;
	glm::vec2 Resolution;
};

struct CloudProfileLayer
{
	float Coverage = 0.175;
	float VerticalSpan = 1.0;
	float Absorption = 2e-3;
	float PhaseCoefficient = 0.5;
	float WindSpeed = 0.5;
};

struct Image
{

};