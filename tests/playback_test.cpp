// Exercise the same App controller used by the desktop entry point.
#define main tsarDesktopEntry
#include "main.cpp"
#undef main

static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        check(glfwInit(), "GLFW initialization failed");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        App a;
        a.grid=24; a.iters=8; a.ren.steps=48;
        a.win=glfwCreateWindow(321,181,"Playback check",nullptr,nullptr);
        check(a.win!=nullptr, "OpenGL window creation failed");
        glfwMakeContextCurrent(a.win);
        check(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "OpenGL loader failed");
        a.fluid.loadPrograms(); a.ren.init(); a.reset(0);
        float detailMean[4]{};
        bindTex(8,GL_TEXTURE_3D,a.ren.billows);
        glGetTexImage(GL_TEXTURE_3D,mipLevels(128)-1,GL_RGBA,GL_FLOAT,detailMean);
        for(float channel : detailMean)
            check(std::isfinite(channel) && channel>0.05f && channel<0.95f,
                  "cellular detail texture or mip chain is empty/invalid");
        a.render(0,true);
        float metered=a.ren.logLum;
        a.render(0,false);
        check(a.ren.logLum==metered,"zero wall-time changes exposure");
        a.fixedCamera=true; a.frameScene();
        glm::vec3 referenceEye=a.view().eye;
        a.introTime=23; a.t=480; a.frameScene();
        check(glm::length(a.view().eye-referenceEye)<0.01f,"fixed camera moves with the timeline");
        a.skipIntro();
        check(glm::length(a.view().eye-referenceEye)<0.01f,"fixed camera jumps at detonation");
        a.fixedCamera=false; a.reset(0);
        a.introTime=10;
        glm::vec3 eye=a.view().eye;
        a.takeCameraControl();
        check(glm::length(a.view().eye-eye)<0.1f,"manual camera takeover jumps");
        a.autoFrame=true; a.frameScene(); a.introTime=0;
        a.paused=true; a.advance(1);
        check(a.introTime==0,"paused intro advances");
        a.paused=false;
        for(int i=0;i<250 && a.introActive;++i) {
            a.advance(0.1);
            if(i%20==0) { a.render(0.1,false); a.ren.validateHdr(); }
        }
        check(!a.introActive && !a.seeded,"intro did not transition cleanly to the fireball");
        for(int i=0;i<150 && !a.seeded;++i) a.advance(0.1);
        check(a.seeded && std::isfinite(a.t),"explosion never reaches the fluid stage");
        a.render(0.1,true); a.ren.validateHdr();
        a.reset(1);
        check(a.introActive && !a.seeded && a.introTime==0,"restart does not restore the introduction");
        glfwSetWindowSize(a.win,333,197);
        glfwPollEvents();
        a.render(0,true); a.ren.validateHdr();
        a.skipIntro(); a.stepTo(20,1000); a.t=a.simT; a.frameScene();
        a.render(0,true); a.ren.validateHdr();
        check(glGetError()==GL_NO_ERROR,"OpenGL error during playback/reset/resize");
        glfwTerminate();
        std::puts("Playback, pause, fixed/manual camera, detonation, restart, preset change, and resize passed");
    } catch(const std::exception& e) {
        std::fprintf(stderr,"%s\n",e.what()); glfwTerminate(); return 1;
    }
}
