#ifndef FPSPARTY_RANDOM_GLSL
#define FPSPARTY_RANDOM_GLSL

struct Random {
  uint state;
};

uint random_uint(inout Random random) {
  random.state = random.state * 2654435761u + 2891336453u;
  return random.state;
}

float random_float(inout Random random) {
  return (random_uint(random) >> 8) / 16777216.0;
}

#endif
