// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
namespace ocp {
class PropertyStore {
public:
    class Value {
        std::string text;
    public:
        const char* getStringValue() const {return text.c_str();}
        void setStringValue(const char* value) {text=value?value:"";}
        double getDoubleValue() const {char* end=nullptr;double n=std::strtod(text.c_str(),&end);return end!=text.c_str() && !*end && std::isfinite(n)?n:0;}
        int getIntValue() const {double n=getDoubleValue();return n>2147483647?2147483647:n<-2147483648?-2147483647-1:static_cast<int>(n);}
        bool getBoolValue() const {return text=="true" || text=="on" || getDoubleValue()!=0;}
        void setDoubleValue(double n) {std::ostringstream s;s<<std::setprecision(17)<<n;text=s.str();}
        void setIntValue(int n) {text=std::to_string(n);}
    };
    Value* getNode(const char* path,bool create=false) {auto i=values.find(path);return i!=values.end()?&i->second:create?&values[path]:nullptr;}
    double getDoubleValue(const char* path) const {auto i=values.find(path);return i==values.end()?0:i->second.getDoubleValue();}
    bool getBoolValue(const char* path) const {auto i=values.find(path);return i!=values.end() && i->second.getBoolValue();}
    void setDoubleValue(const char* path,double n) {values[path].setDoubleValue(n);}
    void setBoolValue(const char* path,bool n) {values[path].setIntValue(n?1:0);}
    void setStringValue(const char* path,const char* text) {values[path].setStringValue(text);}
private:
    std::map<std::string,Value> values;
};
}
