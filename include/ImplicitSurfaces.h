#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>

// 3D vector class with templated type
template<typename T = double>
class Vec3 {
public:
    T x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(T x, T y, T z) : x(x), y(y), z(z) {}

    // Add template conversion constructor
    template<typename U>
    Vec3(const Vec3<U>& other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}

    // Vector length
    T length() const {
        return std::sqrt(x*x + y*y + z*z);
    }

    // Vector normalization
    Vec3<T> normalize() const {
        T len = length();
        if (len == 0) return *this;
        return Vec3<T>(x / len, y / len, z / len);
    }

    // Vector addition
    Vec3<T> operator+(const Vec3<T>& other) const {
        return Vec3<T>(x + other.x, y + other.y, z + other.z);
    }

    // Vector subtraction
    Vec3<T> operator-(const Vec3<T>& other) const {
        return Vec3<T>(x - other.x, y - other.y, z - other.z);
    }

    // Vector scalar multiplication
    Vec3<T> operator*(T scalar) const {
        return Vec3<T>(x * scalar, y * scalar, z * scalar);
    }

    // Dot product
    T dot(const Vec3<T>& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
};

// Implicit surface base class
class ImplicitSurface {
public:
    virtual ~ImplicitSurface() = default;

    // Evaluate the implicit function value at point (x,y,z)
    // Negative value indicates inside the object, 0 indicates on the surface, positive value indicates outside the object
    virtual double evaluate(const Vec3<double>& point) const = 0;

    // Calculate the normal (gradient) at a given point
    virtual Vec3<double> gradient(const Vec3<double>& point) const {
        // Use numerical differentiation to calculate gradient
        const double h = 0.0001; // Small perturbation value

        double dx = evaluate(Vec3<double>(point.x + h, point.y, point.z)) -
                    evaluate(Vec3<double>(point.x - h, point.y, point.z));

        double dy = evaluate(Vec3<double>(point.x, point.y + h, point.z)) -
                    evaluate(Vec3<double>(point.x, point.y - h, point.z));

        double dz = evaluate(Vec3<double>(point.x, point.y, point.z + h)) -
                    evaluate(Vec3<double>(point.x, point.y, point.z - h));

        return Vec3<double>(dx, dy, dz).normalize();
    }
};

// Sphere implicit surface
class Sphere : public ImplicitSurface {
private:
    Vec3<double> center;
    double radius;

public:
    Sphere(const Vec3<double>& center, double radius)
        : center(center), radius(radius) {}

    double evaluate(const Vec3<double>& point) const override {
        Vec3<double> diff = point - center;
        return diff.length() - radius;
    }
};

// Box implicit surface (using smooth approximation)
class Box : public ImplicitSurface {
private:
    Vec3<double> center;
    Vec3<double> dimensions; // half-width, half-height, half-depth
    double smoothing;

public:
    Box(const Vec3<double>& center, const Vec3<double>& dimensions, double smoothing = 0.1)
        : center(center), dimensions(dimensions), smoothing(smoothing) {}

    double evaluate(const Vec3<double>& point) const override {
        Vec3<double> d = Vec3<double>(
            std::abs(point.x - center.x) - dimensions.x,
            std::abs(point.y - center.y) - dimensions.y,
            std::abs(point.z - center.z) - dimensions.z
        );

        Vec3<double> dMax = Vec3<double>(std::max(d.x, 0.0), std::max(d.y, 0.0), std::max(d.z, 0.0));
        return dMax.length() + std::min(std::max(d.x, std::max(d.y, d.z)), 0.0) - smoothing;
    }
};

// Plane implicit surface
class Plane : public ImplicitSurface {
private:
    Vec3<double> normal;
    double distance;

public:
    Plane(const Vec3<double>& normal, double distance)
        : normal(normal.normalize()), distance(distance) {}

    double evaluate(const Vec3<double>& point) const override {
        return normal.dot(point) + distance;
    }
};

// Cylinder implicit surface
class Cylinder : public ImplicitSurface {
private:
    Vec3<double> start, end;
    double radius;

public:
    Cylinder(const Vec3<double>& start, const Vec3<double>& end, double radius)
        : start(start), end(end), radius(radius) {}

