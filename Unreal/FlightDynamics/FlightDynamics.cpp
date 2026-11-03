// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
// This adapter links JSBSim inside a separate shared framework. It is not UE code.
#include "FlightDynamics.h"
#include <FGFDMExec.h>
#include <initialization/FGInitialCondition.h>
#include <models/FGPropagate.h>
#include <models/FGPropulsion.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
namespace {
constexpr double step = 1.0/120.0;
struct Flight {
    JSBSim::FGFDMExec fdm;
    double remainder=0;
    std::string error;
};
template<class F> int guarded(Flight* flight,F operation) noexcept {
    if (!flight) return 0;
    try { operation(); flight->error.clear(); return 1; }
    catch(const std::exception& e) { flight->error=e.what(); }
    catch(...) { flight->error="Unknown flight dynamics failure"; }
    return 0;
}
}
unsigned ocp_fdm_abi(void) { return OCP_FDM_ABI; }
OCPFlightHandle ocp_fdm_create(const char* resources,const char* model,
    const OCPFlightInitial* initial,char* error,size_t capacity) {
    try {
        if (!resources || !model || !initial || initial->size!=sizeof(*initial))
            throw std::invalid_argument("Invalid flight initialization ABI");
        const double values[]={initial->latitude_deg,initial->longitude_deg,initial->terrain_ft,
            initial->altitude_ft,initial->heading_deg,initial->airspeed_kts};
        for(double value:values) if(!std::isfinite(value)) throw std::invalid_argument("Nonfinite initial condition");
        if(std::abs(initial->latitude_deg)>90 || std::abs(initial->longitude_deg)>180 || initial->airspeed_kts<0)
            throw std::invalid_argument("Initial condition out of range");
        auto flight=std::make_unique<Flight>();
        auto& fdm=flight->fdm;
        fdm.SetDebugLevel(0);
        fdm.SetRootDir(SGPath(resources));
        fdm.SetAircraftPath(SGPath("aircraft"));
        fdm.SetEnginePath(SGPath("engine"));
        fdm.SetSystemsPath(SGPath("systems"));
        fdm.Setdt(step);
        if(!fdm.LoadModel(model)) throw std::runtime_error("Cannot load aircraft model");
        fdm.DisableOutput();
        auto ic=fdm.GetIC();
        ic->SetGeodLatitudeDegIC(initial->latitude_deg);
        ic->SetLongitudeDegIC(initial->longitude_deg);
        ic->SetTerrainElevationFtIC(initial->terrain_ft);
        ic->SetAltitudeASLFtIC(initial->altitude_ft);
        ic->SetPsiDegIC(initial->heading_deg);
        ic->SetVcalibratedKtsIC(initial->airspeed_kts);
        if(!fdm.RunIC()) throw std::runtime_error("Cannot initialize flight model");
        fdm.SetPropertyValue("fcs/mixture-cmd-norm",1);
        fdm.SetPropertyValue("fcs/throttle-cmd-norm",.1);
        if(initial->engine_running) fdm.GetPropulsion()->InitRunning(-1);
        if(error && capacity) error[0]=0;
        return flight.release();
    } catch(const std::exception& e) { if(error && capacity) std::snprintf(error,capacity,"%s",e.what()); }
      catch(...) { if(error && capacity) std::snprintf(error,capacity,"Unknown initialization failure"); }
    return nullptr;
}
void ocp_fdm_destroy(OCPFlightHandle handle) { delete static_cast<Flight*>(handle); }
int ocp_fdm_step(OCPFlightHandle handle,double seconds) {
    auto* flight=static_cast<Flight*>(handle);
    if(!flight || !std::isfinite(seconds) || seconds<0) return -1;
    int count=0;
    if(!guarded(flight,[&] {
        flight->remainder+=std::min(seconds,.25);
        while(flight->remainder+1e-12>=step) {
            if(!flight->fdm.Run()) throw std::runtime_error("Flight dynamics step failed");
            flight->remainder-=step; ++count;
        }
    })) return -1;
    return count;
}
int ocp_fdm_set(OCPFlightHandle handle,const char* property,double value) {
    auto* flight=static_cast<Flight*>(handle);
    if(!property || !std::isfinite(value)) return 0;
    return guarded(flight,[&] {
        if(!flight->fdm.GetPropertyManager()->HasNode(property)) throw std::invalid_argument("Unknown flight property");
        flight->fdm.SetPropertyValue(property,value);
    });
}
int ocp_fdm_get(OCPFlightHandle handle,const char* property,double* value) {
    auto* flight=static_cast<Flight*>(handle);
    if(!property || !value) return 0;
    return guarded(flight,[&] {
        if(!flight->fdm.GetPropertyManager()->HasNode(property)) throw std::invalid_argument("Unknown flight property");
        *value=flight->fdm.GetPropertyValue(property);
    });
}
int ocp_fdm_state(OCPFlightHandle handle,OCPFlightState* state) {
    if(!state || state->size!=sizeof(*state)) return 0;
    auto* flight=static_cast<Flight*>(handle);
    return guarded(flight,[&] {
        auto p=flight->fdm.GetPropagate();
        state->latitude_deg=p->GetGeodLatitudeDeg();
        state->longitude_deg=p->GetLongitudeDeg();
        state->altitude_ft=p->GetAltitudeASL();
        state->roll_deg=p->GetEulerDeg(1);
        state->pitch_deg=p->GetEulerDeg(2);
        state->heading_deg=p->GetEulerDeg(3);
        state->airspeed_kts=flight->fdm.GetPropertyValue("velocities/vc-kts");
        state->vertical_speed_fps=flight->fdm.GetPropertyValue("velocities/h-dot-fps");
        state->rpm=flight->fdm.GetPropertyValue("propulsion/engine/engine-rpm");
        state->elapsed_seconds=flight->fdm.GetSimTime();
    });
}
const char* ocp_fdm_error(OCPFlightHandle handle) {
    auto* flight=static_cast<Flight*>(handle);
    return flight?flight->error.c_str():"No flight model";
}
