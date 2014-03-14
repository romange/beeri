// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef EARTH_H
#define EARTH_H

namespace util {
namespace earth {

const double kEarthRadiusKm = 6371.0;

// Input: area in spherical radians (which is returned by s2 functions)
// Output: area in square kilometers.
inline double area_sq_km(double area_sra) {
  return area_sra*kEarthRadiusKm*kEarthRadiusKm;
}

inline double area_sra(double area_sq_km) {
  return area_sq_km/(kEarthRadiusKm*kEarthRadiusKm);
}

inline double km_to_radians(double arc_km) {
  // arc = R_km * theta.
  return arc_km / kEarthRadiusKm;
}

inline double radians_to_km(double radians) {
  // arc = R_km * theta.
  return radians * kEarthRadiusKm;
}

}  // namespace earth
}  // namespace util

#endif  // EARTH_H