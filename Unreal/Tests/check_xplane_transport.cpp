// SPDX-License-Identifier: MIT
#include "OneClickTransport.h"

#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    ocp::PropertyStore props;
    int parkingCommands = 0;
    OneClickTransport transport(&props, [&](const std::string& command) {
        assert(command == "oneclickpilot-parking-brake");
        ++parkingCommands;
        return true;
    });
    transport.init();
    const int sender = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sender >= 0);
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(49000);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    auto send = [&](const char* bytes, size_t size) {
        assert(sendto(sender, bytes, size, 0, reinterpret_cast<sockaddr*>(&destination),
                      sizeof destination) == static_cast<ssize_t>(size));
    };
    auto axis = [&](const char* path, float value) {
        std::array<char, 509> packet{};
        std::memcpy(packet.data(), "DREF", 4);
        std::memcpy(packet.data() + 5, &value, sizeof value);
        std::strcpy(packet.data() + 9, path);
        send(packet.data(), packet.size());
    };
    auto command = [&](const char* path) {
        std::string packet("CMND\0", 5);
        packet += path;
        send(packet.c_str(), packet.size() + 1);
    };

    axis("sim/joystick/yoke_pitch_ratio", .5f);
    axis("sim/joystick/yoke_roll_ratio", -.25f);
    axis("sim/joystick/yoke_heading_ratio", .25f);
    axis("sim/cockpit2/engine/actuators/throttle_ratio_all", .75f);
    axis("sim/cockpit2/engine/actuators/throttle_ratio_all", std::numeric_limits<float>::quiet_NaN());
    command("sim/flight_controls/brakes_toggle_max");
    command("sim/flight_controls/flaps_down");
    command("sim/flight_controls/flaps_down");
    command("sim/flight_controls/flaps_up");
    command("sim/lights/landing_lights_on");
    command("sim/view/right_90");
    send("1,2,3", 5);
    transport.update(0);
    assert(props.getDoubleValue("/controls/flight/elevator") == -.5);
    assert(props.getDoubleValue("/controls/flight/aileron") == -.25);
    assert(props.getDoubleValue("/controls/flight/rudder") == .25);
    assert(props.getDoubleValue("/controls/engines/current-engine/throttle") == .75);
    assert(parkingCommands == 1);
    assert(props.getDoubleValue("/controls/flight/flaps") == 1.0 / 3);
    assert(props.getBoolValue("/controls/lighting/landing-lights"));
    assert(props.getDoubleValue("/sim/current-view/heading-offset-deg") == 90);
    assert(props.getDoubleValue("/sim/fly2gether/rejected-packets") == 2);
    close(sender);
}
