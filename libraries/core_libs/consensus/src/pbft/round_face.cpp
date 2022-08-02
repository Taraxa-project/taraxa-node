#include "pbft/round_face.hpp"

namespace taraxa {

StepType stepTypeFromId(uint32_t id) {
  // We could have multiple finish and polling steps. So all even steps with id bigger than 5 are finish steps and odd
  // are polling
  if (id > StepType::polling) {
    return StepType(4 + (id % 2));
  }
  return StepType(id);
}

}  // namespace taraxa