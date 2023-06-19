#include "CVAR.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Engine {

enum class CVarType : char {
  kInt,
  kUInt,
  kFloat,
  kString,
  kVec4,
  kVec3,
  kVec2
};

class CVarParameter {
 public:
  friend class CVarSystemImpl;

  int32_t array_index;

  CVarType type;
  CVarFlags flags;
  std::string name;
  std::string description;
};

template <typename T>
struct CVarStorage {
  T initial;
  T current;
  CVarParameter* parameter;
};

template <typename T>
struct CVarArray {
  CVarArray(size_t size) { cvars = new CVarStorage<T>[size](); }
  ~CVarArray() { delete[] cvars; }

  CVarStorage<T>* GetCurrentStorage(int32_t index) { return &cvars[index]; }

  T* GetCurrentPtr(int32_t index) { return &cvars[index].current; }

  T GetCurrent(int32_t index) { return cvars[index].current; }

  void SetCurrent(const T& value, int32_t index) {
    cvars[index].current = value;
  }

  int32_t Add(const T& value, CVarParameter* parameter) {
    int32_t index = last_cvar;

    cvars[index].current = value;
    cvars[index].initial = value;
    cvars[index].parameter = parameter;

    parameter->array_index = index;
    ++last_cvar;
    return index;
  }

  int32_t Add(const T& initial_value, const T& current_value,
              CVarParameter* parameter) {
    int32_t index = last_cvar;

    cvars[index].current = current_value;
    cvars[index].initial = initial_value;
    cvars[index].parameter = parameter;

    parameter->array_index = index;
    ++last_cvar;
    return index;
  }

  CVarStorage<T>* cvars{nullptr};
  int32_t last_cvar{0};
};

#define REGISTER_CVAR_IMPL(type, max_count)                         \
type* Get##type##CVar(StringHash name) override final;              \
void Set##type##CVar(StringHash name, type value) override final;   \
CVarParameter* Create##type##CVar(                                  \
    const char* name, const char* description, type default_value,  \
    type current_value) override final;                             \
constexpr static uint32_t kMax##type##Cvars = max_count;            \
CVarArray<type> type##_cvars{kMax##type##Cvars};                    \
template <>                                                         \
CVarArray<type>* GetCVarArray() { return &##type##_cvars; }

#define CVAR_IMPL(T)                                                  \
CVarSystem::T* CVarSystemImpl::Get##T##CVar(StringHash name) {        \
  return GetCVarCurrent<T>(name);                                     \
}                                                                     \
void CVarSystemImpl::Set##T##CVar(StringHash name, T value) {         \
    SetCVarCurrent<T>(name, value);                                   \
}                                                                     \
CVarParameter* CVarSystemImpl::Create##T##CVar(                       \
                                              const char* name,       \
                                              const char* description,\
                                              T default_value,        \
                                              T current_value) {      \
  CVarParameter* param = InitCVar(name, description);                 \
  if (!param) return nullptr;                                         \
  param->type = CVarType::k##T;                                       \
  GetCVarArray<T>()->Add(default_value, current_value, param);        \
  AddToEditor(param);                                                 \
  return param;                                                       \
}

class CVarSystemImpl : public CVarSystem {
 public:
  CVarParameter* GetCVar(StringHash hash) override final;

  template <typename T>
  CVarArray<T>* GetCVarArray();

  REGISTER_CVAR_IMPL(Int, 64)
  REGISTER_CVAR_IMPL(UInt, 64)
  REGISTER_CVAR_IMPL(Float, 64)
  REGISTER_CVAR_IMPL(String, 64)
  REGISTER_CVAR_IMPL(Vec4, 16)
  REGISTER_CVAR_IMPL(Vec3, 32)
  REGISTER_CVAR_IMPL(Vec2, 32)

  template <typename T>
  T* GetCVarCurrent(uint32_t name_hash) {
    CVarParameter* param = GetCVar(name_hash);
    if (!param) return nullptr;
    return GetCVarArray<T>()->GetCurrentPtr(param->array_index);
  }

  template <typename T>
  void SetCVarCurrent(uint32_t name_hash, const T& value) {
    CVarParameter* param = GetCVar(name_hash);
    if (param) GetCVarArray<T>()->SetCurrent(value, param->array_index);
  }

  static CVarSystemImpl* Get() {
    return static_cast<CVarSystemImpl*>(CVarSystem::Get());
  }

  void AddToEditor(CVarParameter* param);
  void DrawImguiEditor() override final;
  void EditParameter(CVarParameter* p, float text_width);

 private:
  CVarParameter* InitCVar(const char* name, const char* description);

  std::unordered_map<uint32_t, CVarParameter> saved_cvars_;

  std::vector<CVarParameter*> cached_edit_parameters_;
};

CVarSystem* CVarSystem::Get() {
  static CVarSystemImpl cvar_sys{};
  return &cvar_sys;
}

