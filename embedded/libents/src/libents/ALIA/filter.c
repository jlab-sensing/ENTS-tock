#include "filter.h"

#include <math.h>
#include <stdio.h>
#include <../util/time.h>

void welford_init(WelfordState *state) {
	state->head = 0;
  	state->count = 0;
  	state->mean = 0.0;
  	state->M2 = 0.0;
}

void welford_add(WelfordState *state, double x){
	state->sensorMeasurements[state->head] = x;
	state->head = (state->head + 1) % ALIA_STD_DEV_WINDOW_SAMPLES;
	state->count +=1;
	double old_mean = state->mean;
	state->mean += (x - state->mean) / state->count;
	state->M2 += (x - old_mean) * (x - state->mean);
}

void welford_remove(WelfordState *state){
	state->count -= 1;
        double old_mean = state->mean;
        state->mean -= (state->sensorMeasurements[state->head] - state->mean) / state->count;
        state->M2 -= (state->sensorMeasurements[state->head] - old_mean) * (state->sensorMeasurements[state->head] - state->mean);
}

void welford_push(WelfordState *state, double x) {
	if(state->count < ALIA_STD_DEV_WINDOW_SAMPLES){
		welford_add(state,x);
	}
	else{
		welford_remove(state);
		welford_add(state,x);
	}
}

double welford_get_stddev(const WelfordState *state){
	if(state->count < 2){
		return 0.0;
	}
	return sqrt(welford_get_variance(state));
}

double welford_get_mean(const WelfordState *state){
	return state->mean;
}

double welford_get_variance(const WelfordState *state){
	if(state->count < 2){
		return 0.0;
	}
	return state->M2 / (state->count-1);
}

bool welford_window_is_full(const WelfordState *state){
	if (state->count < ALIA_STD_DEV_WINDOW_SAMPLES){
		return false;
	}
	return true;
}

double backoff(HeartbeatState *heartbeatState, ALIAUserConfig *config, uint32_t now){
	double calm_hours = 0;

	if (!heartbeatState->has_logged){
		calm_hours = 0.0;
	}
	else{
		calm_hours = (now - heartbeatState->last_event_ts) / 3600;}
	if(config->base_heartbeat_hours * pow(2, (calm_hours / config->doubling_hours)) < config->max_heartbeat_hours){
		return config->base_heartbeat_hours * pow(2, (calm_hours / config->doubling_hours));
	}
	return config->max_heartbeat_hours;

}

bool should_log(double data, WelfordState *state, HeartbeatState *heartbeatState, RunState *runState, ALIAUserConfig *config){
	// if first value send it
	bool event_fired;
	if (state->count > 2){
		size_t last_index = (state->head + ALIA_STD_DEV_WINDOW_SAMPLES - 1) % ALIA_STD_DEV_WINDOW_SAMPLES;
		double deviation = fabs(data - state->sensorMeasurements[last_index]);
		double threshold = (welford_get_stddev(state) * config->event_delta_threshold);
		event_fired = deviation >= threshold;
	}
	else{
		event_fired = false;
	}
	uint32_t time = epoch();
	bool heartbeat_fired;
	if (!heartbeatState->has_logged){
		heartbeat_fired = true;
	}
	else{
		double elapsed_hours = (time - heartbeatState->last_event_ts) / 3600;
		double interval = backoff(heartbeatState, config, time);	
		heartbeat_fired = elapsed_hours >= interval;
	}
	bool should_transmit = heartbeat_fired || event_fired;

	welford_push(state, data);

	if (should_transmit){
		heartbeatState->last_event_ts = time;
		heartbeatState->has_logged = true;
	}
	else{
		runState->run_count +=1;
	}

	return should_transmit;



}


