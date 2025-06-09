#ifndef CAMERA_H
#define CAMERA_H

#include <GLFW/glfw3.h>

class Camera {
public:
    Camera();
    void update(GLFWwindow* window);
    void getView(float* view) const;
    void getProj(float* proj, float aspect) const;

private:
    float yaw, pitch, distance;
    double lastX, lastY;
    bool rotating;
};

#endif 