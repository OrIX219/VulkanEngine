#include "VulkanEngine.h"

int main(int argc, char* argv[]) {
  Engine::VulkanEngine engine;
  engine.Init();
  engine.Run();
  engine.Cleanup();

  return 0;
}