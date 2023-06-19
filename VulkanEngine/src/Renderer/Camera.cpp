#include "Camera.h"

#include <glm/gtx/transform.hpp>

namespace Renderer {

Camera::Camera()
    : pos_{0},
      speed_(10.f),
      sensetivity_(0.1f),
      up_{0.f, 1.f, 0.f},
      front_{0.f, 0.f, -1.f},
      yaw_{-90.f},
      pitch_{0.f} {}

glm::mat4 Camera::GetViewMat() const {
  return glm::lookAt(pos_, pos_ + front_, up_);
}

glm::mat4 Camera::GetProjMat(bool reverse) const {
  glm::mat4 projection;
  if (reverse) {
    projection =
        glm::perspective(glm::radians(90.f), 1600.f / 900.f, 1000.f, 0.1f);
  } else {
    projection =
        glm::perspective(glm::radians(90.f), 1600.f / 900.f, 0.1f, 1000.f);
  }
  projection[1][1] *= -1;
  return projection;
}

void Camera::ProcessKeyboard(Direction direction, float delta_time) {
  float velocity = speed_ * delta_time;
  switch (direction) {
    case Direction::kForward:
      pos_ += velocity * front_;
      break;
    case Direction::kBackward:
      pos_ -= velocity * front_;
      break;
    case Direction::kRight:
      pos_ += velocity * right_;
      break;
    case Direction::kLeft:
      pos_ -= velocity * right_;
      break;
    case Direction::kUp:
      pos_ += velocity * up_;
      break;
    case Direction::kDown:
      pos_ -= velocity * up_;
      break;
  }
}

void Camera::ProcessMouse(float delta_x, float delta_y) {
  delta_x *= sensetivity_;
  delta_y *= sensetivity_;

  yaw_ += delta_x;
  pitch_ += delta_y;

  if (pitch_ > 89.f)
    pitch_ = 89.f;
  else if (pitch_ < -89.f)
    pitch_ = -89.f;

  UpdateVectors();
}

void Camera::UpdateVectors() {
  glm::vec3 direction(1.f);
  direction.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
  direction.y = sin(glm::radians(pitch_));
  direction.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
  front_ = glm::normalize(direction);
  right_ = glm::normalize(glm::cross(front_, up_));
}

}  // namespace Renderer