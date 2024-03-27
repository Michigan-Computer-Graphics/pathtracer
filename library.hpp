#pragma once

#include "stb_image_write.h"

#include <cmath>
#include <tuple>
#include <string>
#include <vector>
#include <numbers>
#include <cstdint>
#include <functional>

constexpr float Infinity = std::numeric_limits<float>::infinity();
constexpr float Pi = std::numbers::pi_v<float>;

struct Vec3
{
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
	explicit Vec3(float value = 0.0f) : x(value), y(value), z(value) {}

	float x, y, z;
};

struct Ray
{
	Ray(Vec3 origin, Vec3 direction) : origin(origin), direction(direction) {}
	Ray() = default;

	Vec3 origin, direction;
};

class Scene
{
public:
	Scene() = default;

	void insert_sphere(Vec3 center, float radius, uint32_t material = 0)
	{
		spheres.emplace_back(center, radius, material);
	}

	void insert_plane(Vec3 normal, float offset, uint32_t material = 0)
	{
		planes.emplace_back(normal, offset, material);
	}

	void insert_box(Vec3 center, Vec3 size, uint32_t material = 0);

	/**
	 * Finds whether a ray intersects with a scene.
	 * @return Whether the intersection occurred.
	 * If intersected, also outputs the travelled distance, and surface normal and material.
	 */
	bool intersect(const Ray& ray, float& distance, Vec3& normal, uint32_t& material) const;

private:
	std::vector<std::tuple<Vec3, float, uint32_t>> spheres;
	std::vector<std::tuple<Vec3, float, uint32_t>> planes;
	std::vector<std::tuple<Vec3, Vec3, uint32_t>> boxes;
};

using Color = Vec3;

inline Vec3 operator+(Vec3 value, Vec3 other) { return { value.x + other.x, value.y + other.y, value.z + other.z }; }
inline Vec3 operator-(Vec3 value, Vec3 other) { return { value.x - other.x, value.y - other.y, value.z - other.z }; }
inline Vec3 operator*(Vec3 value, Vec3 other) { return { value.x * other.x, value.y * other.y, value.z * other.z }; }
inline Vec3 operator/(Vec3 value, Vec3 other) { return { value.x / other.x, value.y / other.y, value.z / other.z }; }
inline Vec3 operator*(Vec3 value, float other) { return { value.x * other, value.y * other, value.z * other }; }
inline Vec3 operator/(Vec3 value, float other) { return { value.x / other, value.y / other, value.z / other }; }
inline Vec3 operator+(Vec3 value) { return { +value.x, +value.y, +value.z }; }
inline Vec3 operator-(Vec3 value) { return { -value.x, -value.y, -value.z }; }

inline float dot(Vec3 value, Vec3 other) { return value.x * other.x + value.y * other.y + value.z * other.z; }
inline float abs_dot(Vec3 value, Vec3 other) { return std::abs(dot(value, other)); }
inline float magnitude_squared(Vec3 value) { return dot(value, value); }
inline float magnitude(Vec3 value) { return std::sqrt(magnitude_squared(value)); }
inline Vec3 reflect(Vec3 value, Vec3 normal) { return normal * (2.0f * dot(value, normal)) - value; }

/**
 * Returns if a value is very close to zero.
 * @param epsilon The threshold used to make this decision.
 */
inline bool almost_zero(float value, float epsilon = 8E-7f) { return -epsilon < value && value < epsilon; }

/**
 * Normalizes a vector.
 * Returns the zero vector if the original vector is very close to zero.
 * @return A unit vector with the same direction as the original vector.
 */
inline Vec3 normalize(Vec3 value)
{
	float squared = magnitude_squared(value);
	if (almost_zero(squared)) return Vec3(0.0f);
	return value * (1.0f / std::sqrt(squared));
}

/**
 * @return A random floating point value between 0 (inclusive) and 1 (exclusive).
 */
float random_float();

/**
 * @return A random point in the volume of a unit sphere.
 */
inline Vec3 random_in_sphere()
{
	Vec3 result;

	do
	{
		result.x = random_float();
		result.y = random_float();
		result.z = random_float();
		result = result * 2.0f - Vec3(1.0f);
	}
	while (magnitude_squared(result) > 1.0f);

	return result;
}

/**
 * @return A random point on the surface of a unit sphere.
 */
inline Vec3 random_on_sphere() { return normalize(random_in_sphere()); }

/**
 * Bounces an old ray to form a new ray.
 * @param ray The old ray.
 * @param distance Distance on the old ray to start the new ray.
 * @param direction The direction of the new ray.
 */
inline Ray bounce(const Ray& ray, float distance, Vec3 direction)
{
	Vec3 point = ray.origin + ray.direction * distance;
	point = point + direction * 1E-4f; //Avoids shadow acne problem
	return { point, direction };
}

/**
 * Flips incident to be on the same side of a surface as outgoing.
 * @param normal The normal vector that describes the surface.
 */
inline void make_same_side(Vec3 outgoing, Vec3 normal, Vec3& incident)
{
	float dot_o = dot(outgoing, normal);
	float dot_i = dot(incident, normal);

	//Flip the vector to be on the same side as normal
	if (dot_o * dot_i < 0.0f) incident = reflect(-incident, normal);
}

/**
 * Returns the luminance value of a color.
 * This can be thought of as the visually perceived brightness.
 */
inline float get_luminance(Color color) { return dot(color, { 0.212671f, 0.715160f, 0.072169f }); }

/**
 * Executes an action in parallel, taking advantage of multiple threads.
 * @param begin The first index to execute (inclusive).
 * @param end One past the last index to execute (exclusive).
 * @param action The action to execute in parallel.
 */
void parallel_for(uint32_t begin, uint32_t end, const std::function<void(uint32_t)>& action);

/**
 * Outputs a series of colors as a PNG image file.
 */
void write_image(const std::string& filename, uint32_t width, uint32_t height, const Color* colors);

inline float fresnel_cos_i(float cos_o, float eta)
{
	float sin_outgoing2 = 1.0f - cos_o * cos_o;
	float sin_incident2 = eta * eta * sin_outgoing2;
	if (sin_incident2 >= 1.0f) return 0.0f;
	return std::sqrt(1.0f - sin_incident2);
}

inline bool fresnel_total_internal_reflection(float cos_i) { return almost_zero(cos_i); }

inline Vec3 fresnel_refract(Vec3 outgoing, Vec3 normal, float eta, float cos_i)
{
	float cos_o = dot(outgoing, normal);
	if (cos_o > 0.0f) cos_i = -cos_i;
	return normalize(normal * (eta * cos_o + cos_i) - outgoing * eta);
}

inline float fresnel_value(float eta, float cos_o, float cos_i)
{
	if (fresnel_total_internal_reflection(cos_i)) return 1.0f;
	cos_o = std::abs(cos_o);
	cos_i = std::abs(cos_i);

	float eta_r = 1.0f / eta;
	float para0 = cos_o * eta_r;
	float para1 = cos_i;
	float perp0 = cos_o;
	float perp1 = cos_i * eta_r;

	float para = (para0 - para1) / (para0 + para1);
	float perp = (perp0 - perp1) / (perp0 + perp1);
	return (para * para + perp * perp) / 2.0f;
}
