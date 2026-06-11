#include "pd_display.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pd_display {

static const char* const TAG = "pd_display";

void PDDisplayComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up PD Display...");

    // Initialize data bus pins
    for (auto* pin : this->data_pins_) {
        pin->setup();
        pin->digital_write(false);
    }

    // Initialize control pins
    this->pin_a0_->setup();
    this->pin_a1_->setup();
    this->pin_a2_->setup();
    this->pin_wr_->setup();
    this->pin_ce0_->setup();
    this->pin_rst_->setup();

    // Deassert active-low lines
    this->pin_ce0_->digital_write(true);
    this->pin_wr_->digital_write(true);
    this->pin_rst_->digital_write(true);
    delay(10);

    // Hardware reset pulse
    this->pin_rst_->digital_write(false);
    delayMicroseconds(500);
    this->pin_rst_->digital_write(true);
    delay(10);

    this->clear_display();
    this->init_display();
    this->set_message("HELO");
}

void PDDisplayComponent::loop() {
    if (this->message_.empty()) return;

    uint32_t now = millis();
    if (now - this->last_scroll_ms_ < this->scroll_ms_) return;
    this->last_scroll_ms_ = now;

    if (this->message_changed_) {
        this->padded_ = "    " + this->message_ + "    ";
        this->scroll_pos_ = 0;
        this->message_changed_ = false;
    }

    if (this->scroll_pos_ + 4 <= (int)this->padded_.size()) {
        char frame[5];
        strncpy(frame, &this->padded_[this->scroll_pos_], 4);
        frame[4] = '\0';
        this->write_display(frame);
        this->scroll_pos_++;
    } else {
        this->scroll_pos_ = 0;
    }
}

void PDDisplayComponent::set_message(const std::string& msg) {
    this->message_ = msg;
    this->message_changed_ = true;
}

void PDDisplayComponent::set_brightness(uint8_t level) {
    uint8_t cmd = 0x03 | ((level & 0x03) << 4);
    this->write_raw(false, 0, cmd);
    delay(5);
}

void PDDisplayComponent::set_data_bus(uint8_t val) {
    for (int i = 0; i < 8; i++) {
        this->data_pins_[i]->digital_write((val >> i) & 0x01);
    }
}

void PDDisplayComponent::set_addr(int addr) {
    this->pin_a0_->digital_write(addr & 0x01);
    this->pin_a1_->digital_write(addr & 0x02);
}

void PDDisplayComponent::write_raw(bool a2_state, int addr, uint8_t data) {
    this->pin_a2_->digital_write(a2_state);
    this->set_addr(addr);
    this->set_data_bus(data);

    this->pin_ce0_->digital_write(false);
    delayMicroseconds(1);

    this->pin_wr_->digital_write(false);
    delayMicroseconds(2);
    this->pin_wr_->digital_write(true);

    this->pin_ce0_->digital_write(true);
    delayMicroseconds(1);
}

void PDDisplayComponent::init_display() {
    this->write_raw(false, 0, 0x03);
    delay(5);
}

void PDDisplayComponent::clear_display() {
    this->write_raw(false, 0, 0x83);
    delay(5);
}

void PDDisplayComponent::put_char(int pos, char ch) {
    this->write_raw(true, pos, (uint8_t)ch);
}

void PDDisplayComponent::write_display(const char* input) {
    char buf[4] = {' ', ' ', ' ', ' '};
    for (int i = 0; i < 4 && input[i] != '\0'; i++) {
        buf[i] = input[i];
    }
    this->put_char(3, buf[0]);
    this->put_char(2, buf[1]);
    this->put_char(1, buf[2]);
    this->put_char(0, buf[3]);
}

}  // namespace PD_display
}  // namespace esphome