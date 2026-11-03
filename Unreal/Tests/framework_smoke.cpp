// SPDX-License-Identifier: MIT
#include "FlightDynamics.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(int argc,char**argv){
    if(argc!=2)return 2;
    OCPFlightInitial initial{sizeof(OCPFlightInitial),50.83138094,-.29991299,7,1500,20,90,1};
    char error[1024]{};
    auto flight=ocp_fdm_create(argv[1],"ocp172p",&initial,error,sizeof error);
    if(!flight){std::puts(error);return 3;}
    assert(ocp_fdm_abi()==OCP_FDM_ABI);
    assert(ocp_fdm_set(flight,"fcs/elevator-cmd-norm",-.3));
    for(int i=0;i<120;++i)assert(ocp_fdm_step(flight,1.0/120)==1);
    OCPFlightState state{};state.size=sizeof state;
    assert(ocp_fdm_state(flight,&state));assert(std::isfinite(state.altitude_ft));assert(state.rpm>0);
    ocp_fdm_destroy(flight);
    std::puts("PASS: dynamically linked JSBSim C172 flight step");
}
