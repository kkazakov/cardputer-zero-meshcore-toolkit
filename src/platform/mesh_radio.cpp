/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mesh_radio.h"

#include "gpio_line.h"
#include "lora_hat.h"
#include "pi4io.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#if !USE_DESKTOP
#include <RadioLib.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifndef APP_LORA_GPIO_CHIP
#define APP_LORA_GPIO_CHIP "/dev/gpiochip0"
#endif
#ifndef APP_LORA_RST_GPIO
#define APP_LORA_RST_GPIO 26
#endif
#ifndef APP_LORA_BUSY_GPIO
#define APP_LORA_BUSY_GPIO 22
#endif
#ifndef APP_LORA_IRQ_GPIO
#define APP_LORA_IRQ_GPIO 23
#endif

namespace platform {
namespace {

constexpr uint32_t kSpiSpeedHz = 1000000;
constexpr float kTcxoVoltage = 3.0f;
constexpr float kCurrentLimitMa = 140.0f;
constexpr uint32_t kRstPin = APP_LORA_RST_GPIO;
constexpr uint32_t kBusyPin = APP_LORA_BUSY_GPIO;
constexpr uint32_t kIrqPin = APP_LORA_IRQ_GPIO;

std::string radio_error(int16_t code) {
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "SX1262 error %d", static_cast<int>(code));
    return buffer;
}

#if !USE_DESKTOP
class LinuxRadioHal final : public RadioLibHal {
public:
    LinuxRadioHal(int spi_fd, GpioLine& reset, GpioLine& busy, GpioLine& irq)
        : RadioLibHal(0, 1, 0, 1, 1, 2), spi_fd_(spi_fd), reset_(reset), busy_(busy), irq_(irq) {}

    void pinMode(uint32_t pin, uint32_t) override {
        // The lines are requested with their final direction before the HAL is
        // constructed; RadioLib only asks for RESET out and BUSY/IRQ in.
        (void)pin;
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (pin == kRstPin) {
            reset_.set(value != 0);
        }
    }

    uint32_t digitalRead(uint32_t pin) override {
        bool high = false;
        if (pin == kBusyPin) {
            busy_.get(high);
        } else if (pin == kIrqPin) {
            irq_.get(high);
        }
        return high ? 1U : 0U;
    }

    void attachInterrupt(uint32_t, void (*)(void), uint32_t) override {}
    void detachInterrupt(uint32_t) override {}

    void delay(RadioLibTime_t milliseconds) override {
        if (milliseconds > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }

    void delayMicroseconds(RadioLibTime_t microseconds) override {
        if (microseconds > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
        }
    }

    RadioLibTime_t millis() override {
        using namespace std::chrono;
        return static_cast<RadioLibTime_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    RadioLibTime_t micros() override {
        using namespace std::chrono;
        return static_cast<RadioLibTime_t>(
            duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
    }

    long pulseIn(uint32_t, uint32_t, RadioLibTime_t) override { return 0; }

    void spiBegin() override {}
    void spiBeginTransaction() override {}
    void spiTransfer(uint8_t* out, size_t length, uint8_t* in) override {
        if (length == 0) {
            return;
        }
        uint8_t scratch[512] = {};
        if (length > sizeof(scratch)) {
            if (in) {
                std::memset(in, 0, length);
            }
            return;
        }
        const uint8_t* tx = out ? out : scratch;
        uint8_t* rx = in ? in : scratch;

        spi_ioc_transfer transfer{};
        transfer.tx_buf = reinterpret_cast<unsigned long>(tx);
        transfer.rx_buf = reinterpret_cast<unsigned long>(rx);
        transfer.len = static_cast<uint32_t>(length);
        transfer.speed_hz = kSpiSpeedHz;
        transfer.bits_per_word = 8;
        if (::ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &transfer) < 0 && in) {
            std::memset(in, 0, length);
        }
    }
    void spiEndTransaction() override {}
    void spiEnd() override {}
    void yield() override { std::this_thread::yield(); }

private:
    int spi_fd_;
    GpioLine& reset_;
    GpioLine& busy_;
    GpioLine& irq_;
};
#endif

} // namespace

struct MeshRadio::Impl {
#if !USE_DESKTOP
    ~Impl() {
        radio.reset();
        module.reset();
        hal.reset();
        if (spi_fd >= 0) {
            ::close(spi_fd);
            spi_fd = -1;
        }
    }

