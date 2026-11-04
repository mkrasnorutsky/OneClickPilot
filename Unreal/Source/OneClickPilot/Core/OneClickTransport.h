// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once
#include "ControlMixer.hpp"
#include <functional>
#include <memory>
#include <string>
class OneClickTransport final {
public:
    using Command=std::function<bool(const std::string&)>;
    OneClickTransport(ocp::ControlMixer*,Command);
    ~OneClickTransport();
    void init();
    void update(double);
    void shutdown();
private:
    struct State;
    std::unique_ptr<State> state;
};
