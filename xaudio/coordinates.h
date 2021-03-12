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

inline double rad2degree(double input) {
	return input * (180 / PI);
}

// Gets spherical coords in degrees to cartesian 3D
inline cartesian_coordinates3d spherical2cartesian(spherical_coordinates input) {
	return cartesian_coordinates3d {
		input.radius * cos(input.azimuth) * cos(input.elevation),	// x
		input.radius * sin(input.azimuth) * cos(input.elevation),	// y
		input.radius * sin(input.elevation),						// z
	};
}

// Gets cartesian coords in degrees to spherical in radians
inline spherical_coordinates cartesian3d2spherical(cartesian_coordinates3d input) {
	return spherical_coordinates {
		sqrt(pow(input.x, 2) + pow(input.y, 2) + pow(input.z, 2)),	// radius
		atan2(input.y, input.x),									// azimuth
		atan(sqrt(pow(input.x, 2) + pow(input.y, 2)) / input.z),	// elevation
	};
}