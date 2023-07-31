#pragma once

#include <deque>
#include <functional>

namespace Engine {

class DeletionQueue {
 public:
  template<typename Func>
  void PushFunction(Func&& func) {
    deletors_.push_back(func);
  }

  template<typename T>
  void PushPointer(T* pointer) {
    deletors_.push_back([pointer]() { delete pointer; });
  }

  void Flush() {
    for (auto iter = deletors_.rbegin(); iter != deletors_.rend(); ++iter) {
      (*iter)();
    }

    deletors_.clear();
  }

 private:
  std::deque<std::function<void()>> deletors_;
};

}