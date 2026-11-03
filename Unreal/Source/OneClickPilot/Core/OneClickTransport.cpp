// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#include "OneClickTransport.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
void closeSocket(int& fd) { if (fd >= 0) ::close(fd); fd = -1; }
int boundSocket(int type, int port) {
    int fd = socket(AF_INET, type, 0);
    if (fd < 0) throw std::runtime_error("OneClickTransport socket creation failed");
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY); address.sin_port = htons(port);
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0 || bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof address) < 0 ||
        (type == SOCK_STREAM && listen(fd, 8) < 0)) {
        ::close(fd); throw std::runtime_error("OneClickTransport cannot bind port " + std::to_string(port));
    }
    return fd;
}
bool starts(const std::string& value, const std::string& prefix) { return value.rfind(prefix, 0) == 0; }
bool validPath(const std::string& p) {
    return !p.empty() && p[0] == '/' && p.size() <= 512 && p.find("..") == std::string::npos &&
        p.find_first_not_of("/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_[]") == std::string::npos;
}
std::string cockpitPath(std::string p) {
    for (const auto& prefix : {std::string("/controls/engines/engine/"), std::string("/controls/anti-ice/engine/")})
        if (starts(p, prefix)) p.replace(prefix.size()-1, 1, "[0]/");
    if (p == "/controls/engines/engine[0]/throttle") return "/controls/engines/current-engine/throttle";
    if (p == "/controls/engines/engine[0]/mixture") return "/controls/engines/current-engine/mixture";
    if (p == "/controls/anti-ice/engine[0]/carb-heat") return "/controls/engines/current-engine/carb-heat";
    if (p == "/controls/engines/engine[0]/starter") return "/controls/switches/starter";
    if (p == "/controls/engines/engine[0]/magnetos") return "/controls/switches/magnetos";
    if (p == "/controls/electric/battery-switch") return "/controls/switches/master-bat";
    if (p == "/controls/electric/engine[0]/generator") return "/controls/switches/master-alt";
    return p;
}
}
struct OneClickTransport::State {
    struct Client { int fd; std::string input, output; bool starter = false; Clock::time_point refreshed; };
    ocp::PropertyStore* root;
    Command command;
    int udp = -1, tcp = -1;
    std::vector<Client> clients;
    State(ocp::PropertyStore* props, Command run) : root(props), command(std::move(run)) {}
    ~State() { stop(); }
    void stop() {
        for (auto& c : clients) drop(c);
        clients.clear(); closeSocket(udp); closeSocket(tcp);
    }
    void drop(Client& c) {
        if (c.starter) root->setBoolValue("/controls/switches/starter", false);
        closeSocket(c.fd);
    }
    void count(const char* path) { auto p = root->getNode(path, true); p->setIntValue(p->getIntValue()+1); }
    bool packet(std::string text) {
        // Validate the entire datagram before changing any controls.
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        std::array<double,20> values{}; size_t begin = 0;
        for (size_t i=0; i<values.size(); ++i) {
            size_t end = text.find(',', begin);
            if ((i==19) != (end==std::string::npos)) return false;
            const std::string field = text.substr(begin, end==std::string::npos ? end : end-begin);
            if (field.empty()) return false;
            char* tail = nullptr; errno = 0;
            values[i] = std::strtod(field.c_str(), &tail);
            if (errno || tail != field.c_str()+field.size() || !std::isfinite(values[i])) return false;
            begin = end+1;
        }
        const char* axes[] = {"aileron","elevator","rudder","elevator-trim"};
        for (int i=0; i<4; ++i) root->setDoubleValue((std::string("/controls/flight/")+axes[i]).c_str(),std::clamp(values[i],-1.0,1.0));
        const char* engine[] = {"throttle","propeller-pitch","mixture"};
        for (int i=0; i<12; ++i) root->setDoubleValue(("/controls/engines/engine["+std::to_string(i/3)+"]/"+engine[i%3]).c_str(),std::clamp(values[4+i],0.0,1.0));
        root->setDoubleValue("/controls/engines/current-engine/throttle",std::clamp(values[4],0.0,1.0));
        root->setDoubleValue("/controls/engines/current-engine/mixture",std::clamp(values[6],0.0,1.0));
        root->setDoubleValue("/controls/gear/brake-left",std::clamp(values[16],0.0,1.0));
        root->setDoubleValue("/controls/gear/brake-right",std::clamp(values[17],0.0,1.0));
        if (values[18] != 0) root->setBoolValue("/controls/gear/gear-down",!root->getBoolValue("/controls/gear/gear-down"));
        root->setDoubleValue("/controls/flight/flaps",std::clamp(root->getDoubleValue("/controls/flight/flaps")+values[19],0.0,1.0));
        count("/sim/fly2gether/received-packets"); return true;
    }
    bool line(Client& c, const std::string& text) {
        std::istringstream in(text); std::string verb,path; in>>verb>>path;
        if (verb.empty() || verb=="data") return true;
        if (verb=="quit") return false;
        if (verb=="get" && validPath(path)) {
            auto p=root->getNode(path.c_str(),false);
            std::ostringstream value;
            if (p) {
                value<<p->getStringValue();
            }
            std::string result=value.str(); std::replace(result.begin(),result.end(),'\n',' '); std::replace(result.begin(),result.end(),'\r',' ');
            c.output+=result+"\r\n";
        } else if (verb=="set" && validPath(path) && (starts(path,"/controls/") || starts(path,"/instrumentation/") || starts(path,"/autopilot/") || starts(path,"/sim/"))) {
            std::string value; std::getline(in>>std::ws,value); path=cockpitPath(path);
            root->getNode(path.c_str(),true)->setStringValue(value.c_str());
            if (path=="/controls/gear/brake-parking") root->setBoolValue("/sim/oneclickpilot/parking-brake-manual",true);
            if (path=="/controls/switches/starter") { c.starter=root->getBoolValue(path.c_str()); c.refreshed=Clock::now(); }
            count("/sim/fly2gether/custom-received");
        } else if (verb=="run" && starts(path,"oneclickpilot-") && path.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")==std::string::npos) {
            if (!command(path)) c.output+="ERR command\r\n";
            else count("/sim/fly2gether/custom-received");
        } else c.output+="ERR unsupported\r\n";
        return c.output.size()<=65536;
    }
    void poll() {
        char buffer[4096];
        for (int i=0;i<128;++i) {
            const auto size=recv(udp,buffer,sizeof buffer,0);
            if (size<0) break;
            if (size>0 && size<static_cast<ssize_t>(sizeof buffer) && !packet(std::string(buffer,size))) count("/sim/fly2gether/rejected-packets");
        }
        for (int i=0;i<8;++i) {
            int fd=accept(tcp,nullptr,nullptr); if(fd<0) break;
            if(clients.size()>=8 || fcntl(fd,F_SETFL,O_NONBLOCK)<0) {::close(fd);continue;}
            int yes=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof yes);
            clients.push_back({fd,{},{},false,Clock::now()});
        }
        for (auto& c:clients) {
            bool live=true;
            // Bounded work per frame, with persistent buffers for fragmented lines.
            for (int i=0;i<8 && live;++i) {
                auto size=recv(c.fd,buffer,sizeof buffer,0);
                if (size==0) {live=false;break;}
                if (size<0) {if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) live=false;break;}
                c.input.append(buffer,size);
                if(c.input.size()>32768 || c.input.find('\0')!=std::string::npos) {live=false;break;}
                size_t end;
                while((end=c.input.find('\n'))!=std::string::npos) {
                    std::string text=c.input.substr(0,end);c.input.erase(0,end+1);
                    if(!text.empty() && text.back()=='\r')text.pop_back();
                    if(text.size()>4096 || !line(c,text)) {live=false;break;}
                }
            }
            if(c.starter && Clock::now()-c.refreshed>std::chrono::seconds(2)) {
                root->setBoolValue("/controls/switches/starter",false);c.starter=false;
            }
            if(live && !c.output.empty()) {
                auto sent=send(c.fd,c.output.data(),c.output.size(),0);
                if(sent>0)c.output.erase(0,sent);
                else if(sent<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)live=false;
            }
            if(!live)drop(c);
        }
        clients.erase(std::remove_if(clients.begin(),clients.end(),[](const Client& c){return c.fd<0;}),clients.end());
    }
};
OneClickTransport::OneClickTransport(ocp::PropertyStore* root, Command command) : state(new State(root,std::move(command))) {}
OneClickTransport::~OneClickTransport() = default;
void OneClickTransport::init() {
    state->stop();
    try { state->udp=boundSocket(SOCK_DGRAM,15501);state->tcp=boundSocket(SOCK_STREAM,15401); }
    catch (...) {state->stop();throw;}

}
void OneClickTransport::update(double) {if(state->udp>=0 && state->tcp>=0)state->poll();}
void OneClickTransport::shutdown() {state->stop();}
