
#ifndef _SCALAR_MATH_H_
#define _SCALAR_MATH_H_

static inline uint32_t max(uint32_t a, uint32_t b) {
  return (a < b) ? b : a;
}

static inline int max(int a, int b) {
  return (a < b) ? b : a;
}

static inline float max(float a, float b) {
  return (a < b) ? b : a;
}

static inline uint32_t min(uint32_t a, uint32_t b) {
  return (a > b) ? b : a;
}

static inline int min(int a, int b) {
  return (a > b) ? b : a;
}

static inline float min(float a, float b) {
  return (a > b) ? b : a;
}

static inline float squared(float f) {
  return f * f;
}

static inline bool is_earlier(float a, float b) {
  if (a < 0) return false;
  if (b < 0) return true;
  if (a >= b) return false;
  return true;
}

static inline float earlier(float a, float b) {
  if (b < 0) return a;
  if (a < 0) return b;
  return min(a,b);
}


static inline bool non_zero(float f, float epsilon = 0.0001) {
  if (f > epsilon) return true;
  if (f < -epsilon) return true;
  return false;
}

static inline bool equals(float f1, float f2, float epsilon = 0.0001) {
  return !non_zero(f1 - f2, epsilon);
}

static inline float clamp(float f, float mini, float maxi) {
  assert(mini <= maxi);

  float r;
  r = min(f, maxi);
  r = max(r, mini);

  return r;
}

static inline float lerp(float f1, float f2, float dt) {
  float result = (1.0f - dt) * f1 + dt * f2;
  return result;
}

#endif
