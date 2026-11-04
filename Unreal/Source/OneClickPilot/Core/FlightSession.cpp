// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#include "FlightSession.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
namespace ocp {
namespace {
void validateFlightState(const OCPFlightState& state) {
    const double values[] = {
        state.latitude_deg, state.longitude_deg, state.altitude_ft,
        state.roll_deg, state.pitch_deg, state.heading_deg, state.airspeed_kts,
        state.vertical_speed_fps, state.rpm, state.elapsed_seconds
    };
    if (!std::all_of(std::begin(values), std::end(values), [](double value) { return std::isfinite(value); }))
        throw std::runtime_error("JSBSim returned non-finite aircraft state");
}
}
FlightSession::FlightSession(FlightAPI functions):mixer(&properties),api(functions),transport(&mixer,[this](const std::string& name){return command(name);}) {}
FlightSession::~FlightSession(){stop();}
void FlightSession::start(const std::string& resources,bool airborne) {
    stop();
    if(!api.valid())throw std::runtime_error("Invalid JSBSim framework ABI");
    properties=PropertyStore{};
    instructor.reset();
    state={};state.size=sizeof state;
    for(const char* p:{"/controls/switches/master-bat","/controls/switches/master-alt","/controls/switches/master-avionics","/controls/gear/gear-down","/consumables/fuel/tank[0]/selected","/consumables/fuel/tank[1]/selected"})properties.setBoolValue(p,true);
    for(const char* path:{"/controls/flight/aileron","/controls/flight/elevator","/controls/flight/rudder","/controls/flight/elevator-trim","/controls/flight/flaps","/controls/gear/brake-left","/controls/gear/brake-right","/controls/switches/starter","/controls/lighting/landing-lights","/controls/lighting/taxi-light","/controls/lighting/nav-lights","/controls/lighting/beacon","/controls/lighting/strobe","/controls/anti-ice/pitot-heat","/controls/engines/current-engine/carb-heat","/controls/engines/active-engine","/fdm/jsbsim/fuel/tank","/sim/current-view/heading-offset-deg","/sim/current-view/pitch-offset-deg","/sim/current-view/roll-offset-deg","/sim/fly2gether/received-packets","/sim/fly2gether/rejected-packets","/sim/fly2gether/custom-received"})properties.setDoubleValue(path,0);
    for(int i=0;i<4;++i)for(const char* field:{"throttle","mixture","propeller-pitch"})properties.setDoubleValue(("/controls/engines/engine["+std::to_string(i)+"]/"+field).c_str(),0);
    properties.setDoubleValue("/instrumentation/comm/frequencies/standby-mhz",118.5);
    properties.setDoubleValue("/instrumentation/nav/frequencies/standby-mhz",108);
    properties.setDoubleValue("/instrumentation/transponder/inputs/knob-mode",1);
    properties.setDoubleValue("/controls/switches/magnetos",3);
    properties.setDoubleValue("/controls/engines/current-engine/mixture",1);
    properties.setDoubleValue("/controls/engines/current-engine/throttle",airborne?.7:.1);
    properties.setDoubleValue("/controls/engines/engine[0]/mixture",1);
    properties.setDoubleValue("/controls/engines/engine[0]/throttle",airborne?.7:.1);
    properties.setBoolValue("/controls/gear/brake-parking",!airborne);
    properties.setDoubleValue("/instrumentation/altimeter/setting-inhg",29.92);
    properties.setStringValue("/sim/rendering/renderer","unreal-engine-5");
    OCPFlightInitial initial{sizeof(OCPFlightInitial),50.83138094,-.29991299,7,airborne?1500.0:11.6,20,airborne?90.0:0.0,1};
    char error[1024]{};
    flight=api.create(resources.c_str(),"ocp172p",&initial,error,sizeof error);
    if(!flight)throw std::runtime_error(error);
    try{transport.init();}catch(...){api.destroy(flight);flight=nullptr;throw;}
    if(!api.state(flight,&state))throw std::runtime_error(api.error(flight));
    validateFlightState(state);
    properties.setDoubleValue("/autopilot/settings/heading-bug-deg",state.heading_deg);
    properties.setBoolValue("/sim/oneclickpilot/ready",true);
}
void FlightSession::stop(){transport.shutdown();properties.setBoolValue("/sim/oneclickpilot/ready",false);if(flight){api.destroy(flight);flight=nullptr;}}
void FlightSession::tick(double seconds) {
    if(!flight)return;
    transport.update(seconds);
    auto value=[this](const char* path){return properties.getDoubleValue(path);};
    const double parking=value("/controls/gear/brake-parking");
    double leftMainContact = 0, rightMainContact = 0;
    api.get(flight, "gear/unit[1]/WOW", &leftMainContact);
    api.get(flight, "gear/unit[2]/WOW", &rightMainContact);
    const bool mainGearOnGround = leftMainContact > .5 || rightMainContact > .5;
    mixer.applyInstructorCommand(instructor.update(state, !parking, seconds, mainGearOnGround));
    properties.setBoolValue("/controls/gear/gear-down",true); // C172 fixed landing gear.
    auto set=[this](const char* path,double v){api.set(flight,path,v);};
    const char* axes[]={"aileron","elevator","rudder","elevator-trim"};
    const char* fcs[]={"fcs/aileron-cmd-norm","fcs/elevator-cmd-norm","fcs/rudder-cmd-norm","fcs/pitch-trim-cmd-norm"};
    for(int i=0;i<4;++i)set(fcs[i],std::clamp(value((std::string("/controls/flight/")+axes[i]).c_str()),-1.0,1.0));
    // Rudder pedals also steer the C172 nosewheel through a separate JSBSim input.
    set("fcs/steer-cmd-norm", std::clamp(value("/controls/flight/rudder"), -1.0, 1.0));
    set("fcs/flap-cmd-norm",std::clamp(value("/controls/flight/flaps"),0.0,1.0));
    set("fcs/throttle-cmd-norm",std::clamp(value("/controls/engines/current-engine/throttle"),0.0,1.0));
    set("fcs/mixture-cmd-norm",std::clamp(value("/controls/engines/current-engine/mixture"),0.0,1.0));

    set("fcs/left-brake-cmd-norm",std::clamp(std::max(parking,value("/controls/gear/brake-left")),0.0,1.0));
    set("fcs/right-brake-cmd-norm",std::clamp(std::max(parking,value("/controls/gear/brake-right")),0.0,1.0));
    set("propulsion/magneto_cmd",value("/controls/switches/magnetos"));
    set("propulsion/starter_cmd",properties.getBoolValue("/controls/switches/master-bat") && properties.getBoolValue("/controls/switches/starter")?1:0);
    for(int tank=0;tank<2;++tank)set(("propulsion/tank["+std::to_string(tank)+"]/priority").c_str(),value(("/consumables/fuel/tank["+std::to_string(tank)+"]/selected").c_str())?1:0);
    if(api.step(flight,seconds)<0 || !api.state(flight,&state))throw std::runtime_error(api.error(flight));
    validateFlightState(state);
    properties.setDoubleValue("/position/latitude-deg",state.latitude_deg);
    properties.setDoubleValue("/position/longitude-deg",state.longitude_deg);
    properties.setDoubleValue("/position/altitude-ft",state.altitude_ft);
    double pressureAltitude = state.altitude_ft;
    const bool hasPressureAltitude = api.get(flight, "atmosphere/pressure-altitude", &pressureAltitude) &&
                                     std::isfinite(pressureAltitude);
    // Fall back to geometric altitude if the flight model has no pressure reading.
    const double setting = std::clamp(value("/instrumentation/altimeter/setting-inhg"), 28.0, 31.0);
    const double pressureRatio = std::pow(1.0 - (hasPressureAltitude ? pressureAltitude : state.altitude_ft) / 145442.2, 5.25588);
    const double indicatedAltitude = 145442.2 * (1.0 - std::pow(pressureRatio * 29.92 / setting, 1.0 / 5.25588));
    properties.setDoubleValue("/instrumentation/altimeter/indicated-altitude-ft", indicatedAltitude);
    properties.setDoubleValue("/orientation/roll-deg",state.roll_deg);
    properties.setDoubleValue("/orientation/pitch-deg",state.pitch_deg);
    properties.setDoubleValue("/orientation/heading-deg",state.heading_deg);
    properties.setDoubleValue("/orientation/heading-magnetic-deg",state.heading_deg); // Magnetic variation not yet ported.
    properties.setDoubleValue("/velocities/airspeed-kt",state.airspeed_kts);
    properties.setDoubleValue("/instrumentation/vertical-speed-indicator/indicated-speed-fpm", state.vertical_speed_fps * 60);
    double groundspeed=0;api.get(flight,"velocities/vg-fps",&groundspeed);
    properties.setDoubleValue("/velocities/groundspeed-kt",groundspeed*.5924838013);
    for(const char* path:{"fcs/aileron-cmd-norm","fcs/elevator-cmd-norm","fcs/rudder-cmd-norm","fcs/pitch-trim-cmd-norm","fcs/throttle-cmd-norm","fcs/mixture-cmd-norm","fcs/steer-cmd-norm","fcs/steer-pos-deg","fcs/flap-cmd-norm","fcs/flap-pos-deg","fcs/flap-pos-norm","fcs/left-brake-cmd-norm","fcs/right-brake-cmd-norm","propulsion/magneto_cmd","propulsion/starter_cmd"}){
        double output=0;if(api.get(flight,path,&output))properties.setDoubleValue((std::string("/fdm/jsbsim/")+path).c_str(),output);
    }
    // Approximate the main bus with an ideal battery and an RPM-dependent alternator.
    const bool alternatorOnline = properties.getBoolValue("/controls/switches/master-alt") && state.rpm >= 1000;
    const bool mainBusPowered = alternatorOnline || properties.getBoolValue("/controls/switches/master-bat");
    properties.setBoolValue("/systems/electrical/alternator-online", alternatorOnline);
    properties.setBoolValue("/systems/electrical/main-bus-powered", mainBusPowered);
    const bool avionicsPowered = mainBusPowered && properties.getBoolValue("/controls/switches/master-avionics");
    properties.setBoolValue("/systems/electrical/avionics-bus-powered", avionicsPowered);
    properties.setBoolValue("/instrumentation/comm/powered", avionicsPowered);
    properties.setBoolValue("/instrumentation/nav/powered", avionicsPowered);
    const double transponderMode = value("/instrumentation/transponder/inputs/knob-mode");
    properties.setDoubleValue("/instrumentation/transponder/outputs/operating-mode",
                              avionicsPowered ? transponderMode : 0);
    const bool altitudeReporting = avionicsPowered && transponderMode == 5 && hasPressureAltitude;
    properties.setBoolValue("/instrumentation/transponder/outputs/altitude-reporting", altitudeReporting);
    properties.setDoubleValue("/instrumentation/transponder/outputs/pressure-altitude-ft",
                              altitudeReporting ? pressureAltitude : 0);
    properties.setBoolValue("/systems/electrical/pitot-heat-powered",
                            mainBusPowered && properties.getBoolValue("/controls/anti-ice/pitot-heat"));
    for (int tank = 0; tank < 2; ++tank) {
        const std::string index = std::to_string(tank);
        double pounds = 0;
        if (api.get(flight, ("propulsion/tank[" + index + "]/contents-lbs").c_str(), &pounds))
            properties.setDoubleValue(("/consumables/fuel/tank[" + index + "]/level-lbs").c_str(), pounds);
    }
    double ambientRankine = 0;
    if (api.get(flight, "atmosphere/T-R", &ambientRankine) && std::isfinite(ambientRankine))
        properties.setDoubleValue("/instrumentation/outside-air-temperature/degc", ambientRankine * (5.0 / 9.0) - 273.15);
    double oilPressure = 0;
    if (api.get(flight, "propulsion/engine[0]/oil-pressure-psi", &oilPressure))
        properties.setDoubleValue("/engines/engine[0]/oil-pressure-psi", oilPressure);
    double oilTemperature = 0;
    if (api.get(flight, "propulsion/engine[0]/oil-temperature-degF", &oilTemperature))
        properties.setDoubleValue("/engines/engine[0]/oil-temperature-degf", oilTemperature);
    properties.setDoubleValue("/engines/engine[0]/rpm",state.rpm);
    properties.setDoubleValue("/controls/engines/engine[0]/throttle",value("/controls/engines/current-engine/throttle"));
    properties.setDoubleValue("/controls/engines/engine[0]/mixture",value("/controls/engines/current-engine/mixture"));
}
bool FlightSession::command(const std::string& name) {
    if(name=="oneclickpilot-parking-brake") {properties.setBoolValue("/controls/gear/brake-parking",!properties.getBoolValue("/controls/gear/brake-parking"));properties.setBoolValue("/sim/oneclickpilot/parking-brake-manual",true);return true;}
    if(name=="oneclickpilot-baro-up" || name=="oneclickpilot-baro-down") {const char* p="/instrumentation/altimeter/setting-inhg";properties.setDoubleValue(p,std::clamp(properties.getDoubleValue(p)+(name=="oneclickpilot-baro-up"?.01:-.01),28.0,31.0));return true;}
    if(name=="oneclickpilot-heading-sync") {properties.setDoubleValue("/autopilot/settings/heading-bug-deg",state.heading_deg);return true;}
    if(name.rfind("oneclickpilot-fuel-",0)==0) {
        const auto choice=name.substr(std::string("oneclickpilot-fuel-").size());
        if(choice!="left" && choice!="right" && choice!="both" && choice!="off")return false;
        properties.setBoolValue("/consumables/fuel/tank[0]/selected",choice=="left" || choice=="both");
        properties.setBoolValue("/consumables/fuel/tank[1]/selected",choice=="right" || choice=="both");return true;
    }
    if(name=="oneclickpilot-ap-disconnect") {api.set(flight,"ap/attitude_hold",0);api.set(flight,"ap/heading_hold",0);properties.setBoolValue("/autopilot/kap140/settings/ap-disc",true);return true;}
    // Primer/carb icing/electrical/radio systems require their own migration.
    return false;
}
}
