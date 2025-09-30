#include "deep_sleep_libretiny.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace deep_sleep {
bool global_has_deep_sleep =
    false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}

namespace deep_sleep_libretiny {

static const char *const TAG = "deep_sleep_libretiny";

optional<uint32_t> DeepSleepLibretiny::get_run_duration_() const {
  return this->run_duration_;
}

void DeepSleepLibretiny::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  esphome::deep_sleep::global_has_deep_sleep = true;

  const optional<uint32_t> run_duration = get_run_duration_();
  if (run_duration.has_value()) {
    ESP_LOGI(TAG, "Scheduling Deep Sleep to start in %" PRIu32 " ms",
             *run_duration);
    this->set_timeout(*run_duration, [this]() { this->begin_sleep(); });
  } else {
    ESP_LOGD(TAG,
             "Not scheduling Deep Sleep, as no run duration is configured.");
  }
}
void DeepSleepLibretiny::dump_config() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  if (this->sleep_duration_.has_value()) {
    uint32_t duration = *this->sleep_duration_ / 1000;
    ESP_LOGCONFIG(TAG, "  Sleep Duration: %" PRIu32 " ms", duration);
  }
  if (this->run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Run Duration: %" PRIu32 " ms", *this->run_duration_);
  }
  if (this->wakeup_cause_to_run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Default Wakeup Run Duration: %" PRIu32 " ms",
                  this->wakeup_cause_to_run_duration_->default_cause);
    ESP_LOGCONFIG(TAG, "  Touch Wakeup Run Duration: %" PRIu32 " ms",
                  this->wakeup_cause_to_run_duration_->touch_cause);
    ESP_LOGCONFIG(TAG, "  GPIO Wakeup Run Duration: %" PRIu32 " ms",
                  this->wakeup_cause_to_run_duration_->gpio_cause);
  }
  if (wakeup_pins_.size() > 0) {
    for (WakeUpPinItem item : this->wakeup_pins_) {
      LOG_PIN("  Wakeup Pin: ", item.wakeup_pin);
    }
  }
}
void DeepSleepLibretiny::loop() {
  if (this->next_enter_deep_sleep_)
    this->begin_sleep();
}
float DeepSleepLibretiny::get_loop_priority() const {
  return -100.0f; // run after everything else is ready
}
void DeepSleepLibretiny::set_sleep_duration(uint32_t time_ms) {
  this->sleep_duration_ = uint64_t(time_ms) * 1000;
}

void DeepSleepLibretiny::set_run_duration(
    WakeupCauseToRunDuration wakeup_cause_to_run_duration) {
  wakeup_cause_to_run_duration_ = wakeup_cause_to_run_duration;
}

bool DeepSleepLibretiny::pin_prevents_sleep(WakeUpPinItem &pinItem) const {
  return (pinItem.wakeup_pin_mode == WAKEUP_PIN_MODE_KEEP_AWAKE &&
          pinItem.wakeup_pin != nullptr && !this->sleep_duration_.has_value() &&
          (pinItem.wakeup_level == get_real_pin_state(*pinItem.wakeup_pin)));
}

void DeepSleepLibretiny::set_run_duration(uint32_t time_ms) {
  this->run_duration_ = time_ms;
}
void DeepSleepLibretiny::begin_sleep(bool manual) {
  if (this->prevent_ && !manual) {
    this->next_enter_deep_sleep_ = true;
    return;
  }
  if (wakeup_pins_.size() > 0) {
    for (WakeUpPinItem &item : this->wakeup_pins_) {
      if (pin_prevents_sleep(item)) {
        // Defer deep sleep until inactive
        if (!this->next_enter_deep_sleep_) {
          this->status_set_warning();
          ESP_LOGW(TAG,
                  "Waiting for pin_ to switch state to enter deep sleep...");
        }
        this->next_enter_deep_sleep_ = true;
        return;
      }
    }
  }

  ESP_LOGI(TAG, "Beginning Deep Sleep");
  if (this->sleep_duration_.has_value()) {
    ESP_LOGI(TAG, "Sleeping for %" PRId64 "us", *this->sleep_duration_);
  }

  for (WakeUpPinItem &item : this->wakeup_pins_) {
    if (item.wakeup_pin_mode == WAKEUP_PIN_MODE_INVERT_WAKEUP) {
      if (item.wakeup_level == get_real_pin_state(*item.wakeup_pin))
      {
        item.wakeup_level = !item.wakeup_level;
      }
    }
    ESP_LOGI(TAG, "Wake-up on P%u %s (%d)", item.wakeup_pin->get_pin(), item.wakeup_level? "HIGH" : "LOW", static_cast<int32_t>(item.wakeup_pin_mode));
  }
  App.run_safe_shutdown_hooks();

  if (this->sleep_duration_.has_value())
    lt_deep_sleep_config_timer((*this->sleep_duration_ / 1000) & 0xFFFFFFFF);

  for (WakeUpPinItem &item : this->wakeup_pins_) {
    lt_deep_sleep_config_gpio(1 << item.wakeup_pin->get_pin(),
                              item.wakeup_level);
    lt_deep_sleep_keep_floating_gpio(1 << item.wakeup_pin->get_pin(),
                              true);
  }

  lt_deep_sleep_enter();
}
float DeepSleepLibretiny::get_setup_priority() const {
  return setup_priority::DATA;
}
void DeepSleepLibretiny::prevent_deep_sleep() { this->prevent_ = true; }
void DeepSleepLibretiny::allow_deep_sleep() { this->prevent_ = false; }

} // namespace deep_sleep_libretiny
} // namespace esphome
