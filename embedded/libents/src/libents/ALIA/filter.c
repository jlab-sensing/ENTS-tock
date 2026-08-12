#include "filter.h"

#include <math.h>
#include <stdio.h>

void welford_init(WelfordState *state) {
  state->count = 0;
  state->mean = 0.0;
  state->M2 = 0.0;
}

void welford_update(WelfordState *state, double x) {
  int n = state->count;

  state->count += 1;
  double delta = x - state->mean;
  state->mean += delta / state->count;

  double delta2 = x - state->mean;
  state->M2 += delta * delta2;
}
