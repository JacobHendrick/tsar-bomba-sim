in vec2 vUv;
out vec4 oColor;
layout(binding = 0) uniform sampler2D uSrc;
uniform int uUp;
uniform float uMax;  // > 0 on the first level: caps glare from the fireball (scene units)
vec3 S(vec2 o) { return texture(uSrc, vUv + o / vec2(textureSize(uSrc, 0))).rgb; }
void main() {
  if (uUp == 0) {  // dual-filter downsample; alpha carries log luminance for auto-exposure
    vec3 c = (4.0 * S(vec2(0.0)) + S(vec2(-1, -1)) + S(vec2(1, 1)) + S(vec2(1, -1)) + S(vec2(-1, 1))) / 8.0;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    oColor = vec4(uMax > 0.0 ? c * min(1.0, uMax / (lum + 1e-9)) : c, log(lum + 1e-4));
  } else {
    vec3 c = S(vec2(-1, 0)) + S(vec2(1, 0)) + S(vec2(0, -1)) + S(vec2(0, 1)) +
             2.0 * (S(vec2(-0.5, 0.5)) + S(vec2(0.5, 0.5)) + S(vec2(0.5, -0.5)) + S(vec2(-0.5, -0.5)));
    oColor = vec4(c / 12.0, 0.0);
  }
}