CVarParameter* CVarSystemImpl::GetCVar(StringHash name) {
  auto iter = saved_cvars_.find(name);
  if (iter != saved_cvars_.end()) return &iter->second;
  return nullptr;
}

CVAR_IMPL(Int)
CVAR_IMPL(UInt)
CVAR_IMPL(Float)
CVAR_IMPL(String)
CVAR_IMPL(Vec4)
CVAR_IMPL(Vec3)
CVAR_IMPL(Vec2)

CVarParameter* CVarSystemImpl::InitCVar(const char* name,
                                        const char* description) {
  if (GetCVar(name)) return nullptr;

  uint32_t name_hash = StringHash{name};
  saved_cvars_[name_hash] = CVarParameter{};

  CVarParameter& new_param = saved_cvars_[name_hash];

  new_param.name = name;
  new_param.description = description;

  return &new_param;
}

template <typename T>
T GetCVarCurrentByIndex(int32_t index) {
  return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrent(index);
}

template <typename T>
T* GetCVarCurrentPtrByIndex(int32_t index) {
  return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrentPtr(index);
}

template <typename T>
void SetCVarCurrentByIndex(int32_t index, const T& value) {
  CVarSystemImpl::Get()->GetCVarArray<T>()->SetCurrent(value, index);
}


#define AUTO_CVAR_IMLP(type)                                                              \
AutoCVar_##type::AutoCVar_##type(const char* name, const char* description,               \
                              const CVarSystem::type& default_value, CVarFlags flags) {   \
  CVarParameter* param = CVarSystem::Get()->Create##type##CVar(                           \
          name, description, default_value, default_value);                               \
  param->flags = flags;                                                                   \
  index = param->array_index;                                                             \
}                                                                                         \
CVarSystem::type AutoCVar_##type::Get() {                                                 \
  return GetCVarCurrentByIndex<CVarType>(index);                                          \
}                                                                                         \
CVarSystem::type* AutoCVar_##type::GetPtr() {                                             \
  return GetCVarCurrentPtrByIndex<CVarType>(index);                                       \
}                                                                                         \
void AutoCVar_##type::Set(CVarSystem::type&& value) {                                     \
  SetCVarCurrentByIndex<CVarType>(index, value);                                          \
}

AUTO_CVAR_IMLP(Int)
AUTO_CVAR_IMLP(UInt)
AUTO_CVAR_IMLP(Float)
AUTO_CVAR_IMLP(String)
AUTO_CVAR_IMLP(Vec4)
AUTO_CVAR_IMLP(Vec3)
AUTO_CVAR_IMLP(Vec2)

void CVarSystemImpl::AddToEditor(CVarParameter* param) {
  bool is_hidden = (param->flags & CVarFlagBits::kNoEdit);
  if (!is_hidden)
    cached_edit_parameters_.push_back(param);
}

