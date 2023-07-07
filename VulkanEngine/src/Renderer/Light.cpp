#include "Light.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/gtx/transform.hpp>

namespace Renderer {

Light::Light()
    : color{1.f, 1.f, 1.f, 1.f},
      ambient_factor(0.01f),
      diffuse_factor(1.f),
      specular_factor(1.f) {}

Light::Light(glm::vec4 color, float ambient_factor, float diffuse_factor,
             float specular_factor)
    : color(color),
      ambient_factor(ambient_factor),
      diffuse_factor(diffuse_factor),
      specular_factor(specular_factor) {}

void Light::SetColor(const glm::vec4& col) { color = col; }

void Light::SetColor(glm::vec4&& col) { color = std::move(col); }

void Light::SetAmbient(float ambient) { ambient_factor = ambient; }

void Light::SetDiffuse(float diffuse) { diffuse_factor = diffuse; }

void Light::SetSpecular(float specular) { specular_factor = specular; }

DirectionalLight::DirectionalLight()
    : position(0.f), direction(1.f), shadow_extent(32.f) {}

DirectionalLight::DirectionalLight(glm::vec4 color, glm::vec3 position,
                                   glm::vec3 direction, glm::vec3 shadow_extent)
    : Light(color),
      position(position),
      direction(direction),
      shadow_extent(shadow_extent) {}

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
  return {ambient_factor, diffuse_factor, specular_factor, direction,
          color,          GetView(),      GetProjection()};
}

PointLight::PointLight()
    : position(0),
      constant(1),
      linear(0.7f),
      quadratic(1.8f) {}

PointLight::PointLight(glm::vec4 color, float constant, float linear,
                       float quadratic)
    : Light(color),
      position(0),
      constant(constant),
      linear(linear),
      quadratic(quadratic) {}

void PointLight::SetPosition(const glm::vec3& pos) { position = pos; }

void PointLight::SetPosition(glm::vec3&& pos) { position = std::move(pos); }

void PointLight::SetConstant(float value) { constant = value; }

void PointLight::SetLinear(float value) { linear = value; }

void PointLight::SetQuadratic(float value) { quadratic = value; }

GPUPointLight PointLight::GetUniform() const {
  return {ambient_factor, diffuse_factor, specular_factor, position,
          color,          constant,       linear,          quadratic};
}

SpotLight::SpotLight()
    : position(0.f), direction(1.f), cut_off_inner(10.f), cut_off_outer(15.f) {}

SpotLight::SpotLight(glm::vec4 color, glm::vec3 position, glm::vec3 direction,
                     float cut_off_inner, float cut_off_outer)
    : Light(color),
      position(position),
      direction(direction),
      cut_off_inner(cut_off_inner),
      cut_off_outer(cut_off_outer) {}

void SpotLight::SetPosition(const glm::vec3& pos) { position = pos; }

void SpotLight::SetPosition(glm::vec3&& pos) { position = std::move(pos); }

void SpotLight::SetDirection(const glm::vec3& dir) {
  direction = dir;
  if (abs(direction.x) < 0.01f && abs(direction.z) < 0.01f) direction.z = 0.01f;
}

void SpotLight::SetDirection(glm::vec3&& dir) {
  direction = std::move(dir);
  if (abs(direction.x) < 0.01f && abs(direction.z) < 0.01f) direction.z = 0.01f;
}

void SpotLight::SetCutoff(float inner, float outer) {
  cut_off_inner = glm::min(inner, 90.f);
  cut_off_outer = glm::min(glm::max(outer, inner), 90.f);
}

GPUSpotLight SpotLight::GetUniform() const {
  return {ambient_factor, diffuse_factor, specular_factor, position,
          direction,      color,          cut_off_inner,   cut_off_outer};
}

}  // namespace Renderer