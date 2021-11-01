/* Copyright (C) 2020-2021 Oxan van Leeuwen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <memory>
#include <string>
#include <vector>
#include <Stream.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif

class StreamServerComponent : public esphome::Component {
public:
    StreamServerComponent() = default;
    void set_uart_parent(esphome::uart::UARTComponent *parent) { this->stream_ = parent; }

    void setup() override;
    void loop() override;
    void dump_config() override;
    void on_shutdown() override;

    float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

    void set_port(uint16_t port) { this->port_ = port; }

    void set_max_clients(int max_clients) { this->max_clients_ = max_clients; }

    void set_hello_message(const char *hello_message) { this->hello_message_ = hello_message; }

protected:
    void discard_clients();
    void cleanup();
    void read();
    bool flush();
    void write();

    struct Client {
        Client(AsyncClient *client, std::vector<uint8_t> &recv_buf);
        ~Client();

        AsyncClient *tcp_client{nullptr};
        std::string identifier{};
        bool disconnected{false};
    };

    esphome::uart::UARTComponent *stream_{nullptr};
    AsyncServer server_{0};
    std::string hello_message_{};
    uint16_t port_{6638};
    int max_clients_{-1};
    std::vector<uint8_t> send_buf_{};
    int send_client_{0};
    std::vector<uint8_t> recv_buf_{};
    std::vector<std::unique_ptr<Client>> clients_{};
    esphome::HighFrequencyLoopRequester high_freq_;
};
