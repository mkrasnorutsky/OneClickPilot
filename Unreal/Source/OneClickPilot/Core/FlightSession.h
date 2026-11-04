// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once
#include "FlightDynamics.h"
#include "PropertyStore.h"
#include "OneClickTransport.h"
#include <string>
#include "CircuitInstructor.hpp"
#include "ControlMixer.hpp"

namespace ocp {
struct FlightAPI {
    decltype(&ocp_fdm_abi) abi=nullptr;
    decltype(&ocp_fdm_create) create=nullptr;
    decltype(&ocp_fdm_destroy) destroy=nullptr;
    decltype(&ocp_fdm_step) step=nullptr;
    decltype(&ocp_fdm_set) set=nullptr;
    decltype(&ocp_fdm_get) get=nullptr;
    decltype(&ocp_fdm_state) state=nullptr;
    decltype(&ocp_fdm_error) error=nullptr;
    bool valid() const {return abi && create && destroy && step && set && get && state && error && abi()==OCP_FDM_ABI;}
};
// Single owner/game thread. Transport and JSBSim are polled on that same thread.
class FlightSession {
public:
    explicit FlightSession(FlightAPI);
    ~FlightSession();
    void start(const std::string& resources,bool airborne=false);
    void tick(double seconds);
    void stop();
    bool command(const std::string&);
    CircuitInstructor::Phase instructorPhase() const { return instructor.phase(); }
    PropertyStore properties;
    ControlMixer mixer;
    OCPFlightState state{};
private:
    FlightAPI api;
    OCPFlightHandle flight=nullptr;
    OneClickTransport transport;
    CircuitInstructor instructor;
};
}
