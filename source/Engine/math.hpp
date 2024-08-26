#pragma once
#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

template<typename T>
using TAuto = std::unique_ptr<T>;

template<typename T>
using TShared = std::shared_ptr<T>;

template<typename T>
using TWeak = std::weak_ptr<T>;

template<typename T, size_t S>
using TArray = std::array<T, S>;

template<typename T>
using TVector = std::vector<T>;

using TMat4 = glm::mat4;
using TMat3 = glm::mat3;
using TMat2 = glm::mat2;

using TDMat4 = glm::dmat4;
using TDMat3 = glm::dmat3;
using TDMat2 = glm::dmat2;

using TVec4 = glm::vec4;
using TVec3 = glm::vec3;
using TVec2 = glm::vec2;

using TDVec4 = glm::dvec4;
using TDVec3 = glm::dvec3;
using TDVec2 = glm::dvec2;

using TUVec4 = glm::uvec4;
using TUVec3 = glm::uvec3;
using TUVec2 = glm::uvec2;

using TIVec4 = glm::ivec4;
using TIVec3 = glm::ivec3;
using TIVec2 = glm::ivec2;

using TQuat = glm::quat;

namespace GRMath
{

};