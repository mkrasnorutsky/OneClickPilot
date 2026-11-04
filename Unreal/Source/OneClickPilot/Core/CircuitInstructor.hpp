// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once

#include "FlightDynamics.h"

namespace ocp {

class CircuitInstructor {
public:
    enum class Phase {
        WaitingForBrakeRelease,
        TakeOffRoll,
        InitialClimb,
        Turn1,
        Crosswind,
        Turn2,
        Downwind,
        Turn3,
        Base,
        Turn4,
        Final,
        Flare,
        Rollout,
        Failed,
        Complete
    };
    
    struct Controls {
        double throttle = 0.0;
        double elevator = 0.0;
        double aileron = 0.0;
        double rudder = 0.0;
        double wheelBrake = 1.0;
    };
    
    void reset();
    
    Controls update(const OCPFlightState& aircraft,
                    bool parkingBrakeReleased,
                    double seconds,
                    bool mainGearOnGround);
    
    Phase phase() const {
        return currentPhase;
    }
    
private:
    
    struct RunwayPosition {
        double along_m;
        double cross_m;
    };
    
    RunwayPosition positionRelativeToRunway(const OCPFlightState&);
    
    Phase currentPhase = Phase::WaitingForBrakeRelease;
    double phaseSeconds = 0.0;
};

}
