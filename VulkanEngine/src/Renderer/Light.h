#pragma once

#include <glm/glm.hpp>

namespace Renderer {

struct GPUDirectionalLight {
  glm::vec4 direction;
  glm::vec4 color;
  glm::mat4 view;
  glm::mat4 projection;
};

struct DirectionalLight {
  glm::vec3 position;
  glm::vec3 direction;
  glm::vec4 color;
  glm::vec3 shadow_extent;

  void SetPosition(const glm::vec3& pos);
  void SetPosition(glm::vec3&& pos);
  void SetDirection(const glm::vec3& dir);
  void SetDirection(glm::vec3&& dir);
  void SetColor(const glm::vec4& col);
  void SetColor(glm::vec4&& col);
  void SetShadowExtent(const glm::vec3& extent);
  void SetShadowExtent(glm::vec3&& extent);

  glm::mat4 GetView() const;
  glm::mat4 GetProjection() const;

  GPUDirectionalLight GetUniform() const;
};

struct GPUPointLight {
  alignas(16) glm::vec3 position;
  alignas(16) glm::vec4 color;
  float constant;
  float linear;
  float quadratic;
};

struct PointLight {
  PointLight();
  PointLight(glm::vec4 color, float constant, float linear, float quadratic);

  glm::vec3 position;
  glm::vec4 color;

  float constant;
  float linear;
  float quadratic;

  void SetPosition(const glm::vec3& pos);
  void SetPosition(glm::vec3&& pos);
  void SetColor(const glm::vec4& col);
  void SetColor(glm::vec4&& col);
  void SetConstant(float value);
  void SetLinear(float value);
  void SetQuadratic(float value);

  GPUPointLight GetUniform() const;
};

}