    int spi_fd{-1};
    GpioLine reset;
    GpioLine busy;
    GpioLine irq;
    std::unique_ptr<LinuxRadioHal> hal;
    std::unique_ptr<Module> module;
    std::unique_ptr<SX1262> radio;
#endif
};

MeshRadio::MeshRadio() = default;

MeshRadio::~MeshRadio() {
    shutdown();
}

bool MeshRadio::initialize(const model::MeshConfig& config, std::string& error) {
    if (ready_) {
        return true;
    }

    error_.clear();
    detail_.clear();
    spi_device_.clear();

#if USE_DESKTOP
    simulated_ = true;
    ready_ = true;
    spi_device_ = "simulator";
    detail_ = "desktop simulator, no radio hardware";
    return true;
#else
    impl_ = std::make_unique<Impl>();

    std::string stage_error;
    if (!enable_lora_power(stage_error)) {
        error_ = stage_error;
        error = error_;
        shutdown();
        return false;
    }

    std::string rf_detail;
    (void)enable_rf_switch(rf_detail); // absent on a LoRa868; not fatal
    detail_ = rf_detail;

    spi_device_ = detected_lora_spi_device();
    if (spi_device_.empty()) {
        const auto probe = probe_lora_hat();
        if (probe.working) {
            spi_device_ = probe.device;
        }
    }
    if (spi_device_.empty()) {
        error_ = "no SX1262 SPI device detected";
        error = error_;
        shutdown();
        return false;
    }

    impl_->spi_fd = ::open(spi_device_.c_str(), O_RDWR | O_CLOEXEC);
    if (impl_->spi_fd < 0) {
        error_ = "cannot open " + spi_device_ + ": " + std::strerror(errno);
        error = error_;
        shutdown();
        return false;
    }
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = kSpiSpeedHz;
    if (::ioctl(impl_->spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ::ioctl(impl_->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ::ioctl(impl_->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        error_ = "cannot configure " + spi_device_ + ": " + std::strerror(errno);
        error = error_;
        shutdown();
        return false;
    }

    if (!impl_->reset.open(APP_LORA_GPIO_CHIP, kRstPin, GpioLine::Direction::Output, true, stage_error) ||
        !impl_->busy.open(APP_LORA_GPIO_CHIP, kBusyPin, GpioLine::Direction::Input, false, stage_error) ||
        !impl_->irq.open(APP_LORA_GPIO_CHIP, kIrqPin, GpioLine::Direction::Input, false, stage_error)) {
        error_ = stage_error;
        error = error_;
        shutdown();
        return false;
    }

    impl_->hal = std::make_unique<LinuxRadioHal>(impl_->spi_fd, impl_->reset, impl_->busy, impl_->irq);
    impl_->module =
        std::make_unique<Module>(impl_->hal.get(), RADIOLIB_NC, kIrqPin, kRstPin, kBusyPin);
    impl_->module->spiConfig.timeout = 100;
    impl_->radio = std::make_unique<SX1262>(impl_->module.get());

    const int16_t state = impl_->radio->begin(static_cast<float>(config.freq_hz) / 1000000.0f,
                                              model::bandwidth_khz(config.bw_index),
                                              static_cast<uint8_t>(config.sf),
                                              static_cast<uint8_t>(config.cr),
                                              static_cast<uint8_t>(config.sync_word),
                                              static_cast<int8_t>(config.tx_power_dbm),
                                              static_cast<uint16_t>(config.preamble_len),
                                              kTcxoVoltage,
                                              false);
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " during init";
        error = error_;
        shutdown();
        return false;
    }

    (void)impl_->radio->setCurrentLimit(kCurrentLimitMa);
    (void)impl_->radio->setDio2AsRfSwitch(true);
    (void)impl_->radio->setCRC(1);
    (void)impl_->radio->setRxBoostedGainMode(true);

    ready_ = true;
    simulated_ = false;
    if (!apply_config(config)) {
        error = error_;
        shutdown();
        return false;
    }

    if (!detail_.empty()) {
        detail_ += " · ";
    }
    detail_ += spi_device_;
    return true;
#endif
}

void MeshRadio::shutdown() {
#if !USE_DESKTOP
    impl_.reset();
#endif
    restore_rf_switch();
    ready_ = false;
    simulated_ = false;
}

bool MeshRadio::ready() const {
    return ready_;
}

bool MeshRadio::simulated() const {
    return simulated_;
}

const std::string& MeshRadio::error() const {
    return error_;
}

const std::string& MeshRadio::detail() const {
    return detail_;
}

const std::string& MeshRadio::spi_device() const {
    return spi_device_;
}

bool MeshRadio::apply_config(const model::MeshConfig& config) {
    if (!ready_ || simulated_) {
        return ready_;
    }
    return set_frequency_mhz(static_cast<float>(config.freq_hz) / 1000000.0f) &&
           set_bandwidth_khz(model::bandwidth_khz(config.bw_index)) &&
           set_spreading_factor(config.sf) && set_coding_rate(config.cr) &&
           set_sync_word(config.sync_word) && set_preamble(config.preamble_len) &&
           set_tx_power(config.tx_power_dbm);
}

bool MeshRadio::apply_spectrum_config(const model::MeshConfig& config, float bandwidth_khz) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    return standby() && set_bandwidth_khz(bandwidth_khz) &&
           set_frequency_mhz(static_cast<float>(config.freq_hz) / 1000000.0f) &&
           set_spreading_factor(config.sf) && set_coding_rate(config.cr) &&
           set_rx_boosted_gain(true);
#else
    (void)config;
    (void)bandwidth_khz;
    return true;
#endif
}

bool MeshRadio::transmit(const uint8_t* data, size_t length) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    // Poll the IRQ register rather than DIO1: the Cap's DIO1 line is wired,
    // but the radio must also work if that pin is unavailable. This is the
    // same startTransmit/finishTransmit sequence MeshCore itself uses.
    int16_t state = impl_->radio->standby();
    if (state == RADIOLIB_ERR_NONE) {
        state = impl_->radio->startTransmit(data, length);
    }
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " during transmit";
        return false;
    }

