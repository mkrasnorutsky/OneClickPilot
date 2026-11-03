// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OneClickPilot contributors
#pragma once
#include <stddef.h>
#include <stdint.h>
#define OCP_FDM_ABI 1
#if defined(__GNUC__)
#define OCP_API __attribute__((visibility("default")))
#else
#define OCP_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
// Every handle has one owner thread; calls on the same handle are not concurrent.
typedef void* OCPFlightHandle;
typedef struct {
    uint32_t size;
    double latitude_deg, longitude_deg, terrain_ft, altitude_ft;
    double heading_deg, airspeed_kts;
    int engine_running;
} OCPFlightInitial;
typedef struct {
    uint32_t size;
    double latitude_deg, longitude_deg, altitude_ft;
    double roll_deg, pitch_deg, heading_deg;
    double airspeed_kts, vertical_speed_fps, rpm, elapsed_seconds;
} OCPFlightState;
OCP_API unsigned ocp_fdm_abi(void);
OCP_API OCPFlightHandle ocp_fdm_create(const char* resources, const char* model,
    const OCPFlightInitial* initial, char* error, size_t error_capacity);
OCP_API void ocp_fdm_destroy(OCPFlightHandle handle);
// Fixed 120Hz stepping. Caps one call at .25s; drops excess wall-clock time.
OCP_API int ocp_fdm_step(OCPFlightHandle handle, double seconds);
OCP_API int ocp_fdm_set(OCPFlightHandle handle, const char* property, double value);
OCP_API int ocp_fdm_get(OCPFlightHandle handle, const char* property, double* value);
OCP_API int ocp_fdm_state(OCPFlightHandle handle, OCPFlightState* state);
// The error pointer is borrowed, valid until the next operation/destruction.
OCP_API const char* ocp_fdm_error(OCPFlightHandle handle);
#ifdef __cplusplus
}
#endif
