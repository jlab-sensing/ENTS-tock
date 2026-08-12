#pragma once
#ifndef ALIA_H
#define ALIA_H
#define ALIA_STD_DEV_WINDOW_SAMPLES 144
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef struct ALIAUserConfig {
	uint32_t sampleRate;	
	double sensorResolution;
	uint32_t eventDeltaThreshold;
	uint32_t stdDevWindowHours;
	uint32_t baseHeartbeatHours;
	uint32_t doublingHours;
	uint32_t maxHeartbeatHours;
	uint32_t numStartupSamples;
} ALIAUserConfig;

//calculate number of statup samples needed for stdDevWindowHours 
static inline void  numSamplesInStartup(struct ALIAUserConfig *cfg){
	cfg->numStartupSamples = (cfg->stdDevWindowHours*3600)/cfg->sampleRate;
}

//run length encoding struct to keep track of run length
typedef struct RunState{
	uint32_t runCount;
} RunState;

//heartbeat struct to keep track of last event that was transmitted
typedef struct HeartbeatState{
	time_t last_event_ts;
} HeartbeatState;

//initictes a struct for calculating the std deviation over a rolling window of the past n values
typedef struct {
    size_t head;
	size_t count;
	double mean;
	double M2;
	float sensorMeasurements[ALIA_STD_DEV_WINDOW_SAMPLES];
} WelfordState;

//initializes the struct for calculating std dev, sets everything to zeroes
void welford_init(WelfordState *state);

//adds a new value to the rolling window stats
void welford_push(WelfordState *state, double x);

//returns the standard deviation of the rolling window
double welford_get_stddev(const WelfordState *state);

//returns the mean of the rolling window
double welford_get_mean(const WelfordState *state);

//returns the variance of the rolling window
double welford_get_variance(const WelfordState *state);

//returns if the window is full
bool welford_window_is_full(const WelfordState *state);

// makes userConfig struct external to allow setting in ESP32 Wifi Config page
extern struct ALIAUserConfig global_ALIAConfig;

//main logic decider function
bool should_log(double data, double prev, HeartbeatState *heartbeatState, RunState *runState);

//backoff function that handles heartbeat doubling logic
double backoff(HeartbeatState *heartbeatState, time_t now);


#endif