    const RadioLibTime_t airtime_us = impl_->radio->getTimeOnAir(length);
    const uint32_t timeout_ms = static_cast<uint32_t>(airtime_us * 5U / 1000U) + 20U;
    const auto begin = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - begin)
               .count() < static_cast<long>(timeout_ms)) {
        if (impl_->radio->getIrqFlags() & RADIOLIB_SX126X_IRQ_TX_DONE) {
            const int16_t finished = impl_->radio->finishTransmit();
            if (finished != RADIOLIB_ERR_NONE) {
                error_ = radio_error(finished) + " finishing transmit";
                return false;
            }
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    (void)impl_->radio->finishTransmit();
    error_ = "transmit timed out";
    return false;
#else
    (void)data;
    (void)length;
    return false;
#endif
}

bool MeshRadio::start_receive() {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " starting receive";
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool MeshRadio::receive(uint8_t* data, size_t capacity, size_t& length, float& rssi, float& snr) {
    length = 0;
    if (!ready_ || simulated_) {
        return false;
    }
#if !USE_DESKTOP
    const uint16_t irq = impl_->radio->getIrqFlags();
    if (irq & (RADIOLIB_SX126X_IRQ_CRC_ERR | RADIOLIB_SX126X_IRQ_HEADER_ERR |
               RADIOLIB_SX126X_IRQ_TIMEOUT)) {
        (void)impl_->radio->clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);
        (void)impl_->radio->startReceive();
        return false;
    }
    if ((irq & RADIOLIB_SX126X_IRQ_RX_DONE) == 0) {
        return false;
    }

    const size_t packet_length = impl_->radio->getPacketLength();
    if (packet_length == 0 || packet_length > capacity) {
        (void)impl_->radio->clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);
        (void)impl_->radio->startReceive();
        return false;
    }

    const int16_t state = impl_->radio->readData(data, packet_length);
    if (state != RADIOLIB_ERR_NONE) {
        (void)impl_->radio->startReceive();
        return false;
    }
    rssi = impl_->radio->getRSSI();
    snr = impl_->radio->getSNR();
    length = packet_length;
    (void)impl_->radio->startReceive();
    return true;
#else
    (void)data;
    (void)capacity;
    (void)rssi;
    (void)snr;
    return false;
#endif
}

