#include "pd_display.h"

#include <cstring>

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
    this->set_message("HELLO");
}

void PDDisplayComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "PD Display:");
    ESP_LOGCONFIG(TAG, "  Scroll Interval: %u ms", (unsigned)this->scroll_ms_);
    ESP_LOGCONFIG(TAG, "  Data Pins (D0-D7):");
    for (auto* pin : this->data_pins_) {
        LOG_PIN("    ", pin);
    }
    LOG_PIN("  A0 Pin: ", this->pin_a0_);
    LOG_PIN("  A1 Pin: ", this->pin_a1_);
    LOG_PIN("  A2 Pin: ", this->pin_a2_);
    LOG_PIN("  CE0 Pin: ", this->pin_ce0_);
    LOG_PIN("  WR Pin: ", this->pin_wr_);
    LOG_PIN("  RST Pin: ", this->pin_rst_);
}

void PDDisplayComponent::loop() {
    if (this->message_changed_) {
        this->message_changed_ = false;
        this->scroll_pos_ = 0;

        // Messages that fit on the display are shown without scrolling
        if (this->message_.size() <= 4) {
            this->padded_.clear();
            this->write_display(this->message_.c_str());
            return;
        }

        this->padded_ = "    " + this->message_ + "    ";
        // Show the first frame now rather than after one scroll interval
        this->last_scroll_ms_ = millis() - this->scroll_ms_;
    }

    if (this->padded_.empty()) return;

    uint32_t now = millis();
    if (now - this->last_scroll_ms_ < this->scroll_ms_) return;
    this->last_scroll_ms_ = now;

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
    // The character ROM is 7-bit ASCII and D7 on a digit enables its
    // attribute, so replace each multi-byte UTF-8 character with '?'
    std::string clean;
    clean.reserve(msg.size());
    for (unsigned char c : msg) {
        if (c < 0x80) {
            clean += (char)c;
        } else if (c >= 0xC0) {
            clean += '?';
        }
        // 0x80-0xBF are UTF-8 continuation bytes, skip them
    }

    this->message_ = clean;
    this->message_changed_ = true;
}

void PDDisplayComponent::set_brightness(uint8_t level) {
    // Control word D1:D0 = brightness (0 blank, 1 25%, 2 50%, 3 100%)
    uint8_t cmd = level & 0x03;
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

}  // namespace pd_display
}  // namespace esphome