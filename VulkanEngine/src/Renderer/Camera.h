#pragma once

#include <glm/glm.hpp>

namespace Renderer {

class Camera {
 public:
  enum class Direction { kForward, kBackward, kRight, kLeft, kUp, kDown };

  Camera();

  const glm::mat4& GetViewMat() const;
  const glm::mat4& GetProjMat(bool reverse = false) const;
  const glm::vec3& GetPosition() const;

  void ProcessKeyboard(Direction direction, float delta_time);
  void ProcessMouse(float delta_x, float delta_y);

 private:
  glm::vec3 pos_;
  glm::vec3 up_;
  glm::vec3 front_;
  glm::vec3 right_;

  float yaw_;
  float pitch_;

  float speed_;
  float sensetivity_;

  void UpdateVectors();
};

}