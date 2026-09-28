in vec3 vPosition, vNormal, vColor;
uniform vec3 uCam, uSunDir;
out vec4 oColor;
void main() {
  vec3 n=normalize(vNormal);
  if(!gl_FrontFacing) n=-n;
  vec3 eye=normalize(uCam-vPosition), halfVector=normalize(eye+uSunDir);
  float diffuse=max(dot(n,uSunDir),0.0);
  float spec=pow(max(dot(n,halfVector),0.0),48.0);
  vec3 ambient=mix(vec3(3.4,3.9,4.5),vec3(6.0,7.5,9.5),0.5+0.5*n.y);
  oColor=vec4(vColor*(ambient+vec3(95,79,61)*diffuse)+vec3(18,16,14)*spec,1);
}
