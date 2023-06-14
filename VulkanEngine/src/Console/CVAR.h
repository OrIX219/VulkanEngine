#pragma once

#include "StringHash.h"
#include <unordered_map>

namespace Engine {

// Hate this workaround to use scoped enums as bitmask
namespace CVarFlagBits {
enum : uint32_t {
  kNone = 0,
  kNoEdit = 1 << 1,
  kEditReadOnly = 1 << 2,
  kAdvanced = 1 << 3,

  kEditCheckbox = 1 << 8,
  kEditFloatDrag = 1 << 9
};
}
using CVarFlags = uint32_t;

class CVarParameter;

#define REGISTER_CVAR(type)                                         \
virtual type* Get##type##CVar(StringHash name) = 0;                 \
virtual void Set##type##CVar(StringHash name, type value) = 0;      \
virtual CVarParameter* Create##type##CVar(const char* name,         \
                                          const char* description,  \
                                          type default_value,       \
                                          type current_value) = 0;

class CVarSystem {
 public:
  using Int = int32_t;
  using Float = float;
  using String = std::string;

  struct Vec4 {
    float x, y, z, w;
  };

  virtual CVarParameter* GetCVar(StringHash name) = 0;

  REGISTER_CVAR(Int)
  REGISTER_CVAR(Float)
  REGISTER_CVAR(String)
  REGISTER_CVAR(Vec4)

  virtual void DrawImguiEditor() = 0;

  static CVarSystem* Get();
};

template <typename T>
struct AutoCVar {
 protected:
  uint32_t index;
  using CVarType = T;
};

#define AUTO_CVAR(type)                                      \
struct AutoCVar_##type : AutoCVar<CVarSystem::type> {        \
  AutoCVar_##type(const char* name, const char* description, \
                  const CVarSystem::type& default_value,     \
                  CVarFlags flags = CVarFlagBits::kNone);    \
  CVarSystem::type Get();                                    \
  CVarSystem::type* GetPtr();                                \
  void Set(CVarSystem::type&& value);                        \
};

AUTO_CVAR(Int)
AUTO_CVAR(Float)
AUTO_CVAR(String)
AUTO_CVAR(Vec4)

}  // namespace Engine