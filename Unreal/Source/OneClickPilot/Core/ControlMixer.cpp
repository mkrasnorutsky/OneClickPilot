// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#include "ControlMixer.hpp"

using namespace ocp;

ControlMixer::ControlMixer(ocp::PropertyStore* p) : root(p) {};

PropertyStore::Value* ControlMixer::getNode(const char* path,bool create) {
    return root->getNode(path, create);
}

double ControlMixer::getDoubleValue(const char* path) const {
    return root->getDoubleValue(path);
}

bool ControlMixer::getBoolValue(const char* path) const {
    return root->getBoolValue(path);
}

void ControlMixer::setDoubleValue(const char* path,double n) {
    root->setDoubleValue(path, n);
}

void ControlMixer::setBoolValue(const char* path,bool n) {
    root->setBoolValue(path,n);
}

void ControlMixer::applyInstructorCommand(CircuitInstructor::Controls controls) {
    setDoubleValue("/controls/engines/current-engine/throttle", controls.throttle);
    setDoubleValue("/controls/flight/elevator", controls.elevator);
    setDoubleValue("/controls/flight/aileron", controls.aileron);
    setDoubleValue("/controls/flight/rudder", controls.rudder);
    setDoubleValue("/controls/gear/brake-left", controls.wheelBrake);
    setDoubleValue("/controls/gear/brake-right", controls.wheelBrake);

}
