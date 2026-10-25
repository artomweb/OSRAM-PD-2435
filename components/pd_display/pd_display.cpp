#include "pd_display.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pd_display {

static const char* const TAG = "pd_display";

// Non-ASCII glyphs in the character ROM (codes 0x00-0x1F, datasheet p.12)
struct CharMapping {
    uint32_t codepoint;
    uint8_t code;
};

static const CharMapping CHAR_MAP[] = {
    {0x00EC, 0x00},  // ì
    {0x2191, 0x01},  // ↑
    {0x2192, 0x02},  // →
    {0x2193, 0x03},  // ↓
    {0x2190, 0x04},  // ←
    {0x00BF, 0x05},  // ¿
    {0x00E0, 0x06},  // à
    {0x00D8, 0x07},  // Ø
    {0x00F8, 0x08},  // ø
    {0x00F2, 0x09},  // ò
    {0x00F9, 0x0A},  // ù
    {0x00D1, 0x0B},  // Ñ
    {0x00C7, 0x0C},  // Ç
    {0x00EA, 0x0D},  // ê
    {0x00C9, 0x0E},  // É
    {0x00E9, 0x0F},  // é
    {0x00E8, 0x10},  // è
    {0x00C6, 0x11},  // Æ
    {0x00E6, 0x12},  // æ
    {0x00C5, 0x13},  // Å
    {0x00E5, 0x14},  // å
    {0x00C4, 0x15},  // Ä
    {0x00E4, 0x16},  // ä
    {0x00D6, 0x17},  // Ö
    {0x00F6, 0x18},  // ö
    {0x00DC, 0x19},  // Ü
    {0x00FC, 0x1A},  // ü
    {0x2103, 0x1B},  // ℃
    {0x2109, 0x1C},  // ℉
    {0x00DF, 0x1D},  // ß
    {0x00A3, 0x1E},  // £
    {0x00A5, 0x1F},  // ¥
};

static const uint32_t INVALID_CODEPOINT = 0xFFFD;

// Decode one UTF-8 character starting at s[i] and advance i past it
static uint32_t decode_utf8(const std::string& s, size_t& i) {
    uint8_t c = s[i++];
    if (c < 0x80) return c;

    int extra;
    uint32_t cp;
    if ((c & 0xE0) == 0xC0) {
        extra = 1;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        extra = 2;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        extra = 3;
        cp = c & 0x07;
    } else {
        return INVALID_CODEPOINT;  // stray continuation byte
    }

    for (int k = 0; k < extra; k++) {
        if (i >= s.size() || ((uint8_t)s[i] & 0xC0) != 0x80) return INVALID_CODEPOINT;
        cp = (cp << 6) | ((uint8_t)s[i++] & 0x3F);
    }
    return cp;
}

static uint8_t to_display_code(uint32_t cp) {
    if (cp < 0x80) return (uint8_t)cp;
    for (const auto& m : CHAR_MAP) {
        if (m.codepoint == cp) return m.code;
    }
    return '?';
}

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
            this->write_display(this->message_.data(), this->message_.size());
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
        this->write_display(&this->padded_[this->scroll_pos_], 4);
        this->scroll_pos_++;
    } else {
        this->scroll_pos_ = 0;
    }
}

void PDDisplayComponent::set_message(const std::string& msg) {
    // Translate UTF-8 to ROM codes. Every code is below 0x80, so D7 (which
    // enables a digit's attribute) is never set by message text
    std::string codes;
    codes.reserve(msg.size());
    size_t i = 0;
    while (i < msg.size()) {
        uint32_t cp = decode_utf8(msg, i);

        // "°C" and "°F" each have a single glyph
        if (cp == 0x00B0 && i < msg.size() && (msg[i] == 'C' || msg[i] == 'F')) {
            codes += (char)(msg[i] == 'C' ? 0x1B : 0x1C);
            i++;
            continue;
        }

        codes += (char)to_display_code(cp);
    }

    this->message_ = codes;
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

// Takes a length rather than a C string so code 0x00 (ì) can be displayed
void PDDisplayComponent::write_display(const char* chars, size_t len) {
    char buf[4] = {' ', ' ', ' ', ' '};
    for (size_t i = 0; i < 4 && i < len; i++) {
        buf[i] = chars[i];
    }
    this->put_char(3, buf[0]);
    this->put_char(2, buf[1]);
    this->put_char(1, buf[2]);
    this->put_char(0, buf[3]);
}

}  // namespace pd_display
}  // namespace esphome