#pragma once
#include "gl_util.hpp"
#include "scene.hpp"

// Cosmetic exterior meshes, in metres. No internal device geometry is modelled.
struct ModelVertex { glm::vec3 p, normal, color; };
struct Mesh {
    std::vector<ModelVertex> vertices;
    GLuint vao = 0, vbo = 0;
    void triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color) {
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        vertices.insert(vertices.end(), {{a,n,color}, {b,n,color}, {c,n,color}});
    }
    void ellipsoid(glm::vec3 center, glm::vec3 radius, glm::vec3 color) {
        const int rings = 20, sides = 40;
        auto v = [&](int i, int j) {
            float a = float(i) * 3.14159265f / rings, b = float(j) * 6.2831853f / sides;
            glm::vec3 n(std::cos(a), std::sin(a) * std::cos(b), std::sin(a) * std::sin(b));
            return ModelVertex{center + radius * n, glm::normalize(n / radius), color};
        };
        for (int i=0; i<rings; ++i) for (int j=0; j<sides; ++j) {
            auto a=v(i,j), b=v(i+1,j), c=v(i+1,j+1), d=v(i,j+1);
            if (i > 0) vertices.insert(vertices.end(), {a,b,d});
            if (i < rings-1) vertices.insert(vertices.end(), {b,c,d});
        }
    }
    void panel(const std::vector<glm::vec3>& outline, glm::vec3 thickness, glm::vec3 color) {
        for (size_t i=1; i+1<outline.size(); ++i) {
            triangle(outline[0]+thickness, outline[i]+thickness, outline[i+1]+thickness, color);
            triangle(outline[0]-thickness, outline[i+1]-thickness, outline[i]-thickness, color);
        }
        for (size_t i=0; i<outline.size(); ++i) {
            glm::vec3 a=outline[i], b=outline[(i+1)%outline.size()];
            triangle(a-thickness, b-thickness, b+thickness, color*0.85f);
            triangle(a-thickness, b+thickness, a+thickness, color*0.85f);
        }
    }
    void star(glm::vec3 center, glm::vec3 axisA, glm::vec3 axisB, float radius) {
        for (int i=0; i<10; ++i) {
            auto point = [&](int k) {
                float a = float(k) * 0.62831853f;
                return center + (axisA*std::cos(a)+axisB*std::sin(a)) * radius * (k%2 ? 0.43f : 1.f);
            };
            triangle(center, point(i), point(i+1), {0.65f,0.025f,0.035f});
        }
    }
    void cable(glm::vec3 a, glm::vec3 b, float radius) {
        glm::vec3 up=glm::normalize(b-a), x=glm::normalize(glm::cross(up, glm::vec3(1,0,0)));
        glm::vec3 z=glm::cross(up,x);
        for (int i=0; i<6; ++i) {
            float t=i*6.2831853f/6, s=(i+1)*6.2831853f/6;
            glm::vec3 u=radius*(x*std::cos(t)+z*std::sin(t)), v=radius*(x*std::cos(s)+z*std::sin(s));
            triangle(a+u,b+u,b+v,{0.35f,0.32f,0.26f});
            triangle(a+u,b+v,a+v,{0.35f,0.32f,0.26f});
        }
    }
    void upload() {
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size()*sizeof(ModelVertex)), vertices.data(), GL_STATIC_DRAW);
        for (GLuint i=0;i<3;++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,sizeof(ModelVertex),reinterpret_cast<void*>(size_t(i)*sizeof(glm::vec3)));
        }
    }
    void draw() const { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size())); }
};
struct Models {
    Mesh aircraft, bomb, propeller, parachute;
    Prog program;
    void init() {
        program=makeProgram({"model.vert","model.frag"});
        const glm::vec3 white(0.76f,0.79f,0.80f), metal(0.38f,0.41f,0.43f);
        aircraft.ellipsoid({0,0,0},{23,1.9f,2.0f},white);
        aircraft.ellipsoid({15,1.25f,0},{3.1f,0.85f,1.5f},{0.045f,0.10f,0.14f});
        aircraft.ellipsoid({21,0,0},{2.1f,1.3f,1.35f},metal);
        for (float side : {-1.f,1.f}) {
            aircraft.panel({{5,0,side*1.4f},{-8,0,side*25},{-11.2f,0,side*25},{-4,0,side*1.4f}}, {0,0.24f,0},white);
            aircraft.panel({{-14,0.8f,side},{-20,0.8f,side*9},{-22,0.8f,side*9},{-19,0.8f,side}}, {0,0.15f,0},white);
            for (float z : {9.f,17.f}) {
                float x=z==9.f ? 2.6f : -1.f;
                aircraft.ellipsoid({x,-0.65f,side*z},{5,1.0f,1.1f},white);
                aircraft.ellipsoid({x+4.2f,-0.65f,side*z},{1.15f,0.65f,0.65f},metal);
            }
            aircraft.star({-6.3f,0.255f,side*16.f},{1,0,0},{0,0,side},1.7f);
            aircraft.star({-12,0,side*1.7f},{0,1,0},{1,0,0},0.8f);
        }
        aircraft.panel({{-13,1,0},{-18,7.4f,0},{-21.2f,7.4f,0},{-21,1,0}}, {0,0,0.18f},white);
        aircraft.star({-18.5f,4.7f,0.19f},{0,1,0},{1,0,0},1.15f);
        aircraft.star({-18.5f,4.7f,-0.19f},{0,1,0},{-1,0,0},1.15f);
        aircraft.upload();
        bomb.ellipsoid({0,0,0},{4.0f,1.05f,1.05f},{0.66f,0.68f,0.64f});
        for (int i=0;i<4;++i) {
            float a=i*1.5707963f;
            glm::vec3 r(0,std::cos(a),std::sin(a)), thick(0,-0.07f*std::sin(a),0.07f*std::cos(a));
            bomb.panel({glm::vec3(-1.8f,0,0)+r*0.6f,glm::vec3(-3.2f,0,0)+r*1.8f,
                        glm::vec3(-4.3f,0,0)+r*1.8f,glm::vec3(-3.8f,0,0)+r*0.3f},thick,metal);
        }
        bomb.upload();
        for (int i=0;i<4;++i) {
            float a=i*1.5707963f;
            glm::vec3 r(0,std::cos(a),std::sin(a)), tangent(0,-std::sin(a),std::cos(a));
            propeller.panel({r*0.45f-tangent*0.18f,r*2.8f-tangent*0.12f,
                             r*2.8f+tangent*0.20f,r*0.45f+tangent*0.3f},{0.035f,0,0},{0.13f,0.15f,0.16f});
        }
        propeller.upload();
        const int rings=12, sides=72;
        auto canopyPoint=[](int i,int j) {
            float theta=float(i)*1.5707963f/12, phi=float(j)*6.2831853f/72;
            float r=12.f*std::sin(theta)*(1.f+0.025f*std::cos(phi*12));
            return glm::vec3(r*std::cos(phi),8.f*std::cos(theta),r*std::sin(phi));
        };
        for (int i=0;i<rings;++i) for (int j=0;j<sides;++j) {
            glm::vec3 a=canopyPoint(i,j),b=canopyPoint(i+1,j),c=canopyPoint(i+1,j+1),d=canopyPoint(i,j+1);
            glm::vec3 fabric=glm::vec3(0.84f,0.81f,0.71f)*(j%6==0 ? 0.8f : 1.f);
            if (i>0) parachute.triangle(a,b,d,fabric);
            parachute.triangle(b,c,d,fabric);
        }
        for(auto& vertex : parachute.vertices)
            vertex.normal=glm::normalize(vertex.p/glm::vec3(144,64,144));
        for(int i=0;i<12;++i) parachute.cable({0,-16,0},canopyPoint(12,i*6),0.045f);
        parachute.upload();
    }
    void draw(const scene::Flight& f, const glm::mat4& vp, glm::vec3 eye, glm::vec3 sun) {
        program.use(); program.set("uViewProj",vp); program.set("uCam",eye); program.set("uSunDir",sun);
        glm::mat4 plane=glm::translate(glm::mat4(1),f.aircraft);
        program.set("uModel",plane); aircraft.draw();
        for(float side:{-1.f,1.f}) for(float z:{9.f,17.f}) {
            float x=z==9.f ? 7.6f : 4.f;
            for(int rotor=0;rotor<2;++rotor) {
                auto m=glm::translate(plane,glm::vec3(x+rotor*0.35f,-0.65f,z*side));
                m=glm::rotate(m,(rotor==0 ? f.propellerAngle : -f.propellerAngle+0.4f),glm::vec3(1,0,0));
                program.set("uModel",m); propeller.draw();
            }
        }
        if(f.bombVisible) {
            auto m=glm::translate(glm::mat4(1),f.bomb);
            program.set("uModel",glm::rotate(m,-1.5707963f*f.tilt,glm::vec3(0,0,1))); bomb.draw();
            if(f.canopy>0.001f) {
                m=glm::translate(m,glm::vec3(0,18,0));
                program.set("uModel",glm::scale(m,glm::vec3(f.canopy,1.f,f.canopy))); parachute.draw();
            }
        }
    }
};
