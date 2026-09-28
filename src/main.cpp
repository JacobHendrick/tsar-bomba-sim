#include <cstdio>
#include <cstdlib>
#include <string>
#include "renderer.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
struct App {
    GLFWwindow* win = nullptr;
    int preset = 0, grid = 128, iters = 40, requestedPreset = -1;
    phys::Burst burst{phys::kPresets[0]};
    Fluid fluid;
    Renderer ren;
    double t = 0, simT = 0, speedMul = 1;
    bool paused = false, logTime = true, seeded = false, shotPending = false;
    bool introEnabled = true, introActive = true, autoFrame = true, exposureReset = true;
    bool fixedCamera = false;
    double introTime = 0;
    float yaw = 0.5f, pitch = 0.10f, dist = 1, targetY = 0;
    glm::vec3 focus{0};
    double mx = 0, my = 0;
    const float sunEl = 0.105f, sunAz = 2.3f;
    glm::vec3 sunDir{std::cos(sunEl) * std::sin(sunAz), std::sin(sunEl), std::cos(sunEl) * std::cos(sunAz)};
    glm::vec3 sun = glm::vec3(1.0f, 0.86f, 0.72f) * 550.f;
    void reset(int p) {
        preset = p;
        burst = phys::Burst(phys::kPresets[p]);
        fluid.init(burst, grid, iters);
        t = 1e-5 * burst.t2, simT = 0, seeded = false;
        introTime = 0; introActive = introEnabled; autoFrame = true; exposureReset = true;
        yaw = 0.5f; pitch = 0.10f;
        frameScene();
    }
    void frameScene() {
        if (!autoFrame) return;
        focus = glm::vec3(0);
        if (fixedCamera) {
            dist = 100000.f * float(burst.scale);
            targetY = 26000.f * float(burst.scale);
            return;
        }
        float growth = scene::ease(30, 480, t);
        dist = glm::mix(34000.f, 100000.f, growth) * float(burst.scale);
        targetY = glm::mix(7000.f, 32000.f, growth) * float(burst.scale);
    }
    void skipIntro() {
        introActive = false; introTime = scene::introDuration; exposureReset = true;
        frameScene();
    }
    double linearSpeed() const { return 10.0 * std::pow(burst.W, 0.25) * speedMul; }
    float dtStep() const { return float(std::min(fluid.size.y / fluid.n.y / 250.0, 0.2 * simT)); }
    void stepTo(double target, int maxSteps) {
        if (!seeded && target >= burst.tSeed) {
            fluid.seed(burst, sunDir);
            seeded = true;
            simT = burst.tSeed;
        }
        for (int i = 0; seeded && i < maxSteps && simT < target; ++i) {
            float dt = float(std::min(double(dtStep()), target - simT));
            fluid.step(burst, simT + dt, dt, sunDir);
            simT = std::min(target, simT + dt);
        }
    }
    void advance(double dtw) {
        if (paused) return;
        if (introActive) {
            introTime = std::min(scene::introDuration, introTime + dtw * speedMul);
            if (introTime >= scene::introDuration) skipIntro();
            return;
        }
        bool early = logTime && t < burst.tSeed;
        double target = early ? t * std::pow(10.0, dtw * speedMul / 1.5) : t + dtw * linearSpeed();
        stepTo(target, 3);
        t = seeded ? std::min(target, simT) : target;
        frameScene();
    }
    std::pair<glm::vec3, glm::vec3> cameraPose() const {
        glm::vec3 target = focus + glm::vec3(0, targetY, 0);
        glm::vec3 eye = target + dist * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
        // Keep the orbit camera above the decorative terrain's maximum height.
        eye.y = std::max(eye.y, 2400.f * float(burst.scale) + 30.f);
        if (introActive && autoFrame && !fixedCamera) {
            auto flight = scene::flight(introTime, float(burst.hob));
            float follow = scene::ease(scene::releaseTime, scene::releaseTime+1.8, introTime);
            glm::vec3 closeTarget = glm::mix(flight.aircraft, flight.bomb+glm::vec3(0,9,0), follow);
            glm::vec3 closeEye = closeTarget + glm::mix(glm::vec3(48,17,60), glm::vec3(28,5,54), follow);
            float pullback = scene::ease(18, scene::introDuration, introTime);
            target = glm::mix(closeTarget, target, pullback);
            eye = glm::mix(closeEye, eye, pullback);
        }
        return {eye, target};
    }
    void takeCameraControl() {
        if (!autoFrame) return;
        auto [eye, target] = cameraPose();
        glm::vec3 offset = eye - target;
        dist = glm::length(offset);
        pitch = std::asin(std::clamp(offset.y / dist, -1.f, 1.f));
        yaw = std::atan2(offset.x, offset.z);
        focus = {target.x, 0, target.z}; targetY = target.y;
        autoFrame = false; fixedCamera = false;
    }
    View view() const {
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        auto [eye, target] = cameraPose();
        glm::mat4 proj = glm::perspective(glm::radians(55.f), float(fw) / float(std::max(fh, 1)), introActive ? 1.f : 10.f, 1e6f);
        return {proj * glm::lookAt(eye, target, glm::vec3(0, 1, 0)), eye};
    }
    void render(float dtw, bool instant) {
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        if (fw < 8 || fh < 8) return;
        ren.resize(fw, fh);
        ren.draw(burst, t, fluid, seeded, view(), sunDir, sun, dtw, instant || exposureReset, introActive, introTime);
        exposureReset = false;
    }
    void saveShot(const std::string& file) {
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        if (fw < 8 || fh < 8) throw std::runtime_error("cannot capture an empty framebuffer");
        std::vector<unsigned char> px(size_t(fw) * fh * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0,0, fw, fh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
        stbi_flip_vertically_on_write(1);
        if (!stbi_write_png(file.c_str(), fw, fh, 3, px.data(), fw * 3))
            throw std::runtime_error("cannot save screenshot: " + file);
        std::printf("saved %s at %s t = %.6g s\n", file.c_str(), introActive ? "intro" : "explosion", introActive ? introTime : t);
        if (!introActive && burst.W == 50.0 && seeded)
            std::printf("Visual cloud envelope: %.1f km wide, %.1f km high (art-directed scale)\n",
                        ren.visualEnvelope.x/1000.f, ren.visualEnvelope.y/1000.f);
    }
    void keys(float dtw) {
        float k = 1.2f * dtw;
        for (int key : {GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_W, GLFW_KEY_S})
            if (glfwGetKey(win,key)) takeCameraControl();
        if (glfwGetKey(win, GLFW_KEY_LEFT)) yaw -= k;
        if (glfwGetKey(win, GLFW_KEY_RIGHT)) yaw += k;
    if (glfwGetKey(win, GLFW_KEY_UP)) pitch = std::min(pitch + k, 1.4f);
    if (glfwGetKey(win, GLFW_KEY_DOWN)) pitch = std::max(pitch - k, -1.2f);
    if (glfwGetKey(win, GLFW_KEY_W)) targetY += 0.3f * dist * dtw;
    if (glfwGetKey(win, GLFW_KEY_S)) targetY = std::max(targetY - 0.3f * dist * dtw, 0.f);
  }
  void updateTitle() {
    char buf[256];
    if (introActive) {
        std::snprintf(buf, sizeof buf, "Tu-95V | %s | %.1f / 24 s%s | I: skip drop | Space: pause | C: cinematic camera",
                      introTime < scene::releaseTime ? "approach" : introTime < 18 ? "parachute drop" : "detonation view",
                      introTime, paused ? " (paused)" : "");
        glfwSetWindowTitle(win,buf);
        return;
    }
    bool early = logTime && t <burst.tSeed;
    std::snprintf(buf, sizeof buf, "%s | t = %.3g s%s | %s x%.3g | fireball %.0f K | shock %.1f km | grid %dx%dx%d",
                  burst.name, t, paused ? " (paused)" : "", early ? "log-time" : "speed", early ? speedMul : linearSpeed(),
                  burst.temperature(t), burst.shockR(t) / 1e3, fluid.n.x, fluid.n.y, fluid.n.z);
                  glfwSetWindowTitle(win, buf);
    }
};
static App& app(GLFWwindow* w) { return *static_cast<App*>(glfwGetWindowUserPointer(w)); }
static void onKey(GLFWwindow* w, int key, int, int action, int) {
  if (action != GLFW_PRESS) return;
  App& a = app(w);
  switch (key) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, 1); break;
    case GLFW_KEY_SPACE: a.paused = !a.paused; break;
    case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD: a.speedMul = std::min(a.speedMul * 2, 1024.0); break;
    case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: a.speedMul = std::max(a.speedMul / 2, 1.0 / 1024.0); break;
    case GLFW_KEY_L: a.logTime = !a.logTime; break;
    case GLFW_KEY_I: a.skipIntro(); break;
    case GLFW_KEY_C:
      if (a.autoFrame && !a.fixedCamera) a.takeCameraControl();
      else { a.autoFrame = true; a.fixedCamera = false; a.frameScene(); }
      break;
    case GLFW_KEY_F:
      a.fixedCamera = !a.fixedCamera; a.autoFrame = true;
      a.yaw = 0.5f; a.pitch = 0.10f; a.frameScene();
      break;
    case GLFW_KEY_R: a.requestedPreset = a.preset; break;
    case GLFW_KEY_1: case GLFW_KEY_2: a.requestedPreset = key - GLFW_KEY_1; break;
    case GLFW_KEY_P: a.shotPending = true; break;
  }
}
static void onCursor(GLFWwindow* w, double x, double y) {
  App& a = app(w);
  float dx = float(x - a.mx), dy = float(y - a.my);
  a.mx = x, a.my = y;
  if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) || glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT)) a.takeCameraControl();
  if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT)) a.yaw -= 0.005f * dx, a.pitch = std::clamp(a.pitch + 0.005f * dy, -1.2f, 1.4f);
  if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT)) a.targetY = std::max(a.targetY + 0.002f * dy * a.dist, 0.f);
}
static void onScroll(GLFWwindow* w, double, double dy) {
  App& a = app(w);
  a.takeCameraControl();
  a.dist = std::clamp(a.dist * float(std::pow(0.9, dy)), a.introActive ? 12.f : 100.f, float(a.burst.width * 5));
}
int main(int argc, char** argv) {
  try {
  App a;
  int W = 1280, H = 720;
  double shotT = -1;
  bool introShot = false;
  std::string shotFile;
  for (int i = 1; i < argc; ++i) {
    std::string s = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) throw std::runtime_error("missing value for " + s);
      return argv[++i];
    };
    auto integer = [&](int lo, int hi) {
      std::string value = next();
      size_t used = 0;
      int n = std::stoi(value, &used);
      if (used != value.size() || n < lo || n > hi)
        throw std::runtime_error(s + " must be between " + std::to_string(lo) + " and " + std::to_string(hi));
      return n;
    };
    if (s == "--grid") {
      a.grid = integer(16, 256);
      if (a.grid % 8 != 0) throw std::runtime_error("--grid must be a multiple of 8");
    }
    else if (s == "--iters") a.iters = integer(1, 1000);
    else if (s == "--steps") a.ren.steps = integer(1, 4096);
    else if (s == "--preset") a.preset = integer(1, 2) - 1;
    else if (s == "--size") { W = integer(8, 8192); H = integer(8, 8192); }
    else if (s == "--skip-intro") a.introEnabled = false;
    else if (s == "--fixed-camera") a.fixedCamera = true;
    else if (s == "--shot" || s == "--intro-shot") {
      if (shotT >= 0) throw std::runtime_error("choose only one screenshot mode");
      introShot = s == "--intro-shot";
      std::string value = next();
      size_t used = 0;
      shotT = std::stod(value, &used);
      if (used != value.size() || !std::isfinite(shotT) || shotT < 0)
        throw std::runtime_error("--shot time must be finite and nonnegative");
      if (introShot && shotT >= scene::introDuration)
        throw std::runtime_error("--intro-shot time must be below 24 seconds");
      shotFile = next();
      if (shotFile.empty()) throw std::runtime_error("--shot requires an output filename");
    }
    else if (s == "--help" || s == "-h") {
      std::printf("usage: tsar [--preset 1|2] [--grid N] [--iters N] [--steps N] [--size W H] [--skip-intro]\n"
                  "            [--fixed-camera] [--shot T file.png | --intro-shot T file.png]\n");
      return 0;
    }
    else throw std::runtime_error("unknown option: " + s + " (use --help)");
  }
  glfwSetErrorCallback([](int code, const char* message) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, message);
  });
  if (!glfwInit()) throw std::runtime_error("cannot initialize GLFW; a graphical display is required");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  if (shotT >= 0) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  a.win = glfwCreateWindow(W, H, "Tsar Bomba", nullptr, nullptr);
  if (!a.win) throw std::runtime_error("No OpenGL 4.3 core context (macOS stops at 4.1)");
  glfwMakeContextCurrent(a.win);
  glfwSwapInterval(1);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) || !GLAD_GL_VERSION_4_3)
    throw std::runtime_error("cannot load OpenGL 4.3 functions");
  std::printf("OpenGL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
  GLint max3D = 0;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3D);
  if (a.grid > max3D) throw std::runtime_error("--grid exceeds the GPU's 3D texture limit");
  glfwSetWindowUserPointer(a.win, &a);
  glfwSetKeyCallback(a.win, onKey);
  glfwSetCursorPosCallback(a.win, onCursor);
  glfwSetScrollCallback(a.win, onScroll);
    a.fluid.loadPrograms();
    a.ren.init();
    a.reset(a.preset);
    a.updateTitle();
    if (shotT >= 0) {
      if (introShot) { a.introActive = true; a.introTime = shotT; }
      else {
        a.skipIntro();
        a.stepTo(shotT, 1 << 30);
        a.t = a.seeded ? a.simT : shotT;
        a.frameScene();
      }
      a.render(0.f, true);  // first frame meters exposure
      a.render(0.f, true);
      if (GLenum error = glGetError(); error != GL_NO_ERROR)
        throw std::runtime_error("OpenGL rendering error " + std::to_string(error));
      a.ren.validateHdr();
      a.saveShot(shotFile);
      glfwTerminate();
      return 0;
    }
    double last = glfwGetTime(), titleT = 0;
    while (!glfwWindowShouldClose(a.win)) {
      glfwPollEvents();
      if (glfwWindowShouldClose(a.win)) break;
      if (a.requestedPreset >= 0) {
        a.reset(a.requestedPreset);
        a.requestedPreset = -1;
      }
      int fw, fh;
      glfwGetFramebufferSize(a.win, &fw, &fh);
      if (fw < 8 || fh < 8) {
        glfwWaitEventsTimeout(0.1);
        last = glfwGetTime();
        continue;
      }
      double now = glfwGetTime();
      float dtw = float(std::min(now - last, 0.1));
      last = now;
      a.keys(dtw);
      a.advance(dtw);
      a.render(dtw, false);
      if (a.shotPending) a.saveShot("tsar_" + std::to_string(int(a.t)) + "s.png"), a.shotPending = false;
      glfwSwapBuffers(a.win);
      if (now - titleT > 0.25) titleT = now, a.updateTitle();
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    glfwTerminate();
    return 1;
  }
  glfwTerminate();
  return 0;
}
