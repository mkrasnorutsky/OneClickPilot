// SPDX-License-Identifier: MIT
#pragma once

#include "FlightDynamics.h"

bool WriteFlightDiagnostics(const char* Path, uint64_t Frames, const OCPFlightState& State);