    double evaluate(const Vec3<double>& point) const override {
        Vec3<double> axis = end - start;
        double length = axis.length();
        axis = axis.normalize();

        Vec3<double> relativePos = point - start;
        double dot = relativePos.dot(axis);
        dot = std::max(0.0, std::min(length, dot));

        Vec3<double> closestPoint = start + axis * dot;
        return (point - closestPoint).length() - radius;
    }
};

// Boolean operation class - CSG operations
class BooleanOperation : public ImplicitSurface {
protected:
    std::shared_ptr<ImplicitSurface> left;
    std::shared_ptr<ImplicitSurface> right;

public:
    BooleanOperation(std::shared_ptr<ImplicitSurface> left,
                    std::shared_ptr<ImplicitSurface> right)
        : left(left), right(right) {}

    // Accessor methods
    std::shared_ptr<ImplicitSurface> getLeft() const { return left; }
    std::shared_ptr<ImplicitSurface> getRight() const { return right; }
};

// Union operation
class UnionOp : public BooleanOperation {
public:
    using BooleanOperation::BooleanOperation;

    double evaluate(const Vec3<double>& point) const override {
        double leftVal = left->evaluate(point);
        double rightVal = right->evaluate(point);
        return std::min(leftVal, rightVal);
    }
};

// Intersection operation
class IntersectionOp : public BooleanOperation {
public:
    using BooleanOperation::BooleanOperation;

    double evaluate(const Vec3<double>& point) const override {
        double leftVal = left->evaluate(point);
        double rightVal = right->evaluate(point);
        return std::max(leftVal, rightVal);
    }
};

// Difference operation
class DifferenceOp : public BooleanOperation {
public:
    using BooleanOperation::BooleanOperation;

    double evaluate(const Vec3<double>& point) const override {
        double leftVal = left->evaluate(point);
        double rightVal = right->evaluate(point);
        return std::max(leftVal, -rightVal);
    }
};

// Smooth boolean operations - CSG operations with smooth transitions
// Smooth union
class SmoothUnionOp : public BooleanOperation {
private:
    double k; // Smoothing factor, smaller value means smoother transition

public:
    SmoothUnionOp(std::shared_ptr<ImplicitSurface> left,
                 std::shared_ptr<ImplicitSurface> right,
                 double smoothFactor = 0.1)
        : BooleanOperation(left, right), k(smoothFactor) {}

    double evaluate(const Vec3<double>& point) const override {
        double leftVal = left->evaluate(point);
        double rightVal = right->evaluate(point);

        double h = std::max(k - std::abs(leftVal - rightVal), 0.0) / k;
        return std::min(leftVal, rightVal) - h * h * h * k * (1.0/6.0);
    }
};

// Smooth intersection
class SmoothIntersectionOp : public BooleanOperation {
private:
    double k; // Smoothing factor

public:
    SmoothIntersectionOp(std::shared_ptr<ImplicitSurface> left,
                        std::shared_ptr<ImplicitSurface> right,
                        double smoothFactor = 0.1)
        : BooleanOperation(left, right), k(smoothFactor) {}

    double evaluate(const Vec3<double>& point) const override {
        double leftVal = left->evaluate(point);
        double rightVal = right->evaluate(point);

        double h = std::max(k - std::abs(leftVal - rightVal), 0.0) / k;
        return std::max(leftVal, rightVal) + h * h * h * k * (1.0/6.0);
    }
};

// Smooth difference
class SmoothDifferenceOp : public BooleanOperation {
private:
    double k; // Smoothing factor

public:
    SmoothDifferenceOp(std::shared_ptr<ImplicitSurface> left,
                      std::shared_ptr<ImplicitSurface> right,
                      double smoothFactor = 0.1)
        : BooleanOperation(left, right), k(smoothFactor) {}

    double evaluate(const Vec3<double>& point) const override {
        double leftVal = left->evaluate(point);
        double rightVal = -right->evaluate(point);

        double h = std::max(k - std::abs(leftVal - rightVal), 0.0) / k;
        return std::max(leftVal, rightVal) + h * h * h * k * (1.0/6.0);
    }
};