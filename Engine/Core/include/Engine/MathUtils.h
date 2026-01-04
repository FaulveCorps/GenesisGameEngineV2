#pragma once

#include <cmath>
#include <cstring>

namespace Genesis::Engine {

struct Matrix4 {
    float m[16];

    Matrix4() {
        Identity();
    }

    void Identity() {
        std::memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Matrix4 CreateIdentity() {
        Matrix4 mat;
        return mat;
    }

    static Matrix4 CreateTranslation(float x, float y, float z) {
        Matrix4 mat;
        mat.m[12] = x;
        mat.m[13] = y;
        mat.m[14] = z;
        return mat;
    }

    static Matrix4 CreateScale(float x, float y, float z) {
        Matrix4 mat;
        mat.m[0] = x;
        mat.m[5] = y;
        mat.m[10] = z;
        return mat;
    }

    static Matrix4 CreateRotationX(float angleRadians) {
        Matrix4 mat;
        float c = std::cos(angleRadians);
        float s = std::sin(angleRadians);
        mat.m[5] = c;
        mat.m[6] = s;
        mat.m[9] = -s;
        mat.m[10] = c;
        return mat;
    }

    static Matrix4 CreateRotationY(float angleRadians) {
        Matrix4 mat;
        float c = std::cos(angleRadians);
        float s = std::sin(angleRadians);
        mat.m[0] = c;
        mat.m[2] = -s;
        mat.m[8] = s;
        mat.m[10] = c;
        return mat;
    }

    static Matrix4 CreateRotationZ(float angleRadians) {
        Matrix4 mat;
        float c = std::cos(angleRadians);
        float s = std::sin(angleRadians);
        mat.m[0] = c;
        mat.m[1] = s;
        mat.m[4] = -s;
        mat.m[5] = c;
        return mat;
    }

    // Simple matrix multiplication: result = this * other
    Matrix4 Multiply(const Matrix4& other) const {
        Matrix4 result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                result.m[col * 4 + row] = 
                    m[0 * 4 + row] * other.m[col * 4 + 0] +
                    m[1 * 4 + row] * other.m[col * 4 + 1] +
                    m[2 * 4 + row] * other.m[col * 4 + 2] +
                    m[3 * 4 + row] * other.m[col * 4 + 3];
            }
        }
        return result;
    }
    
    // Operator overload
    Matrix4 operator*(const Matrix4& other) const {
        return Multiply(other);
    }

    static Matrix4 CreateLookAt(float eyeX, float eyeY, float eyeZ,
                                float centerX, float centerY, float centerZ,
                                float upX, float upY, float upZ) {
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;
        float rlf = 1.0f / std::sqrt(fx*fx + fy*fy + fz*fz);
        fx *= rlf; fy *= rlf; fz *= rlf;

        float rx = fy * upZ - fz * upY;
        float ry = fz * upX - fx * upZ;
        float rz = fx * upY - fy * upX;
        float rlr = 1.0f / std::sqrt(rx*rx + ry*ry + rz*rz);
        rx *= rlr; ry *= rlr; rz *= rlr;

        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;

        Matrix4 mat;
        mat.m[0] = rx; mat.m[4] = ry; mat.m[8] = rz; mat.m[12] = -(rx * eyeX + ry * eyeY + rz * eyeZ);
        mat.m[1] = ux; mat.m[5] = uy; mat.m[9] = uz; mat.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
        mat.m[2] = -fx; mat.m[6] = -fy; mat.m[10] = -fz; mat.m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
        mat.m[3] = 0.0f; mat.m[7] = 0.0f; mat.m[11] = 0.0f; mat.m[15] = 1.0f;
        return mat;
    }

    static Matrix4 CreateOrtho(float left, float right, float bottom, float top, float zNear, float zFar) {
        Matrix4 mat;
        mat.m[0] = 2.0f / (right - left);
        mat.m[5] = 2.0f / (top - bottom);
        mat.m[10] = -2.0f / (zFar - zNear);
        mat.m[12] = -(right + left) / (right - left);
        mat.m[13] = -(top + bottom) / (top - bottom);
        mat.m[14] = -(zFar + zNear) / (zFar - zNear);
        mat.m[15] = 1.0f;
        return mat;
    }
};

} // namespace Genesis::Engine
