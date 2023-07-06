#pragma once

#include <glm/glm.hpp>

namespace Renderer {

struct Light {
  glm::vec4 color;
  float ambient_factor;
  float diffuse_factor;
  float specular_factor;
  
  Light();
  Light(glm::vec4 color, float ambient_factor = 0.01f,
        float diffuse_factor = 1.f, float specular_factor = 1.f);

  void SetColor(const glm::vec4& col);
  void SetColor(glm::vec4&& col);
  void SetAmbient(float ambient);
  void SetDiffuse(float diffuse);
  void SetSpecular(float specular);
};

struct alignas(16) GPULight {
  float ambient_factor;
  float diffuse_factor;
  float specular_factor;
};

struct alignas(16) GPUDirectionalLight : public GPULight {
  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 color;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

struct DirectionalLight : public Light {
  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 shadow_extent;

  void SetPosition(const glm::vec3& pos);
  void SetPosition(glm::vec3&& pos);
  void SetDirection(const glm::vec3& dir);
  void SetDirection(glm::vec3&& dir);
  void SetShadowExtent(const glm::vec3& extent);
  void SetShadowExtent(glm::vec3&& extent);

  glm::mat4 GetView() const;
  glm::mat4 GetProjection() const;

  GPUDirectionalLight GetUniform() const;
};

struct alignas(16) GPUPointLight : public GPULight {
  alignas(16) glm::vec3 position;
  alignas(16) glm::vec4 color;
  float constant;
  float linear;
  float quadratic;
};

struct PointLight : public Light {
  PointLight();
  PointLight(glm::vec4 color, float constant, float linear, float quadratic);

  glm::vec3 position;

  float constant;
  float linear;
  float quadratic;

  void SetPosition(const glm::vec3& pos);
  void SetPosition(glm::vec3&& pos);
  void SetConstant(float value);
  void SetLinear(float value);
  void SetQuadratic(float value);

  GPUPointLight GetUniform() const;
};

struct alignas(16) GPUSpotLight : public GPULight {
  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 direction;
  alignas(16) glm::vec4 color;
  float cut_off_inner;
  float cut_off_outer;
};

struct SpotLight : public Light {
  glm::vec3 position;
  glm::vec3 direction;
  float cut_off_inner;
  float cut_off_outer;

  void SetPosition(const glm::vec3& pos);
  void SetPosition(glm::vec3&& pos);
  void SetDirection(const glm::vec3& dir);
  void SetDirection(glm::vec3&& dir);
  void SetCutoff(float inner, float outer);

  GPUSpotLight GetUniform() const;
};

}