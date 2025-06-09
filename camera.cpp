#include "camera.h"
#include <bx/math.h>
#include <cmath>

Camera::Camera()
    : yaw(0.0f), pitch(0.0f), distance(3.0f), lastX(0), lastY(0), rotating(false) {}

void Camera::update(GLFWwindow* window) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        if (!rotating) {
            lastX = xpos;
            lastY = ypos;
            rotating = true;
        } else {
            float dx = static_cast<float>(xpos - lastX);
            float dy = static_cast<float>(ypos - lastY);
            yaw += dx * 0.01f;
            pitch += dy * 0.01f;
            pitch = bx::clamp(pitch, -1.5f, 1.5f);
            lastX = xpos;
            lastY = ypos;
        }
    } else {
        rotating = false;
    }

    // Zoom with scroll
    static double lastScroll = 0.0;
    double scroll = glfwGetTime();  // replace with actual scroll callback if needed
    distance = bx::clamp(distance, 0.1f, 100.0f);
}

void Camera::getView(float* view) const {
    float eye[3], at[3] = { 0, 0, 0 }, up[3] = { 0, 1, 0 };
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float cy = std::cos(yaw), sy = std::sin(yaw);

    eye[0] = distance * cp * sy;
    eye[1] = distance * sp;
    eye[2] = distance * cp * cy;

    bx::mtxLookAt(view, eye, at, up);
}

void Camera::getProj(float* proj, float aspect) const {
    bx::mtxProj(proj, 60.0f, aspect, 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
}
