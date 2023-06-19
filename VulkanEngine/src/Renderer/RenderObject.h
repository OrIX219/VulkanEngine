#pragma once

#include "MaterialSystem.h"
#include "Mesh.h"

#include <glm\mat4x4.hpp>

#include <limits>

namespace Renderer {

struct RenderObject {
 public:
  RenderObject() : model_mat(1.f) {}
  RenderObject(Mesh* mesh, Material* material) : model_mat(1.f) {
    Create(mesh, material);
  }

  void Create(Mesh* mesh, Material* material) { 
    this->mesh = mesh;
    this->material = material;
  }

  void RefreshRenderBounds() {
    const RenderBounds& original = mesh->GetBounds();
    if (!original.valid) return;

    std::array<glm::vec3, 8> verts;
    for (size_t i = 0; i < 8; ++i) verts[i] = original.origin;

    verts[0] += original.extents * glm::vec3(1, 1, 1);
    verts[1] += original.extents * glm::vec3(1, 1, -1);
    verts[2] += original.extents * glm::vec3(1, -1, 1);
    verts[3] += original.extents * glm::vec3(1, -1, -1);
    verts[4] += original.extents * glm::vec3(-1, 1, 1);
    verts[5] += original.extents * glm::vec3(-1, 1, -1);
    verts[6] += original.extents * glm::vec3(-1, -1, 1);
    verts[7] += original.extents * glm::vec3(-1, -1, -1);

    glm::vec3 min{std::numeric_limits<float>().max()};
    glm::vec3 max{std::numeric_limits<float>().min()};

    glm::mat4 transform = model_mat;

    for (size_t i = 0; i < 8; ++i) {
      verts[i] = transform * glm::vec4(verts[i], 1.f);
      min = glm::min(verts[i], min);
      max = glm::max(verts[i], max);
    }

    glm::vec3 extents = (max - min) / 2.f;
    glm::vec3 origin = min + extents;

    float max_scale = 0.f;
    max_scale = std::max(max_scale,
                         glm::length(glm::vec3(transform[0][0], transform[0][1],
                                               transform[0][2])));
    max_scale = std::max(max_scale,
                         glm::length(glm::vec3(transform[1][0], transform[1][1],
                                               transform[1][2])));
    max_scale = std::max(max_scale,
                         glm::length(glm::vec3(transform[2][0], transform[2][1],
                                               transform[2][2])));

    float radius = max_scale * original.radius;

    bounds.extents = extents;
    bounds.radius = radius;
    bounds.origin = origin;
    bounds.valid = true;
  }

  Mesh* mesh;
  Material* material;

  glm::mat4 model_mat;

  RenderBounds bounds;

  bool draw_forward_pass;
  bool draw_shadow_pass;
};

}