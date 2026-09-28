uniform ivec3 uN;        // grid cells (x, y up, z)
uniform vec3 uSize;      // domain size in metres
uniform vec3 uExt;       // extinction (1/m) per unit smoke, dust, hot air
uniform sampler1D uAtm;  // background T, rho, theta, p vs height
const float G = 9.81, PI = 3.14159265, SIGMA = 5.670374e-8;
vec4 atm(float y) { return texture(uAtm, clamp(y / uSize.y, 0.0, 1.0)); }
float cellSize() { return uSize.y / float(uN.y); }
vec3 cellPos(ivec3 c) { return (vec3(c) + 0.5) / vec3(uN) * uSize - vec3(0.5 * uSize.x, 0.0, 0.5 * uSize.z); }
vec3 toUvw(vec3 p) { return (p + vec3(0.5 * uSize.x, 0.0, 0.5 * uSize.z)) / uSize; }
ivec3 clampC(ivec3 c) { return clamp(c, ivec3(0), uN - 1); }
bool inGrid(ivec3 c) { return all(greaterThanEqual(c, ivec3(0))) && all(lessThan(c, uN)); }
// scalars: x = theta' (buoyancy), y = smoke, z = dust, w = incandescent temperature excess (K)
float hotT(vec4 s, float y) { return atm(y).x + max(s.w, 0.0); }
float extinction(vec4 s, float T) { return uExt.x * s.y + uExt.y * s.z + uExt.z * smoothstep(1200.0, 3000.0, T); }
float hash(vec3 p) { p = fract(p * 0.3183099 + 0.1) * 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float noise(vec3 x) {
  vec3 i = floor(x), f = fract(x);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
             mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
// Planck's law at 610/550/465 nm (c2 = 14387.77 um K), normalised to unit luminance.
vec3 planckChroma(float T) {
  vec3 l = vec3(0.610, 0.550, 0.465);
  vec3 b = 1.0 / (l * l * l * l * l * (exp(min(14387.77 / (l * T), 60.0)) - 1.0));
  return b / dot(b, vec3(0.2126, 0.7152, 0.0722));
}
// Radiance with brightness proportional to sigma T^4 (W m^-2 sr^-1), faded below the ~800 K Draper point.
float visibleGlow(float T) { return smoothstep(800.0, 2000.0, T); }
vec3 blackbody(float T) { float T2 = T * T; return planckChroma(T) * SIGMA * T2 * T2 / PI * visibleGlow(T); }
#ifdef COMPUTE
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
ivec3 gid() { return ivec3(gl_GlobalInvocationID); }
bool outside(ivec3 c) { return any(greaterThanEqual(c, uN)); }
#endif
