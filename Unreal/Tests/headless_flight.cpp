// SPDX-License-Identifier: MIT
#include "FlightSession.h"
#include <dlfcn.h>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <iostream>
namespace { volatile std::sig_atomic_t stopped=0; void stop(int){stopped=1;} }
int main(int argc,char** argv){
    if(argc!=3)return 2;
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){std::cerr<<dlerror()<<'\n';return 3;}
    int status=0;
    try{
        ocp::FlightAPI api;
#define LOAD(name) api.name=reinterpret_cast<decltype(api.name)>(dlsym(library,"ocp_fdm_" #name))
        LOAD(abi);LOAD(create);LOAD(destroy);LOAD(step);LOAD(set);LOAD(get);LOAD(state);LOAD(error);
#undef LOAD
        ocp::FlightSession session(api);session.start(argv[2]);
        std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
        auto previous=std::chrono::steady_clock::now();
        std::cout<<"READY JSBSim + Fly2Gether X-Plane UDP 49000"<<std::endl;
        while(!stopped){
            auto now=std::chrono::steady_clock::now();
            session.tick(std::chrono::duration<double>(now-previous).count());previous=now;
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';status=1;}
    dlclose(library);return status;
}
