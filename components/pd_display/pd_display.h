#pragma once

#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace PD_display {

class PDDisplayComponent : public Component {
   public:
    void set_data_pins(std::vector<GPIOPin*> pins) { data_pins_ = pins; }
    void set_a0_pin(GPIOPin* pin) { pin_a0_ = pin; }
    void set_a1_pin(GPIOPin* pin) { pin_a1_ = pin; }
    void set_a2_pin(GPIOPin* pin) { pin_a2_ = pin; }
    void set_ce0_pin(GPIOPin* pin) { pin_ce0_ = pin; }
    void set_wr_pin(GPIOPin* pin) { pin_wr_ = pin; }
    void set_rst_pin(GPIOPin* pin) { pin_rst_ = pin; }
    void set_scroll_ms(uint32_t scroll_ms) { scroll_ms_ = scroll_ms; }

    float get_setup_priority() const override { return setup_priority::HARDWARE; }
    void setup() override;
    void loop() override;

    void set_message(const std::string& msg);
    void set_brightness(uint8_t level);

   private:
    void set_data_bus(uint8_t val);
    void set_addr(int addr);
    void write_raw(bool a2_state, int addr, uint8_t data);
    void init_display();
    void clear_display();
    void put_char(int pos, char ch);
    void write_display(const char* input);

    std::vector<GPIOPin*> data_pins_;
    GPIOPin* pin_a0_{nullptr};
    GPIOPin* pin_a1_{nullptr};
    GPIOPin* pin_a2_{nullptr};
    GPIOPin* pin_ce0_{nullptr};
    GPIOPin* pin_wr_{nullptr};
    GPIOPin* pin_rst_{nullptr};

    uint32_t scroll_ms_{300};
    std::string message_;
    std::string padded_;
    bool message_changed_{false};
    int scroll_pos_{0};
    uint32_t last_scroll_ms_{0};
};

}  // namespace PD_display
}  // namespace esphome