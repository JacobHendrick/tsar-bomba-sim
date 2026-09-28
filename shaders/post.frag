in vec2 vUv;
out vec4 oColor;
layout(binding = 0) uniform sampler2D uHdr;
layout(binding = 1) uniform sampler2D uBloom;
uniform mat4 uInvViewProj, uViewProj;
uniform vec3 uCam;
uniform vec4 uShock;   // burst point, Sedov-Taylor radius
uniform vec3 uShockP;  // shell width (m), screen displacement, burst height
uniform float uExposure, uBloomK;
// Refraction is strongest where the view ray grazes the shock sphere.
float shell(vec3 o, vec3 d, vec3 c, float tg) {
  float tc = dot(c - o, d);
  vec3 p = o + d * tc;
  float x = (length(p - c) - uShock.w) / max(uShockP.x, 1.0);
  return tc > 0.0 && tc < tg && p.y > 0.0 ? exp(-x * x) : 0.0;
}
void main() {
  vec4 wp = uInvViewProj * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
  vec3 o = uCam, d = normalize(wp.xyz / wp.w - o), c = uShock.xyz;
  float tg = d.y < 0.0 ? -o.y / d.y : 1e30, s = shell(o, d, c, tg);
  if (uShock.w > uShockP.z) {  // ground reflection: mirrored shell plus the Mach-stem ring on the snow
    s = max(s, 0.7 * shell(o, d, c * vec3(1, -1, 1), tg));
    float x = (length((o + d * tg).xz - c.xz) - sqrt(uShock.w * uShock.w - c.y * c.y)) / max(uShockP.x, 1.0);
    if (tg < 1e30) s = max(s, 0.5 * exp(-x * x));
  }
  vec4 cs = uViewProj * vec4(c, 1.0);
  vec2 dir = normalize(vUv - (cs.xy / cs.w * 0.5 + 0.5) + 1e-6) * sign(cs.w);
  vec2 uv = vUv + dir * s * uShockP.y;
  vec3 col = (texture(uHdr, uv).rgb + uBloomK * texture(uBloom, uv).rgb) * uExposure * (1.0 + 40.0 * s * uShockP.y);
  col = clamp(col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14), 0.0, 1.0);  // ACES fit
  oColor = vec4(pow(col, vec3(1.0 / 2.2)) + (hash(vec3(gl_FragCoord.xy, 3.0)) - 0.5) / 255.0, 1.0);
}
