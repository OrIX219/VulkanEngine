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

class CVarSystem {
 public:
  virtual CVarParameter* GetCVar(StringHash name) = 0;

  virtual int32_t* GetIntCVar(StringHash name) = 0;
  virtual float* GetFloatCVar(StringHash name) = 0;
  virtual const char* GetStringCVar(StringHash name) = 0;

  virtual void SetIntCVar(StringHash name, int32_t value) = 0;
  virtual void SetIntCVar(StringHash name, float value) = 0;
  virtual void SetIntCVar(StringHash name, const char* value) = 0;

  virtual CVarParameter* CreateIntCVar(const char* name,
                                       const char* description,
                                       int32_t default_value,
                                       int32_t current_value) = 0;
  virtual CVarParameter* CreateFloatCVar(const char* name,
                                         const char* description,
                                         float default_value,
                                         float current_value) = 0;
  virtual CVarParameter* CreateStringCVar(const char* name,
                                          const char* description,
                                          std::string default_value,
                                          std::string current_value) = 0;

  virtual void DrawImguiEditor() = 0;

  static CVarSystem* Get();
};

template <typename T>
struct AutoCVar {
 protected:
  uint32_t index;
  using CVarType = T;
};

struct AutoCVar_Int : AutoCVar<int32_t> {
  AutoCVar_Int(const char* name, const char* description, int32_t default_value,
               CVarFlags flags = CVarFlagBits::kNone);

  int32_t Get();
  int32_t* GetPtr();
  void Set(int32_t value);
};

struct AutoCVar_Float : AutoCVar<float> {
  AutoCVar_Float(const char* name, const char* description, float default_value,
                 CVarFlags flags = CVarFlagBits::kNone);

  float Get();
  float* GetPtr();
  void Set(float value);
};

struct AutoCVar_String : AutoCVar<std::string> {
  AutoCVar_String(const char* name, const char* description,
                  const char* default_value,
                  CVarFlags flags = CVarFlagBits::kNone);

  const char* Get();
  void Set(std::string&& value);
};

}  // namespace Engine