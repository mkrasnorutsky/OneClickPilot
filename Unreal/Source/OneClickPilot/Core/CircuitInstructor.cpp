// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#include "CircuitInstructor.hpp"
#include <algorithm>
#include <cmath>

using namespace ocp;

void CircuitInstructor::reset() {
    currentPhase = Phase::WaitingForBrakeRelease;
    phaseSeconds = 0.0;
}

CircuitInstructor::Controls CircuitInstructor::update(const OCPFlightState& aircraft,
                                                      bool parkingBrakeReleased,
                                                      double seconds,
                                                      bool mainGearOnGround) {

    constexpr double runwayHeading = 20.0;
    constexpr double runwayElevationFt = 7.0;
    constexpr double rotationStartKt = 51.0;
    constexpr double rotationRampKt = 7.0;
    const RunwayPosition runway = positionRelativeToRunway(aircraft);
    const double height = aircraft.altitude_ft - runwayElevationFt;

    if (currentPhase == Phase::WaitingForBrakeRelease) {
        if (!parkingBrakeReleased) return {};
        currentPhase = Phase::TakeOffRoll;
        phaseSeconds = 0.0;
    }

    phaseSeconds += std::clamp(seconds, 0.0, .25);
    const double targetHeading = runwayHeading - std::clamp(runway.cross_m * (currentPhase == Phase::TakeOffRoll ? 1.1 : .40), -12.0, 12.0);
    const double headingError = std::remainder(targetHeading - aircraft.heading_deg, 360.0);

    if (currentPhase == Phase::TakeOffRoll) {
        if (!parkingBrakeReleased || std::abs(runway.cross_m) >= 8.0 ||
            runway.along_m >= 1036.0 || phaseSeconds >= 45.0) {
            currentPhase = Phase::Failed;
            return {};
        }
        if (height >= 18.0 && aircraft.vertical_speed_fps > 0.0) {
            currentPhase = Phase::InitialClimb;
            phaseSeconds = 0.0;
        } else {
            const double rotation = std::clamp((aircraft.airspeed_kts - rotationStartKt) / rotationRampKt, 0.0, 1.0);
            const double elevator = rotation * std::clamp((aircraft.pitch_deg - 11.0) * .055, -.30, .20);
            const double rudder = std::clamp(headingError * .075, -.45, .45);
            return {1.0, elevator, 0.0, rudder, 0.0};
        }
    }

    auto enter = [&](Phase phase) {
        currentPhase = phase;
        phaseSeconds = 0.0;
    };
    auto headingTo = [&](double heading) {
        return std::remainder(heading - aircraft.heading_deg, 360.0);
    };
    auto aligned = [&](double heading) {
        return std::abs(headingTo(heading)) < 4.0 && std::abs(aircraft.roll_deg) < 6.0;
    };
    constexpr double crosswindHeading = runwayHeading - 90.0;
    constexpr double downwindHeading = runwayHeading - 180.0;
    constexpr double baseHeading = runwayHeading + 90.0;
    constexpr double patternHeight = 800.0;

    if (currentPhase == Phase::InitialClimb && height >= 500.0 && runway.along_m >= 1036.0)
        enter(Phase::Turn1);
    if (currentPhase == Phase::Turn1 && aligned(crosswindHeading))
        enter(Phase::Crosswind);
    if (currentPhase == Phase::Crosswind && runway.cross_m <= -950.0)
        enter(Phase::Turn2);
    if (currentPhase == Phase::Turn2 && aligned(downwindHeading))
        enter(Phase::Downwind);
    if (currentPhase == Phase::Downwind && runway.along_m <= -1550.0)
        enter(Phase::Turn3);
    if (currentPhase == Phase::Turn3 && aligned(baseHeading))
        enter(Phase::Base);
    if (currentPhase == Phase::Base && runway.cross_m >= -390.0)
        enter(Phase::Turn4);
    if (currentPhase == Phase::Turn4 && aligned(runwayHeading))
        enter(Phase::Final);

    const bool approach = currentPhase == Phase::Turn4 || currentPhase == Phase::Final;
    if (approach && height < 24.0 && runway.along_m >= 0.0 &&
        std::abs(runway.cross_m) < 7.0 && std::abs(headingTo(runwayHeading)) < 12.0)
        enter(Phase::Flare);
    if (currentPhase == Phase::Flare && mainGearOnGround) {
        if (runway.along_m < 0.0 || runway.along_m > 950.0 ||
            std::abs(runway.cross_m) >= 8.0 || aircraft.vertical_speed_fps < -9.0) {
            enter(Phase::Failed);
        } else {
            enter(Phase::Rollout);
        }
    }
    if (currentPhase == Phase::Rollout) {
        if (std::abs(runway.cross_m) >= 8.0 || runway.along_m >= 1036.0 || phaseSeconds > 35.0) {
            enter(Phase::Failed);
            return {};
        }
        if (aircraft.airspeed_kts < 2.0) {
            enter(Phase::Complete);
            return {};
        }
        return {0.0, 0.0, 0.0, std::clamp(headingError * .075, -.45, .45), .65};
    }
    if (currentPhase == Phase::Complete || currentPhase == Phase::Failed)
        return {};
    if (phaseSeconds > 180.0 || (currentPhase != Phase::InitialClimb && currentPhase != Phase::Flare && height < 7.0)) {
        enter(Phase::Failed);
        return {};
    }

    const bool departure = currentPhase == Phase::InitialClimb;
    const bool descending = currentPhase == Phase::Turn3 || currentPhase == Phase::Base;
    const bool flare = currentPhase == Phase::Flare;
    double trackError = headingError;
    switch (currentPhase) {
    case Phase::Turn1:
    case Phase::Crosswind: trackError = headingTo(crosswindHeading); break;
    case Phase::Turn2: trackError = headingTo(downwindHeading); break;
    case Phase::Downwind: trackError = headingTo(downwindHeading + std::clamp((runway.cross_m + 1340) * .12, -12.0, 12.0)); break;
    case Phase::Turn3:
    case Phase::Base: trackError = headingTo(baseHeading); break;
    case Phase::Turn4: trackError = headingTo(runwayHeading); break;
    default: break;
    }

    const double bankLimit = departure ? 10.0 : (flare ? 5.0 : 20.0);
    const double targetBank = std::clamp(trackError, -bankLimit, bankLimit);
    const double climbPitch = std::clamp(10.0 + (aircraft.airspeed_kts - 65.0) * .5, 3.0, 16.0);
    const double captureWeight = departure ? 0.0 : (currentPhase == Phase::Turn1
        ? std::clamp(phaseSeconds / 5.0, 0.0, 1.0) : 1.0);
    const double targetHeight = descending ? 340.0 : patternHeight;
    const double altitudePitch = std::clamp(4.5 + (targetHeight - height) * .045 -
                                            aircraft.vertical_speed_fps * .20, -3.0, 16.0);
    double targetPitch = std::lerp(climbPitch, std::min(climbPitch, altitudePitch), captureWeight);
    const double captureThrottle = std::clamp(.55 + (75.0 - aircraft.airspeed_kts) * .045 +
                                              (targetHeight - height) * .003, .22, 1.0);
    double throttle = std::lerp(1.0, captureThrottle, captureWeight);
    if (approach) {
        const double glideHeight = std::max(8.0, 43.0 - runway.along_m * .172);
        const double targetSink = std::clamp(-6.0 + (glideHeight - height) * .075, -14.0, 3.0);
        targetPitch = std::clamp(5.0 + (targetSink - aircraft.vertical_speed_fps) * .85, -8.0, 10.0);
        throttle = std::clamp(.35 + (65.0 - aircraft.airspeed_kts) * .06, 0.0, .8);
    }
    if (flare) {
        targetPitch = std::clamp(8.0 - aircraft.vertical_speed_fps * .25, 5.0, 11.0);
        throttle = 0.0;
    }
    const double elevator = std::clamp((aircraft.pitch_deg - targetPitch) * .055, -.30, .30);
    const double aileron = std::clamp((targetBank - aircraft.roll_deg) * .050, -.35, .35);
    const double rudder = std::clamp(trackError * .008, -.08, .08);
    return {throttle, elevator, aileron, rudder, 0.0};
}

CircuitInstructor::RunwayPosition CircuitInstructor::positionRelativeToRunway(const OCPFlightState& state){
    constexpr double radiansPerDegree = 3.14159265358979323846 / 180.0;
    constexpr double earthRadiusM = 6378137.0;
    constexpr double originLatDeg = 50.83138094;
    constexpr double originLonDeg = -0.29991299;
    constexpr double headingRad = 20.0 * radiansPerDegree;

    const double northM =
        (state.latitude_deg - originLatDeg) * radiansPerDegree * earthRadiusM;
    const double eastM =
        (state.longitude_deg - originLonDeg) * radiansPerDegree * earthRadiusM
        * std::cos(originLatDeg * radiansPerDegree);

    return {
        northM * std::cos(headingRad) + eastM * std::sin(headingRad),
        -northM * std::sin(headingRad) + eastM * std::cos(headingRad),
    };
}
