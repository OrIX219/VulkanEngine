#pragma once

#include "Material.h"
#include "Mesh.h"

#include <glm\mat4x4.hpp>

namespace Renderer {

class RenderObject {
 public:
  RenderObject() : model_mat_(1.f) {}
  RenderObject(Mesh* mesh, Material* material) : model_mat_(1.f) {
    Create(mesh, material);
  }

  void Create(Mesh* mesh, Material* material) { 
    mesh_ = mesh;
    material_ = material;
  }

  glm::mat4& ModelMatrix() { return model_mat_; }

  Mesh* GetMesh() { return mesh_; }
  Material* GetMaterial() { return material_; }

 private:
  Mesh* mesh_;
  Material* material_;

  glm::mat4 model_mat_;
};

}