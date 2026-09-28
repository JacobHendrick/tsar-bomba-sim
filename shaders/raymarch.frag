in vec2 vUv;
out vec4 oColor;
layout(binding = 1) uniform sampler3D uScal;
layout(binding = 2) uniform sampler3D uLight;
layout(binding = 3) uniform sampler3D uGlow;
layout(binding = 4) uniform sampler3D uDetail;
uniform mat4 uInvViewProj;
uniform vec3 uCam, uSunDir, uSun;  // uSun: sun irradiance (W/m^2)
uniform vec4 uFb, uFbParams;       // analytic fireball centre+radius; base y, T, visibility, Rmax
uniform vec4 uGlowA;               // analytic glow: power (W), height, T
uniform float uGlowLod, uCells, uGridOn, uTime;
uniform int uSteps;
float hg(float c, float g) { return (1.0 - g * g) / (4.0 * PI * pow(1.0 + g * g - 2.0 * g * c, 1.5)); }
vec3 sky(vec3 d) {
  float mu = max(dot(d, uSunDir), 0.0), e = max(d.y, 0.0);
  vec3 c = mix(vec3(0.72, 0.81, 0.93), vec3(0.12, 0.28, 0.58), pow(e, 0.42));
  c += vec3(1.0, 0.64, 0.37) * (0.35 * pow(mu, 8.0) + 1.3 * pow(mu, 128.0)) * exp(-3.0 * e);
  return 8.0 * c;
}
float sceneScale() { return uSize.x / 160000.0; }
float detailNoise(vec3 p, float lod) { return textureLod(uDetail, p, lod).a; }
// Integrate an exponential-height haze along the viewing segment.
vec3 airTransmittance(vec3 p) {
  float distanceToEye = length(p - uCam);
  float h0 = max(uCam.y, 0.0), h1 = max(p.y, 0.0);
  float delta = (h1 - h0) / 8500.0;
  float density = abs(delta) < 0.01 ? exp(-0.5 * (h0 + h1) / 8500.0)
    : (exp(-h0 / 8500.0) - exp(-h1 / 8500.0)) / delta;
  return exp(-vec3(2.2, 3.6, 6.0) * 1e-6 * distanceToEye * density);
}
vec3 aerial(vec3 radiance, vec3 p, vec3 d) {
  vec3 tr = airTransmittance(p);
  return radiance * tr + sky(d) * (1.0 - tr);
}
// Procedural landscape for visual context; the fluid boundary remains y = 0.
float terrainHeight(vec2 p) {
  float scale = sceneScale();
  // Smooth interpolation here avoids the planar facets of a low-resolution
  // noise texture when taking height derivatives for the surface normal.
  vec3 q = vec3(p.x, 1730.0 * scale, p.y) / (22000.0 * scale);
  float broad = noise(q);
  float ridge = 1.0 - abs(2.0 * noise(q * 2.7 + 17.3) - 1.0);
  float mountains = smoothstep(0.18 * uSize.x, 0.38 * uSize.x, length(p));
  return scale * (25.0 + 180.0 * broad + mountains * (1700.0 * broad * ridge * ridge));
}
float terrainHit(vec3 o, vec3 d) {
  float scale = sceneScale(), top = 2400.0 * scale;
  if (abs(d.y) < 1e-7) return 1e30;
  vec2 span = vec2(-o.y, top - o.y) / d.y;
  float lo = max(0.0, min(span.x, span.y));
  float hi = min(800000.0 * scale, max(span.x, span.y));
  if (hi <= lo) return 1e30;
  float previous = lo;
  for (int i = 0; i <= 48; ++i) {
    float t = mix(lo, hi, float(i) / 48.0);
    vec3 p = o + t * d;
    if (p.y <= terrainHeight(p.xz)) {
      float a = previous, b = t;
      for (int j = 0; j < 6; ++j) {
        float mid = 0.5 * (a + b);
        vec3 m = o + mid * d;
        if (m.y > terrainHeight(m.xz)) a = mid; else b = mid;
      }
      return 0.5 * (a + b);
    }
    previous = t;
  }
  return 1e30;
}
vec3 terrainNormal(vec2 p) {
  float e = 70.0 * sceneScale();
  return normalize(vec3(terrainHeight(p - vec2(e, 0)) - terrainHeight(p + vec2(e, 0)),
    2.0 * e, terrainHeight(p - vec2(0, e)) - terrainHeight(p + vec2(0, e))));
}
// Erode and warp the rendered boundary below the solver's cell size. World-space
// coordinates and continuous drift keep the texture stable under camera movement.
vec4 cloudSample(vec3 p, float footprint) {
  vec3 q = (p - vec3(0, 12.0 * uTime, 0)) / (16000.0 * sceneScale());
  float lod = max(0.0, log2(max(footprint, 1.0) * 128.0 / (16000.0 * sceneScale())));
  vec4 detail = textureLod(uDetail, q, lod);
  vec3 warp = (detail.rgb - 0.5) * (0.85 * cellSize());
  vec3 w = toUvw(p + warp);
  if (any(lessThan(w, vec3(0))) || any(greaterThan(w, vec3(1)))) return vec4(0);
  vec4 s = texture(uScal, w);
  float fine = detailNoise(q * 3.13 + 0.17, lod + 1.0);
  float erosion = 0.025 + 0.07 * (1.0 - detail.a) + 0.015 * (1.0 - fine);
  float edge = smoothstep(erosion, erosion + 0.085, s.y);
  s.y *= edge * mix(0.7, 1.65, detail.a);
  s.z *= mix(0.75, 1.25, fine);
  return s;
}
vec2 box(vec3 o, vec3 d) {
  vec3 safeD = mix(vec3(-1.0), vec3(1.0), greaterThanEqual(d, vec3(0))) * max(abs(d), vec3(1e-7));
  vec3 a = (vec3(-0.5 * uSize.x, 0.0, -0.5 * uSize.z) - o) / safeD, b = (vec3(0.5 * uSize.x, uSize.y, 0.5 * uSize.z) - o) / safeD;
  vec3 n = min(a, b), f = max(a, b);
  return vec2(max(max(n.x, n.y), n.z), min(min(f.x, f.y), f.z));
}
vec2 fireball(vec3 o, vec3 d) {  // sphere cut by the flat base plane
  vec3 oc = o - uFb.xyz;
  float b = dot(oc, d), h = b * b - dot(oc, oc) + uFb.w * uFb.w;
  if (h < 0.0) return vec2(1e30, -1e30);
  vec2 t = vec2(-b - sqrt(h), -b + sqrt(h));
  if (abs(d.y) < 1e-7) {
    if (o.y < uFbParams.x) return vec2(1e30, -1e30);
  } else {
    float tp = (uFbParams.x - o.y) / d.y;
    if (d.y > 0.0) t.x = max(t.x, tp); else t.y = min(t.y, tp);
  }
  return t;
}
void main() {
  vec4 wp = uInvViewProj * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
  vec3 o = uCam, d = normalize(wp.xyz / wp.w - o);
  // one glow light: grid emission (mip-reduced) + analytic fireball
  vec4 g = textureLod(uGlow, vec3(0.5), uGlowLod) * uCells * uGridOn;
  float gP = g.x + uGlowA.x;
  vec3 gPos = vec3(0.0, (g.y + uGlowA.x * uGlowA.y) / max(gP, 1e-6), 0.0);
  vec3 gI = planckChroma(max((g.z + uGlowA.x * uGlowA.z) / max(gP, 1e-6), 500.0)) * gP / (4.0 * PI);
  float tg = terrainHit(o, d);
  vec3 bg;
  if (tg < 1e30) {  // snow
    vec3 p = o + d * tg, Lg = gPos - p;
    vec2 ts = box(p, uSunDir);
    float sh = uGridOn > 0.5 && ts.x <= ts.y && ts.y > 0.0 ? texture(uLight, toUvw(p + uSunDir * max(ts.x, 0.0))).x : 1.0;
    float r2 = max(dot(Lg, Lg), 10000.0);
    vec3 normal = terrainNormal(p.xz);
    float grain = detailNoise(p / (12000.0 * sceneScale()), 1.0);
    float snow = smoothstep(0.86, 0.98, normal.y) * mix(0.8, 1.0, grain);
    vec3 alb = mix(vec3(0.17, 0.20, 0.23), vec3(0.78, 0.85, 0.93), snow);
    alb *= mix(0.82, 1.05, grain);
    vec3 E = uSun * max(dot(normal, uSunDir), 0.0) * sh + 1.7 * sky(normal)
      + gI * max(dot(normal, Lg), 0.0) / (r2 * sqrt(r2));
    bg = aerial(alb / PI * E, p, d);
  } else {
    bg = sky(d) + uSun * step(0.99998, dot(d, uSunDir)) / 6.8e-5;
  }
  vec2 tf = fireball(o, d);
  float fbA = uFbParams.z > 0.0 && tf.x < tf.y && tf.y > 0.0 && tf.x < tg
                  ? uFbParams.z * (1.0 - exp(-(tf.y - max(tf.x, 0.0)) / (0.1 * uFb.w))) : 0.0;
  vec3 fbL = blackbody(uFbParams.y), L = vec3(0.0);
  if (fbA > 0.0) {
    vec3 surface = o + d * max(tf.x, 0.0);
    float mottling = detailNoise((surface - uFb.xyz) / max(4.0 * uFb.w, 1.0) + vec3(0, -uTime * 0.025, 0), 0.0);
    float limb = sqrt(clamp((tf.y - max(tf.x, 0.0)) / max(2.0 * uFb.w, 1.0), 0.0, 1.0));
    fbL = aerial(blackbody(uFbParams.y * mix(0.84, 1.04, mottling)) * mix(0.55, 1.0, limb), surface, d);
  }
  float Tr = 1.0;
  bool fbDone = fbA <= 0.0;
  vec2 tb = box(o, d);
  tb = vec2(max(tb.x, 0.0), min(tb.y, tg));
  if (uGridOn > 0.5 && tb.x < tb.y) {
    float mu = dot(d, uSunDir), phase = mix(hg(mu, 0.7), hg(mu, -0.25), 0.3);
    vec3 skyFill = sky(vec3(0, 1, 0));
    vec3 amb = 1.3 * mix(skyFill, vec3(dot(skyFill, vec3(0.2126, 0.7152, 0.0722))), 0.6)
      + 0.12 * sky(vec3(1, 0, 0));
    float dt = (tb.y - tb.x) / float(uSteps);
    float t = tb.x + dt * hash(vec3(gl_FragCoord.xy, 1.0));
    for (int i = 0; i < uSteps && t < tb.y; ++i, t += dt) {
      if (!fbDone && t > tf.x) { L += Tr * fbA * fbL; Tr *= 1.0 - fbA; fbDone = true; }
      vec3 p = o + d * t, w = toUvw(p);
      vec4 s = cloudSample(p, dt * 0.4);
      float T = hotT(s, p.y), ext = extinction(s, T);
      if (ext < 1e-8) continue;
      vec3 sc = uExt.x * s.y * vec3(0.96, 0.96, 0.95) + uExt.y * s.z * vec3(0.66, 0.60, 0.52);
      vec3 Lg = gPos - p;
      float sunTr = clamp(texture(uLight, w).x, 0.0, 1.0);
      float overhead = cloudSample(p + vec3(0, 0.8 * cellSize(), 0), dt * 0.4).y;
      float ambientVisibility = exp(-2.5 * overhead);
      // A softened second scattering lobe fills sunlit billows without flattening shadows.
      vec3 sunLight = uSun * (sunTr * phase + 0.10 * pow(sunTr, 0.35) / (4.0 * PI));
      vec3 S = sc * (sunLight + amb * (0.45 + 0.55 * ambientVisibility)
        + gI / (4.0 * PI * max(dot(Lg, Lg), uFbParams.w * uFbParams.w)));
      if (T > 700.0) S += (ext - dot(sc, vec3(0.2126, 0.7152, 0.0722))) * blackbody(T);
      float a = exp(-ext * dt);
      L += Tr * aerial(S / ext, p, d) * (1.0 - a);
      Tr *= a;
      if (Tr < 0.003) break;
    }
  }
  if (!fbDone) { L += Tr * fbA * fbL; Tr *= 1.0 - fbA; }
  oColor = vec4(L + Tr * bg, 1.0);
}
