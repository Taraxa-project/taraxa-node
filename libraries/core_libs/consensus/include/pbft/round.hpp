#pragma once

#include "pbft/round_face.hpp"
#include "pbft/step/certify.hpp"
#include "pbft/step/filter.hpp"
#include "pbft/step/finish.hpp"
#include "pbft/step/polling.hpp"
#include "pbft/step/propose.hpp"
#include "pbft/step/step.hpp"

namespace taraxa {

class Round : public RoundFace {
 public:
  Round(uint64_t id, std::shared_ptr<NodeFace> node);
  ~Round();

  static std::shared_ptr<Round> make(uint64_t id, std::optional<uint64_t> step, std::shared_ptr<NodeFace> node);

  void start(std::optional<uint64_t> step = {}) override;
  void sleepUntil(const time_point& time);
  void sleepUntil(const std::chrono::milliseconds& time) override;
  /**
   * @brief Set PBFT step
   * @param pbft_step PBFT step
   */
  void updateStepData();

  void finish() override;

  void setStep(std::unique_ptr<Step> step) override {
    step_ = std::move(step);
    if (step_) {
      updateStepData();
    }
  }

  uint64_t getStepId() override { return step_->getId(); }
  StepType getStepType() const override;

  void run() override;

  template <class T, typename... Args>
  void startStep(Args... args) {
    setStep(std::make_unique<T>(args..., shared_from_this()));
  }

  void nextStep_();

  std::unique_ptr<Step> step_;

 private:
  std::unique_ptr<std::thread> daemon_;

  std::condition_variable stop_cv_;
  std::mutex stop_mtx_;

  void initDbValues();

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa