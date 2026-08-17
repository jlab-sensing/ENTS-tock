#pragma once
#ifndef ALIA_H
#define ALIA_H
#define ALIA_STD_DEV_WINDOW_SAMPLES 144
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef struct ALIAUserConfig {
	uint32_t sample_rate;	
	double sensor_resolution;
	uint32_t event_delta_threshold;
	uint32_t std_dev_window_hours;
	uint32_t base_heartbeat_hours;
	uint32_t doubling_hours;
	uint32_t max_heartbeat_hours;
	uint32_t num_startup_samples;
} ALIAUserConfig;

typedef struct {
    float    value;        // the data value being reported
    uint32_t runLength;     // how many samples this value/run represents
    uint32_t timestamp;     // epoch() at time of transmission
} ALIATransmitRecord;

//calculate number of statup samples needed for stdDevWindowHours 
static inline void  numSamplesInStartup(struct ALIAUserConfig *cfg){
	cfg->numStartupSamples = (cfg->stdDevWindowHours*3600)/cfg->sampleRate;
}

//run length encoding struct to keep track of run length
typedef struct RunState{
	uint32_t run_count;
} RunState;

//heartbeat struct to keep track of last event that was transmitted
typedef struct HeartbeatState{
	time_t last_event_ts;
	bool has_logged;
} HeartbeatState;

//initictes a struct for calculating the std deviation over a rolling window of the past n values
typedef struct {
    	size_t head;
	size_t count;
	double mean;
	double M2;
	double sensorMeasurements[ALIA_STD_DEV_WINDOW_SAMPLES];
} WelfordState;

//initializes the struct for calculating std dev, sets everything to zeroes
void welford_init(WelfordState *state);

//pushes a new value to the rolling window stats (adds new value and removes oldst value)
void welford_push(WelfordState *state, double x);

//adds a new value to the rolling window stats
void welford_add(WelfordState *state, double x);

//removes the last value from the rolling window stats
void wleford_remove(WelfordState *state);

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
bool should_log(double data, WelfordState *state, HeartbeatState *heartbeatState, RunState *runState, ALIAUserConfig *config);

//backoff function that handles heartbeat doubling logic
double backoff(HeartbeatState *heartbeatState, ALIAUserConfig *config, uint32_t now);


#endif
