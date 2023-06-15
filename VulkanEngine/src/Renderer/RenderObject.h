#pragma once

#include "MaterialSystem.h"
#include "Mesh.h"

#include <glm\mat4x4.hpp>

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

  Mesh* mesh;
  Material* material;

  glm::mat4 model_mat;

  bool draw_forward_pass;
  bool draw_shadow_pass;
};

}