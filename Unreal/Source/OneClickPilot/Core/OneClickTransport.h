// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once
#include "PropertyStore.h"
#include <functional>
#include <memory>
#include <string>
class OneClickTransport final {
public:
    using Command=std::function<bool(const std::string&)>;
    OneClickTransport(ocp::PropertyStore*,Command);
    ~OneClickTransport();
    void init();
    void update(double);
    void shutdown();
private:
    struct State;
    std::unique_ptr<State> state;
};
