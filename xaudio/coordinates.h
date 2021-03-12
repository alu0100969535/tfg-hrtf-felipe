#pragma once
#include <math.h>
#include "filter_data.h"

struct cartesian_coordinates3d {
	double x;
	double y;
	double z;
};

struct spherical_coordinates {
	double radius;
	double azimuth;
	double elevation;
};

inline double degree2rad(double input) {
	return input * (PI / 180);
}

// Gets spherical coords in degrees to cartesian 3D
inline cartesian_coordinates3d cartesian3d_to_spherical(spherical_coordinates input) {
	return cartesian_coordinates3d {
		input.radius * cos(input.azimuth) * cos(input.elevation),	// x
		input.radius * sin(input.azimuth) * cos(input.elevation),	// y
		input.radius * sin(input.elevation),						// z
	};
}

// Gets cartesian coords in degrees to spherical in radians
inline spherical_coordinates spherical_to_cartesian(cartesian_coordinates3d input) {
	return spherical_coordinates {
		sqrt(pow(input.x, 2) + pow(input.y, 2) + pow(input.z, 2)),	// radius
		atan2(input.y, input.x),									// azimuth
		atan(sqrt(pow(input.x, 2) + pow(input.y, 2)) / input.z),	// elevation
	};
}