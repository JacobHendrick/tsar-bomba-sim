#pragma once
#include <utility>
#include "gl_util.hpp"
#include "physics.hpp"
struct Fluid {
    glm::ivec3 n{0};
    glm::vec3 size{0}, ext{0};
    int iters = 40, glowLod = 0;
    GLuint vel[2]{}, scal[2]{}, pres[2]{}, div = 0, curl = 0, light = 0, glow = 0, atm = 0;
    Prog seedP, curlP, forceP, advectP, divP, jacobiP, projectP, lightP;
    void loadPrograms() {
        seedP = makeProgram({"seed.comp"});
        curlP = makeProgram({"curl.comp"});
    forceP = makeProgram({"forces.comp"});
    advectP = makeProgram({"advect.comp"});
    divP = makeProgram({"divergence.comp"});
    jacobiP = makeProgram({"jacobi.comp"});
    projectP = makeProgram({"project.comp"});
    lightP = makeProgram({"light.comp"});
  }
  void setCommon(const Prog& p) const {
    p.set("uN", n);
    p.set("uSize", size);
    p.set("uExt", ext);
    p.set("uAtm", 7);
  }
  void init(const phys::Burst& b, int N, int jacobi) {
    GLuint old[] = {vel[0], vel[1], scal[0], scal[1], pres[0], pres[1], div, curl, light, glow, atm};
    glDeleteTextures(11, old);
    n = {N, N / 2, N};
    size = glm::vec3(b.width, b.height, b.width);
    ext = glm::vec3(1.5e-3f, 1.0e-3f, 4e-3f) / float(b.scale);  // 1/m per unit smoke, dust, hot air
    iters = jacobi;
    for (int i = 0; i < 2; ++i) {
      vel[i] = makeTex(GL_TEXTURE_3D, GL_RGBA32F, n.x, n.y, n.z);
      scal[i] = makeTex(GL_TEXTURE_3D, GL_RGBA32F, n.x, n.y, n.z);
      pres[i] = makeTex(GL_TEXTURE_3D, GL_R32F, n.x, n.y, n.z);
      zero3D(vel[i], n, 4);
      zero3D(scal[i], n, 4);
      zero3D(pres[i], n, 1);
    }
    div = makeTex(GL_TEXTURE_3D, GL_R32F, n.x, n.y, n.z);
    curl = makeTex(GL_TEXTURE_3D, GL_RGBA32F, n.x, n.y, n.z);
    light = makeTex(GL_TEXTURE_3D, GL_R32F, n.x, n.y, n.z);
    glowLod = mipLevels(n.x) - 1;
    glow = makeTex(GL_TEXTURE_3D, GL_RGBA32F, n.x, n.y, n.z, glowLod + 1);
    zero3D(light, n, 1);
    zero3D(glow, n, 4);
    glGenerateMipmap(GL_TEXTURE_3D);
    auto tbl = phys::atmTable(b.height, 512);
    atm = makeTex(GL_TEXTURE_1D, GL_RGBA32F, 512);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 512, GL_RGBA, GL_FLOAT, tbl.data());
    bindTex(7, GL_TEXTURE_1D, atm);
    for (const Prog* p : {&seedP, &curlP, &forceP, &advectP, &divP, &jacobiP, &projectP, &lightP}) setCommon(*p);
  }
  void seed(const phys::Burst& b, glm::vec3 sun) {
    seedP.use();
    seedP.set("uCenter", glm::vec3(0, b.hob, 0));
    double r = std::max(b.Rmax, 2.5 * size.y / n.y);  // keep at least ~2.5 cells, same total heat
    seedP.set("uRadius", float(r));
    seedP.set("uBase", float(std::max(b.base, b.hob - r)));
    seedP.set("uTemp", float(b.temperature(b.tSeed)));
    seedP.set("uHeat", float(b.heat * std::pow(b.Rmax / r, 3.0)));
    seedP.set("uDust", glm::vec3(b.groundShock(b.tSeed), 6 * b.Rmax, 0));
    bindImg(0, vel[0], GL_RGBA32F);
    bindImg(1, scal[0], GL_RGBA32F);
    dispatch(n);
    updateLight(sun);
  }
  void step(const phys::Burst& b, double t, float dt, glm::vec3 sun) {
    curlP.use();
    bindTex(0, GL_TEXTURE_3D, vel[0]);
    bindImg(0, curl, GL_RGBA32F);
    dispatch(n);
    forceP.use();
    forceP.set("uDt", dt);
    forceP.set("uVort", 0.15f);
    bindTex(1, GL_TEXTURE_3D, scal[0]);
    bindTex(2, GL_TEXTURE_3D, curl);
    bindImg(0, vel[1], GL_RGBA32F);
    dispatch(n);
    advectP.use();
    advectP.set("uDt", dt);
    advectP.set("uMode", 0);
    bindTex(0, GL_TEXTURE_3D, vel[1]);
    bindTex(1, GL_TEXTURE_3D, vel[1]);
    bindImg(0, vel[0], GL_RGBA32F);
    dispatch(n);
    divP.use();
    bindTex(0, GL_TEXTURE_3D, vel[0]);
    bindImg(0, div, GL_R32F);
    dispatch(n);
    jacobiP.use();
    bindTex(1, GL_TEXTURE_3D, div);
    for (int i = 0; i < iters; ++i, std::swap(pres[0], pres[1])) {
      bindTex(0, GL_TEXTURE_3D, pres[0]);
      bindImg(0, pres[1], GL_R32F);
      dispatch(n);
    }
    projectP.use();
    bindTex(0, GL_TEXTURE_3D, vel[0]);
    bindTex(1, GL_TEXTURE_3D, pres[0]);
    bindImg(0, vel[1], GL_RGBA32F);
    dispatch(n);
    std::swap(vel[0], vel[1]);
    advectP.use();
    advectP.set("uMode", 1);
    advectP.set("uRadK", float(b.radK));
    advectP.set("uDust", glm::vec3(b.groundShock(t), 6 * b.Rmax, 3e-4f));
    bindTex(0, GL_TEXTURE_3D, vel[0]);
    bindTex(1, GL_TEXTURE_3D, scal[0]);
    bindImg(0, scal[1], GL_RGBA32F);
    dispatch(n);
    std::swap(scal[0], scal[1]);
    updateLight(sun);
  }
  // Sun transmittance per cell, plus emitted power (P, P*y, P*T) mip-reduced to a single glow light.
  void updateLight(glm::vec3 sun) {
    lightP.use();
    lightP.set("uSunDir", sun);
    bindTex(1, GL_TEXTURE_3D, scal[0]);
    bindImg(0, light, GL_R32F);
    bindImg(1, glow, GL_RGBA32F);
    dispatch(n);
    glBindTexture(GL_TEXTURE_3D, glow);
    glGenerateMipmap(GL_TEXTURE_3D);
  }
};
