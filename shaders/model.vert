layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uViewProj, uModel;
out vec3 vPosition, vNormal, vColor;
void main() {
  vec4 world=uModel*vec4(aPosition,1);
  vPosition=world.xyz;
  vNormal=mat3(transpose(inverse(uModel)))*aNormal;
  vColor=aColor;
  gl_Position=uViewProj*world;
}
