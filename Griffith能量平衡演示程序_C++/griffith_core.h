#ifndef GRIFFITH_CORE_H
#define GRIFFITH_CORE_H

#include <string>
#include <vector>

namespace griffith {
constexpr double pi = 3.1415926535897932384626433832795;
enum class State { plane_stress, plane_strain };
enum class Classification { subcritical, critical, supercritical };

struct Parameters {
    double E_gpa = 70.0;
    double nu = 0.22;
    double gamma = 1.0;
    double a_mm = 0.5;
    double sigma_mpa = 8.0;
    double thickness_mm = 5.0;
    State state = State::plane_stress;
    void validate() const;
};

// A is the projected crack area (2aB), with two free surfaces of total area 2A.
// U_release is effective mechanical energy release, not stored strain energy.
struct Results {
    double E_prime_pa, a_m, sigma_pa, B_m, A_m2;
    double U_release_j, S_surface_j, net_release_j;
    double G_j_m2, R_j_m2, sigma_c_mpa, a_c_mm, ratio;
    Classification classification;
    double K_mpa_sqrt_m;
};

struct CurvePoint {
    double a_mm, U_release_j, S_surface_j, net_release_j, G_j_m2, R_j_m2;
};

Results calculate(const Parameters& p);
std::vector<CurvePoint> curve_points(const Parameters& p, double max_a_mm, int count = 181);
const char* state_name(State state);
const char* classification_name(Classification classification);
} // namespace griffith
#endif
