#include "CVAR.h"

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Engine {

enum class CVarType : char { kInt, kFloat, kString };

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

class CVarSystemImpl : public CVarSystem {
 public:
  CVarParameter* GetCVar(StringHash hash) override final;

  int32_t* GetIntCVar(StringHash name) override final;
  float* GetFloatCVar(StringHash name) override final;
  const char* GetStringCVar(StringHash name) override final;

  void SetIntCVar(StringHash name, int32_t value) override final;
  void SetIntCVar(StringHash name, float value) override final;
  void SetIntCVar(StringHash name, const char* value) override final;

  CVarParameter* CreateIntCVar(const char* name, const char* description,
                               int32_t default_value,
                               int32_t current_value) override final;

  CVarParameter* CreateFloatCVar(const char* name, const char* description,
                                 float default_value,
                                 float current_value) override final;

  CVarParameter* CreateStringCVar(const char* name, const char* description,
                                  std::string default_value,
                                  std::string current_value) override final;

  constexpr static uint32_t kMaxIntCvars = 64;
  CVarArray<int32_t> int_cvars{kMaxIntCvars};

  constexpr static uint32_t kMaxFloatCvars = 64;
  CVarArray<float> float_cvars{kMaxFloatCvars};

  constexpr static uint32_t kMaxStringCvars = 64;
  CVarArray<std::string> string_cvars{kMaxStringCvars};

  template <typename T>
  CVarArray<T>* GetCVarArray();

  template <>
  CVarArray<int32_t>* GetCVarArray() {
    return &int_cvars;
  }

  template <>
  CVarArray<float>* GetCVarArray() {
    return &float_cvars;
  }

  template <>
  CVarArray<std::string>* GetCVarArray() {
    return &string_cvars;
  }

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

int32_t* CVarSystemImpl::GetIntCVar(StringHash name) {
  return GetCVarCurrent<int32_t>(name);
}

float* CVarSystemImpl::GetFloatCVar(StringHash name) {
  return GetCVarCurrent<float>(name);
}

const char* CVarSystemImpl::GetStringCVar(StringHash name) {
  return GetCVarCurrent<std::string>(name)->c_str();
}

void CVarSystemImpl::SetIntCVar(StringHash name, int32_t value) {
  SetCVarCurrent<int32_t>(name, value);
}

void CVarSystemImpl::SetIntCVar(StringHash name, float value) {
  SetCVarCurrent<float>(name, value);
}

void CVarSystemImpl::SetIntCVar(StringHash name, const char* value) {
  SetCVarCurrent<std::string>(name, value);
}

CVarParameter* CVarSystemImpl::CreateIntCVar(const char* name,
                                             const char* description,
                                             int32_t default_value,
                                             int32_t current_value) {
  CVarParameter* param = InitCVar(name, description);
  if (!param) return nullptr;

  param->type = CVarType::kInt;

  GetCVarArray<int32_t>()->Add(default_value, current_value, param);

  AddToEditor(param);

  return param;
}

CVarParameter* CVarSystemImpl::CreateFloatCVar(const char* name,
                                               const char* description,
                                               float default_value,
                                               float current_value) {
  CVarParameter* param = InitCVar(name, description);
  if (!param) return nullptr;

  param->type = CVarType::kFloat;

  GetCVarArray<float>()->Add(default_value, current_value, param);

  AddToEditor(param);

  return param;
}

CVarParameter* CVarSystemImpl::CreateStringCVar(const char* name,
                                                const char* description,
                                                std::string default_value,
                                                std::string current_value) {
  CVarParameter* param = InitCVar(name, description);
  if (!param) return nullptr;

  param->type = CVarType::kString;

  GetCVarArray<std::string>()->Add(default_value, current_value, param);

  AddToEditor(param);

  return param;
}

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

AutoCVar_Int::AutoCVar_Int(const char* name, const char* description,
                           int32_t default_value, CVarFlags flags) {
  CVarParameter* param = CVarSystem::Get()->CreateIntCVar(
      name, description, default_value, default_value);
  param->flags = flags;
  index = param->array_index;
}

int32_t AutoCVar_Int::Get() { return GetCVarCurrentByIndex<CVarType>(index); }

int32_t* AutoCVar_Int::GetPtr() {
  return GetCVarCurrentPtrByIndex<CVarType>(index);
}

void AutoCVar_Int::Set(int32_t value) {
  SetCVarCurrentByIndex<CVarType>(index, value);
}

AutoCVar_Float::AutoCVar_Float(const char* name, const char* description,
                               float default_value, CVarFlags flags) {
  CVarParameter* param = CVarSystem::Get()->CreateFloatCVar(
      name, description, default_value, default_value);
  param->flags = flags;
  index = param->array_index;
}

float AutoCVar_Float::Get() { return GetCVarCurrentByIndex<CVarType>(index); }

float* AutoCVar_Float::GetPtr() {
  return GetCVarCurrentPtrByIndex<CVarType>(index);
}

void AutoCVar_Float::Set(float value) {
  SetCVarCurrentByIndex<CVarType>(index, value);
}

AutoCVar_String::AutoCVar_String(const char* name, const char* description,
                                 const char* default_value, CVarFlags flags) {
  CVarParameter* param = CVarSystem::Get()->CreateStringCVar(
      name, description, default_value, default_value);
  param->flags = flags;
  index = param->array_index;
}

const char* AutoCVar_String::Get() {
  return GetCVarCurrentByIndex<CVarType>(index).c_str();
}

void AutoCVar_String::Set(std::string&& value) {
  SetCVarCurrentByIndex<CVarType>(index, value);
}

void CVarSystemImpl::AddToEditor(CVarParameter* param) {
  bool is_hidden = ((uint32_t)param->flags & (uint32_t)CVarFlags::kNoEdit);
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
    bool is_advanced = ((uint32_t)p->flags & (uint32_t)CVarFlags::kAdvanced);
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
  constexpr float kSlack = 50;
  constexpr float kEditorWidth = 100;

  float full_width = text_width + kSlack;

  ImVec2 start_pos = ImGui::GetCursorScreenPos();

  ImGui::Text(label);

  ImVec2 final_pos = {start_pos.x + full_width, start_pos.y};

  ImGui::SameLine();
  ImGui::SetCursorScreenPos(final_pos);

  ImGui::SetNextItemWidth(kEditorWidth);
}
void CVarSystemImpl::EditParameter(CVarParameter* p, float text_width) {
  static const ImVec4 kYellow = {1.f, 1.f, 0.f, 1.f};
  const bool readonly_flag =
      ((uint32_t)p->flags & (uint32_t)CVarFlags::kEditReadOnly);
  const bool checkbox_flag =
      ((uint32_t)p->flags & (uint32_t)CVarFlags::kEditCheckbox);
  const bool drag_flag =
      ((uint32_t)p->flags & (uint32_t)CVarFlags::kEditFloatDrag);

  switch (p->type) {
    case CVarType::kInt:
      if (readonly_flag) {
        ImGui::Text((p->name + ":").c_str());
        ImGui::SameLine();
        ImGui::TextColored(kYellow, "%i",
                           GetCVarArray<int32_t>()->GetCurrent(p->array_index));
      } else {
        if (checkbox_flag) {
          bool checkbox =
              GetCVarArray<int32_t>()->GetCurrent(p->array_index) != 0;
          Label(p->name.c_str(), text_width);

          ImGui::PushID(p->name.c_str());
          if (ImGui::Checkbox("", &checkbox)) {
            GetCVarArray<int32_t>()->SetCurrent(checkbox ? 1 : 0,
                                                p->array_index);
          }
          ImGui::PopID();
        } else {
          Label(p->name.c_str(), text_width);
          ImGui::PushID(p->name.c_str());
          ImGui::InputInt(
              "", GetCVarArray<int32_t>()->GetCurrentPtr(p->array_index));
          ImGui::PopID();
        }
      }
      break;
    case CVarType::kFloat:
      if (readonly_flag) {
        ImGui::Text((p->name + ":").c_str());
        ImGui::SameLine();
        ImGui::TextColored(kYellow, "%f",
                           GetCVarArray<float>()->GetCurrent(p->array_index));
      } else {
        Label(p->name.c_str(), text_width);
        ImGui::PushID(p->name.c_str());
        if (drag_flag) {
          ImGui::DragFloat("",
                           GetCVarArray<float>()->GetCurrentPtr(p->array_index),
                           0.01f, 0.f, 1.f);
        } else {
          ImGui::InputFloat(
              "", GetCVarArray<float>()->GetCurrentPtr(p->array_index), 0, 0,
              "%.3f");
        }
        ImGui::PopID();
      }
      break;
    case CVarType::kString:
      if (readonly_flag) {
        ImGui::PushID(p->name.c_str());
        ImGui::Text((p->name + ":").c_str());
        ImGui::SameLine();
        ImGui::TextColored(
            kYellow, "%s",
            GetCVarArray<std::string>()->GetCurrent(p->array_index).c_str());
        ImGui::PopID();
      } else {
        Label(p->name.c_str(), text_width);
        ImGui::InputText(
            "", GetCVarArray<std::string>()->GetCurrentPtr(p->array_index));
        ImGui::PopID();
      }
      break;
    default:
      break;
  }

  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(p->description.c_str());
}

}  // namespace Engine