void CVarSystemImpl::DrawImguiEditor() {
  static std::string search_text = "";
  ImGui::InputText("Filter", &search_text);
  static bool show_advanced = false;
  ImGui::Checkbox("Advanced", &show_advanced);
  ImGui::Separator();

  std::unordered_map<std::string,
                     std::pair<std::vector<CVarParameter*>, float>>
      categorized_params;

  // Insert all the edit parameters into the hashmap by category
  size_t categorized_counter = 0;
  for (auto p : cached_edit_parameters_) {
    bool is_advanced = (p->flags & CVarFlagBits::kAdvanced);
    if ((!show_advanced && is_advanced) ||
        p->name.find(search_text) == std::string::npos)
      continue;

    int dotPos = -1;
    // Find where the first dot is to categorize
    for (int i = 0; i < p->name.length(); i++) {
      if (p->name[i] == '.') {
        dotPos = i;
        break;
      }
    }

    std::string category = "";
    if (dotPos != -1) {
      category = p->name.substr(0, dotPos);
      ++categorized_counter;
    } else {
      category = "#" + std::to_string(categorized_counter);
    }

    auto it = categorized_params.find(category);
    if (it == categorized_params.end()) {
      categorized_params[category] = {std::vector<CVarParameter*>(), 0.f};
      it = categorized_params.find(category);
    }
    it->second.first.push_back(p);
    it->second.second =
        std::max(it->second.second, ImGui::CalcTextSize(p->name.c_str()).x);
  }

  for (auto [category, parameters] : categorized_params) {
    if (category[0] != '#') {
      if (ImGui::TreeNode(category.c_str())) {
        for (auto p : parameters.first)
          EditParameter(p, parameters.second);
        ImGui::TreePop();
      }
    } else {
      for (auto p : parameters.first)
        EditParameter(p, parameters.second);
    }
  }
}
void Label(const char* label, float text_width) {
  constexpr float kSlack = 20;

  float full_width = text_width + kSlack;

  ImVec2 start_pos = ImGui::GetCursorScreenPos();

  ImGui::Text(label);

  ImVec2 final_pos = {start_pos.x + full_width, start_pos.y};

  ImGui::SameLine();
  ImGui::SetCursorScreenPos(final_pos);
}
void CVarSystemImpl::EditParameter(CVarParameter* p, float text_width) {
  static const ImVec4 kYellow = {1.f, 1.f, 0.f, 1.f};
  const bool readonly_flag = (p->flags & CVarFlagBits::kEditReadOnly);
  const bool checkbox_flag = (p->flags & CVarFlagBits::kEditCheckbox);
  const bool drag_flag = (p->flags & CVarFlagBits::kEditFloatDrag);

  switch (p->type) {
    case CVarType::kInt:
      if (readonly_flag) {
        ImGui::Text((p->name + ":").c_str());
        ImGui::SameLine();
        ImGui::TextColored(kYellow, "%i",
                           GetCVarArray<Int>()->GetCurrent(p->array_index));
      } else {
        if (checkbox_flag) {
          bool checkbox =
              GetCVarArray<Int>()->GetCurrent(p->array_index) != 0;
          Label(p->name.c_str(), text_width);

          ImGui::PushID(p->name.c_str());
          if (ImGui::Checkbox("", &checkbox)) {
            GetCVarArray<Int>()->SetCurrent(checkbox ? 1 : 0,
                                                p->array_index);
          }
          ImGui::PopID();
        } else {
          Label(p->name.c_str(), text_width);
          ImGui::PushID(p->name.c_str());
          ImGui::InputInt(
              "", GetCVarArray<Int>()->GetCurrentPtr(p->array_index));
          ImGui::PopID();
        }
      }
      break;
    case CVarType::kFloat:
      if (readonly_flag) {
        ImGui::Text((p->name + ":").c_str());
        ImGui::SameLine();
        ImGui::TextColored(kYellow, "%f",
                           GetCVarArray<Float>()->GetCurrent(p->array_index));
      } else {
        Label(p->name.c_str(), text_width);
        ImGui::PushID(p->name.c_str());
        if (drag_flag) {
          float default_val =
              GetCVarArray<float>()->GetCurrentStorage(p->array_index)->initial;
          ImGui::DragFloat("",
                           GetCVarArray<Float>()->GetCurrentPtr(p->array_index),
                           1.f, 0.f, default_val * 2);
        } else {
          ImGui::InputFloat(
              "", GetCVarArray<Float>()->GetCurrentPtr(p->array_index), 0, 0,
              "%.3f");
        }
        ImGui::PopID();
      }
      break;
    case CVarType::kString:
      ImGui::PushID(p->name.c_str());
      if (readonly_flag) {
        ImGui::Text((p->name + ":").c_str());
        ImGui::SameLine();
        ImGui::TextColored(
            kYellow, "%s",
            GetCVarArray<String>()->GetCurrent(p->array_index).c_str());
      } else {
        Label(p->name.c_str(), text_width);
        ImGui::InputText(
            "", GetCVarArray<String>()->GetCurrentPtr(p->array_index));
      }
      ImGui::PopID();
      break;
    case CVarType::kVec4:
      if (readonly_flag) {
        Label(p->name.c_str(), text_width);
        ImGui::ColorEdit4(
            "vec4##",
            reinterpret_cast<float*>(
                GetCVarArray<Vec4>()->GetCurrentPtr(p->array_index)),
            ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs);
      } else {
        ImGui::PushID(p->name.c_str());
        Label(p->name.c_str(), text_width);
        ImGui::ColorEdit4(
            "", reinterpret_cast<float*>(
                    GetCVarArray<Vec4>()->GetCurrentPtr(p->array_index)));
        ImGui::PopID();
      }
      break;
    case CVarType::kVec3:
      if (readonly_flag) {
        Label(p->name.c_str(), text_width);
        ImGui::ColorEdit3(
            "vec3##",
            reinterpret_cast<float*>(
                GetCVarArray<Vec3>()->GetCurrentPtr(p->array_index)),
            ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs);
      } else {
        ImGui::PushID(p->name.c_str());
        Label(p->name.c_str(), text_width);
        ImGui::ColorEdit3(
            "", reinterpret_cast<float*>(
                    GetCVarArray<Vec3>()->GetCurrentPtr(p->array_index)));
        ImGui::PopID();
      }
      break;
    case CVarType::kVec2:
      ImGui::PushID(p->name.c_str());
      if (readonly_flag) {
        Label(p->name.c_str(), text_width);
        ImGui::InputFloat2(
            "",
            reinterpret_cast<float*>(
                GetCVarArray<Vec4>()->GetCurrentPtr(p->array_index)),
            "%.3f", ImGuiInputTextFlags_ReadOnly);
      } else {
        Label(p->name.c_str(), text_width);
        ImGui::DragFloat2(
            "",
            reinterpret_cast<float*>(
                GetCVarArray<Vec4>()->GetCurrentPtr(p->array_index)));
      }
      ImGui::PopID();
      break;
    default:
      break;
  }

  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(p->description.c_str());
}

}  // namespace Engine