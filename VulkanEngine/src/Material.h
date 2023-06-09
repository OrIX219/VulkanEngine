#pragma once

#include "Pipeline.h"

namespace Renderer {

struct Material {
  VkDescriptorSet texture_set{VK_NULL_HANDLE};
  Pipeline pipeline;
};

}  // namespace Renderer