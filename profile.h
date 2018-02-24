#pragma once

#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <time.h>

namespace engine {
namespace profile {

enum ActionType { Push = 0, Execute = 1, Finish = 2 };
enum OwnerType { Opr = 0, Var = 1 };

static const std::string owners = "ov";
static const std::string actions = "pef";

struct Action {
  explicit Action(const std::string &name) : owner(name) {}

  std::string owner;
  ActionType action_type{Push};
  OwnerType owner_type{Opr};
  size_t timestramp;

  std::string debug_string() const {
    std::stringstream ss;
    ss << owners[owner_type] << "\t" << owner << "\t" << timestramp << '\t'
       << actions[action_type] << '\n';
    return ss.str();
  }
};

// NOTE Profiler is not thread-safe.
class Profiler {
public:
  Action &AddAction(const std::string &owner, OwnerType owner_type,
                    ActionType action_type);

  Action &AddPushAction(const std::string &owner) {
    return AddAction(owner, Opr, Push);
  }
  Action &AddFinishAction(const std::string &owner) {
    return AddAction(owner, Opr, Finish);
  }

  void Clear() { queue_.clear(); }

  static Profiler *Get() {
    static std::unique_ptr<Profiler> p(new Profiler);
    return p.get();
  }

  std::string debug_string() const {
    std::stringstream ss;
    ss << "\nProfiler status:\n";
    for (const auto &act : queue_) {
      ss << act.debug_string();
    }
    return ss.str();
  }

private:
  std::vector<Action> queue_;
};

inline Action &Profiler::AddAction(const std::string &owner,
                                   OwnerType owner_type,
                                   ActionType action_type) {
  queue_.emplace_back(owner);
  auto &action = queue_.back();
  action.action_type = action_type;
  action.owner_type = owner_type;
  action.timestramp = time(NULL);
  return action;
}

} // namespace profile
} // namespace engine
