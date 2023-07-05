#include "Light.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/gtx/transform.hpp>

namespace Renderer {

void DirectionalLight::SetPosition(const glm::vec3& pos) { position = pos; }

void DirectionalLight::SetPosition(glm::vec3&& pos) {
  position = std::move(pos);
}

void DirectionalLight::SetDirection(const glm::vec3& dir) {
  direction = dir;
  if (abs(direction.x) < 0.01f && abs(direction.z) < 0.01f) direction.z = 0.01f;
}

void DirectionalLight::SetDirection(glm::vec3&& dir) {
  direction = std::move(dir);
}

void DirectionalLight::SetColor(const glm::vec4& col) { color = col; }

void DirectionalLight::SetColor(glm::vec4&& col) { color = std::move(col); }

void DirectionalLight::SetShadowExtent(const glm::vec3& extent) {
  shadow_extent = extent;
}

void DirectionalLight::SetShadowExtent(glm::vec3&& extent) {
  shadow_extent = std::move(extent);
}

glm::mat4 DirectionalLight::GetView() const {
  return glm::lookAt(position, position + direction, glm::vec3(0, 1, 0));
}

glm::mat4 DirectionalLight::GetProjection() const {
  return glm::ortho(-shadow_extent.x, shadow_extent.x, -shadow_extent.y,
                    shadow_extent.y, -shadow_extent.z * 3, shadow_extent.z);
}

GPUDirectionalLight DirectionalLight::GetUniform() const {
  return {glm::vec4(direction, 1.f), color, GetView(), GetProjection()};
}

PointLight::PointLight()
    : position(0),
      color(1),
      constant(1),
      linear(0.7f),
      quadratic(1.8f) {}

PointLight::PointLight(glm::vec4 color, float constant, float linear,
                       float quadratic)
    : position(0),
      color(color),
      constant(constant),
      linear(linear),
      quadratic(quadratic) {}

void PointLight::SetPosition(const glm::vec3& pos) { position = pos; }

void PointLight::SetPosition(glm::vec3&& pos) { position = std::move(pos); }

void PointLight::SetColor(const glm::vec4& col) { color = col; }

void PointLight::SetColor(glm::vec4&& col) { color = std::move(col); }

void PointLight::SetConstant(float value) { constant = value; }

void PointLight::SetLinear(float value) { linear = value; }

void PointLight::SetQuadratic(float value) { quadratic = value; }

GPUPointLight PointLight::GetUniform() const {
  return {position, color, constant, linear, quadratic};
}

}  // namespace Renderer