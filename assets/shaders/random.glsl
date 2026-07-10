#ifndef FPSPARTY_RANDOM_GLSL
#define FPSPARTY_RANDOM_GLSL

struct Random {
  uint state;
};

uint lowbias32(uint x) {
  x ^= x >> 16;
  x *= 0x21f0aaadu;
  x ^= x >> 15;
  x *= 0xd35a2d97u;
  x ^= x >> 15;
  return x;
}

uint pcg(uint state) {
  uint word = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

uint random_uint(inout Random random) {
  random.state = random.state * 747796405u + 2891336453u;
  return pcg(random.state);
}

float random_float(inout Random random) {
  return (random_uint(random) >> 8) / 16777216.0;
}

#endif
