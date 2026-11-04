// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once

#include "PropertyStore.h"
#include "CircuitInstructor.hpp"

namespace ocp {

class ControlMixer {
    ocp::PropertyStore* root;
public:
    ControlMixer(ocp::PropertyStore* p);
    
    PropertyStore::Value* getNode(const char* path,bool create=false);
    double getDoubleValue(const char* path) const;
    bool getBoolValue(const char* path) const;
    
    void setDoubleValue(const char* path,double n);
    void setBoolValue(const char* path,bool n);
    
    void applyInstructorCommand(CircuitInstructor::Controls controls);
};

}
