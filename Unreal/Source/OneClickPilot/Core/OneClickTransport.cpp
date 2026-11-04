// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#include "OneClickTransport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr int ControlPort = 49000;
constexpr const char* Flaps = "/controls/flight/flaps";

struct Axis {
    std::string_view dataref;
    const char* property;
    double minimum;
    double sign;
};

constexpr Axis Axes[] = {
    {"sim/joystick/yoke_roll_ratio", "/controls/flight/aileron", -1, 1},
    {"sim/joystick/yoke_pitch_ratio", "/controls/flight/elevator", -1, -1},
    {"sim/joystick/yoke_heading_ratio", "/controls/flight/rudder", -1, 1},
    {"sim/cockpit2/controls/elevator_trim", "/controls/flight/elevator-trim", -1, 1},
    {"sim/cockpit2/engine/actuators/throttle_ratio_all", "/controls/engines/current-engine/throttle", 0, 1},
    {"sim/cockpit2/engine/actuators/mixture_ratio_all", "/controls/engines/current-engine/mixture", 0, 1},
};

struct Switch {
    std::string_view command;
    const char* property;
};

constexpr Switch Switches[] = {
    {"sim/lights/landing_lights", "/controls/lighting/landing-lights"},
    {"sim/lights/taxi_lights", "/controls/lighting/taxi-light"},
    {"sim/lights/nav_lights", "/controls/lighting/nav-lights"},
    {"sim/lights/beacon_lights", "/controls/lighting/beacon"},
    {"sim/lights/strobe_lights", "/controls/lighting/strobe"},
    {"sim/electrical/battery_1", "/controls/switches/master-bat"},
};
}

struct OneClickTransport::State {
    ocp::ControlMixer* root;
    Command command;
    int udp = -1;

    State(ocp::ControlMixer* props, Command run) : root(props), command(std::move(run)) {}
    ~State() { stop(); }

    void stop() {
        if (udp >= 0) ::close(udp);
        udp = -1;
    }

    void count(const char* property) {
        auto node = root->getNode(property, true);
        node->setIntValue(node->getIntValue() + 1);
    }

    bool dataref(std::string_view path, double value) {
        for (const Axis& axis : Axes) {
            if (path == axis.dataref) {
                root->setDoubleValue(axis.property, std::clamp(value * axis.sign, axis.minimum, 1.0));
                return true;
            }
        }
        // Fly2Gether sends these with its native X-Plane controls. The C172
        // has fixed propeller pitch and needs no simulator joystick override.
        return path == "sim/cockpit2/engine/actuators/prop_ratio_all" ||
               path == "sim/operation/override/override_joystick_pitch" ||
               path == "sim/operation/override/override_joystick_roll" ||
               path == "sim/operation/override/override_joystick_heading";
    }

    bool run(std::string_view path) {
        if (path == "sim/flight_controls/brakes_toggle_max")
            return command("oneclickpilot-parking-brake");
        if (path == "sim/flight_controls/landing_gear_toggle")
            return true; // The C172 has fixed landing gear.
        if (path == "sim/flight_controls/flaps_up" || path == "sim/flight_controls/flaps_down") {
            const int detent = static_cast<int>(std::lround(std::clamp(root->getDoubleValue(Flaps), 0.0, 1.0) * 3));
            const int direction = path == "sim/flight_controls/flaps_down" ? 1 : -1;
            root->setDoubleValue(Flaps, std::clamp(detent + direction, 0, 3) / 3.0);
            return true;
        }
        if (path == "sim/view/forward_with_2d_panel" || path == "sim/view/forward_with_nothing" ||
            path == "sim/view/left_45" || path == "sim/view/right_45" ||
            path == "sim/view/left_90" || path == "sim/view/right_90" || path == "sim/view/back") {
            double heading = 0;
            if (path == "sim/view/left_45") heading = -45;
            if (path == "sim/view/right_45") heading = 45;
            if (path == "sim/view/left_90") heading = -90;
            if (path == "sim/view/right_90") heading = 90;
            if (path == "sim/view/back") heading = 180;
            root->setDoubleValue("/sim/current-view/heading-offset-deg", heading);
            root->setDoubleValue("/sim/current-view/pitch-offset-deg", 0);
            root->setDoubleValue("/sim/current-view/roll-offset-deg", 0);
            return true;
        }
        for (const Switch& control : Switches) {
            if (path.substr(0, control.command.size()) != control.command) continue;
            const auto suffix = path.substr(control.command.size());
            if (suffix != "_on" && suffix != "_off" && suffix != "_toggle") continue;
            root->setBoolValue(control.property, suffix == "_on" ||
                (suffix == "_toggle" && !root->getBoolValue(control.property)));
            return true;
        }
        return false;
    }

    bool packet(std::string_view bytes) {
        if (bytes.size() < 6 || bytes[4] != '\0') return false;
        const auto type = bytes.substr(0, 4);
        if (type == "DREF" && bytes.size() == 509) {
            const auto field = bytes.substr(9);
            const auto end = field.find('\0');
            if (end == std::string_view::npos || end == 0) return false;
            uint32_t bits = 0;
            for (unsigned i = 0; i < 4; ++i)
                bits |= uint32_t(static_cast<unsigned char>(bytes[5 + i])) << (8 * i);
            float value;
            static_assert(sizeof value == sizeof bits);
            std::memcpy(&value, &bits, sizeof value);
            return std::isfinite(value) && dataref(field.substr(0, end), value);
        }
        if (type == "CMND" && bytes.size() <= 505 && bytes.back() == '\0') {
            const auto path = bytes.substr(5, bytes.size() - 6);
            if (path.empty() || path.find('\0') != std::string_view::npos) return false;
            if (!run(path)) return false;
            count("/sim/fly2gether/custom-received");
            return true;
        }
        return false;
    }

    void poll() {
        char buffer[510];
        for (int i = 0; i < 128; ++i) {
            const auto size = recv(udp, buffer, sizeof buffer, 0);
            if (size < 0) break;
            const bool accepted = packet(std::string_view(buffer, static_cast<size_t>(size)));
            count(accepted ? "/sim/fly2gether/received-packets" : "/sim/fly2gether/rejected-packets");
        }
    }
};

OneClickTransport::OneClickTransport(ocp::ControlMixer* root, Command command)
    : state(new State(root, std::move(command))) {}
OneClickTransport::~OneClickTransport() = default;

void OneClickTransport::init() {
    state->stop();
    state->udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (state->udp < 0) throw std::runtime_error("Cannot create X-Plane UDP socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(ControlPort);
    if (fcntl(state->udp, F_SETFL, O_NONBLOCK) < 0 ||
        bind(state->udp, reinterpret_cast<sockaddr*>(&address), sizeof address) < 0) {
        state->stop();
        throw std::runtime_error("Cannot bind X-Plane UDP port 49000");
    }
}

void OneClickTransport::update(double) { if (state->udp >= 0) state->poll(); }
void OneClickTransport::shutdown() { state->stop(); }
