#pragma once
#include <algorithm>
#include <cmath>
#include "fluid.hpp"
struct View { glm::mat4 viewProj; glm::vec3 eye; };
// Raymarch -> HDR float target -> dual-filter bloom + log-average exposure -> shock distortion + ACES.
struct Renderer {
  int w = 0, h = 0, steps = 160;
  GLuint vao = 0, hdr = 0, hdrFbo = 0, detail = 0;
  std::vector<GLuint> bl, blFbo;
  std::vector<glm::ivec2> blSize;
  Prog marchP, bloomP, postP;
  float logLum = 0.f;
  void init() {
    glGenVertexArrays(1, &vao);
    marchP = makeProgram({"fullscreen.vert", "raymarch.frag"});
    bloomP = makeProgram({"fullscreen.vert", "bloom.frag"});
    postP = makeProgram({"fullscreen.vert", "post.frag"});
    Prog detailP = makeProgram({"detail.comp"});
    detail = makeTex(GL_TEXTURE_3D, GL_RGBA16F, 128, 128, 128, mipLevels(128));
    for (GLenum wrap : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R})
      glTexParameteri(GL_TEXTURE_3D, wrap, GL_REPEAT);
    detailP.use();
    detailP.set("uN", glm::ivec3(128));
    bindImg(0, detail, GL_RGBA16F);
    dispatch(glm::ivec3(128));
    glGenerateMipmap(GL_TEXTURE_3D);
    glDeleteProgram(detailP.id);
  }
  static GLuint fboFor(GLuint tex) {
    GLuint f;
    glGenFramebuffers(1, &f);
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glDeleteFramebuffers(1, &f);
      throw std::runtime_error("incomplete render framebuffer");
    }
    return f;
  }
  void resize(int W, int H) {
    if ((W == w && H == h) || W < 8 || H < 8) return;
    w = W, h = H;
    glDeleteTextures(1, &hdr);
    glDeleteFramebuffers(1, &hdrFbo);
    glDeleteTextures(GLsizei(bl.size()), bl.data());
    glDeleteFramebuffers(GLsizei(blFbo.size()), blFbo.data());
    bl.clear(), blFbo.clear(), blSize.clear();
    hdr = makeTex(GL_TEXTURE_2D, GL_RGBA32F, w, h);
    hdrFbo = fboFor(hdr);
    glm::ivec2 s(w, h);
    for (int i = 0; i < 6 && s.x > 3 && s.y > 3; ++i) {
      s /= 2;
      bl.push_back(makeTex(GL_TEXTURE_2D, GL_RGBA32F, s.x, s.y, 1, i == 0 ? mipLevels(std::max(s.x, s.y)) : 1));
      blFbo.push_back(fboFor(bl.back()));
      blSize.push_back(s);
    }
  }
  float exposure() const { return std::clamp(0.18f / std::exp(logLum), 1e-9f, 1.f); }
  void validateHdr() const {
    // Screenshot/smoke-test path only: catch NaNs before tone mapping hides them.
    std::vector<float> pixels(size_t(w) * h * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, hdrFbo);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("HDR readback failed");
    for (float value : pixels)
      if (!std::isfinite(value)) throw std::runtime_error("non-finite HDR pixel");
  }
  void pass(GLuint fbo, glm::ivec2 size) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, size.x, size.y);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  void draw(const phys::Burst& b, double t, const Fluid& fl, bool gridOn, const View& v, glm::vec3 sunDir,
            glm::vec3 sun, float dtWall, bool instantExposure) {
    glBindVertexArray(vao);
    glm::mat4 inv = glm::inverse(v.viewProj);
    double T = b.temperature(t), R = b.fireballR(t), Rs = b.shockR(t);
    float vis = gridOn ? float(1.0 - glm::smoothstep(b.tSeed, b.tSeed + 0.5 * b.t2, t)) : 1.f;
    fl.setCommon(marchP);
    marchP.set("uInvViewProj", inv);
    marchP.set("uCam", v.eye);
    marchP.set("uSunDir", sunDir);
    marchP.set("uSun", sun);
    marchP.set("uFb", glm::vec4(0, b.hob, 0, R));
    marchP.set("uFbParams", glm::vec4(b.base, T, vis, b.Rmax));
    marchP.set("uGlowA", glm::vec4(vis * 5.670374e-8 * std::pow(T, 4) * 4 * 3.14159265 * R * R, b.hob, T, 0));
    marchP.set("uGlowLod", float(fl.glowLod));
    marchP.set("uCells", float(fl.n.x) * fl.n.y * fl.n.z);
    marchP.set("uGridOn", gridOn ? 1.f : 0.f);
    marchP.set("uSteps", steps);
    marchP.set("uTime", float(t));
    bindTex(1, GL_TEXTURE_3D, fl.scal[0]);
    bindTex(2, GL_TEXTURE_3D, fl.light);
    bindTex(3, GL_TEXTURE_3D, fl.glow);
    bindTex(4, GL_TEXTURE_3D, detail);
    marchP.use();
    pass(hdrFbo, {w, h});
    bloomP.use();
    bloomP.set("uUp", 0);
    for (size_t i = 0; i < bl.size(); ++i) {
      bloomP.set("uMax", i == 0 ? 20.f / exposure() : 0.f);
      bindTex(0, GL_TEXTURE_2D, i == 0 ? hdr : bl[i - 1]);
      pass(blFbo[i], blSize[i]);
    }
    float px[4];
    bindTex(0, GL_TEXTURE_2D, bl[0]);
    glGenerateMipmap(GL_TEXTURE_2D);
    glGetTexImage(GL_TEXTURE_2D, mipLevels(std::max(blSize[0].x, blSize[0].y)) - 1, GL_RGBA, GL_FLOAT, px);
    if (std::isfinite(px[3])) logLum = instantExposure ? px[3] : logLum + (px[3] - logLum) * (1.f - std::exp(-1.5f * dtWall));
    bloomP.set("uUp", 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glColorMask(1, 1, 1, 0);
    for (size_t i = bl.size() - 1; i > 0; --i) {
      bindTex(0, GL_TEXTURE_2D, bl[i]);
      pass(blFbo[i - 1], blSize[i - 1]);
    }
    glDisable(GL_BLEND);
    glColorMask(1, 1, 1, 1);
    postP.set("uInvViewProj", inv);
    postP.set("uViewProj", v.viewProj);
    postP.set("uCam", v.eye);
    postP.set("uShock", glm::vec4(0, b.hob, 0, Rs));
    float amp = t > 0 && Rs < 0.7 * b.width ? float(0.006 * std::min(1.0, 3 * b.Rmax / Rs)) : 0.f;
    postP.set("uShockP", glm::vec3(0.04 * Rs, amp, b.hob));
    postP.set("uExposure", exposure());
    postP.set("uBloomK", 0.06f / float(bl.size()));
    bindTex(0, GL_TEXTURE_2D, hdr);
    bindTex(1, GL_TEXTURE_2D, bl[0]);
    postP.use();
    pass(0, {w, h});
  }
};