bool MeshRadio::standby() {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    return impl_->radio->standby() == RADIOLIB_ERR_NONE;
#else
    return true;
#endif
}

bool MeshRadio::sleep() {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    return impl_->radio->sleep(false) == RADIOLIB_ERR_NONE;
#else
    return true;
#endif
}

bool MeshRadio::set_frequency_mhz(float frequency_mhz) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setFrequency(frequency_mhz);
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting frequency";
        return false;
    }
    return true;
#else
    (void)frequency_mhz;
    return false;
#endif
}

bool MeshRadio::set_bandwidth_khz(float bandwidth_khz) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setBandwidth(bandwidth_khz);
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting bandwidth";
        return false;
    }
    return true;
#else
    (void)bandwidth_khz;
    return false;
#endif
}

bool MeshRadio::set_spreading_factor(int spreading_factor) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setSpreadingFactor(static_cast<uint8_t>(spreading_factor));
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting spreading factor";
        return false;
    }
    return true;
#else
    (void)spreading_factor;
    return false;
#endif
}

bool MeshRadio::set_coding_rate(int coding_rate) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setCodingRate(static_cast<uint8_t>(coding_rate));
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting coding rate";
        return false;
    }
    return true;
#else
    (void)coding_rate;
    return false;
#endif
}

bool MeshRadio::set_sync_word(int sync_word) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setSyncWord(static_cast<uint8_t>(sync_word & 0xFF));
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting sync word";
        return false;
    }
    return true;
#else
    (void)sync_word;
    return false;
#endif
}

bool MeshRadio::set_preamble(int symbols) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setPreambleLength(static_cast<uint16_t>(symbols));
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting preamble";
        return false;
    }
    return true;
#else
    (void)symbols;
    return false;
#endif
}

bool MeshRadio::set_tx_power(int dbm) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    const int16_t state = impl_->radio->setOutputPower(static_cast<int8_t>(dbm));
    if (state != RADIOLIB_ERR_NONE) {
        error_ = radio_error(state) + " setting TX power";
        return false;
    }
    return true;
#else
    (void)dbm;
    return false;
#endif
}

bool MeshRadio::set_rx_boosted_gain(bool enabled) {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    return impl_->radio->setRxBoostedGainMode(enabled) == RADIOLIB_ERR_NONE;
#else
    (void)enabled;
    return true;
#endif
}

float MeshRadio::instant_rssi() {
    if (!ready_ || simulated_) {
        return -120.0f;
    }
#if !USE_DESKTOP
    return impl_->radio->getRSSI(false);
#else
    return -120.0f;
#endif
}

uint16_t MeshRadio::irq_flags() {
    if (!ready_ || simulated_) {
        return 0;
    }
#if !USE_DESKTOP
    return static_cast<uint16_t>(impl_->radio->getIrqFlags());
#else
    return 0;
#endif
}

bool MeshRadio::clear_irq() {
    if (!ready_ || simulated_) {
        return ready_;
    }
#if !USE_DESKTOP
    return impl_->radio->clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL) == RADIOLIB_ERR_NONE;
#else
    return false;
#endif
}

bool MeshRadio::is_receiving() {
    if (!ready_ || simulated_) {
        return false;
    }
#if !USE_DESKTOP
    const uint16_t irq = static_cast<uint16_t>(impl_->radio->getIrqFlags());
    return (irq & (RADIOLIB_SX126X_IRQ_HEADER_VALID | RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED)) != 0;
#else
    return false;
#endif
}

} // namespace platform
