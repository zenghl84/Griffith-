#include "griffith_core.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace griffith {
namespace {
void finite(double value, const char* name) {
    if (!std::isfinite(value)) throw std::invalid_argument(std::string(name) + " must be finite.");
}
void positive(double value, const char* name) {
    finite(value, name);
    if (value <= 0) throw std::invalid_argument(std::string(name) + " must be greater than zero.");
}
double checked(long double value) {
    double result = static_cast<double>(value);
    if (!std::isfinite(result)) throw std::overflow_error("Parameters exceed the numeric range.");
    if (result == 0.0 && value != 0.0L) throw std::overflow_error("Parameters are below the numeric range.");
    return result;
}
} // namespace

void Parameters::validate() const {
    positive(E_gpa, "E_gpa"); positive(gamma, "gamma");
    positive(a_mm, "a_mm"); positive(thickness_mm, "thickness_mm");
    finite(nu, "nu"); finite(sigma_mpa, "sigma_mpa");
    if (nu < 0 || nu >= 0.5) throw std::invalid_argument("nu must satisfy 0 <= nu < 0.5.");
    if (sigma_mpa < 0) throw std::invalid_argument("sigma_mpa must be nonnegative.");
    if (state != State::plane_stress && state != State::plane_strain)
        throw std::invalid_argument("Unsupported plane state.");
}

Results calculate(const Parameters& p) {
    p.validate();
    // Wider intermediates protect against avoidable overflow in SI conversion.
    long double e = static_cast<long double>(p.E_gpa) * 1e9L;
    if (p.state == State::plane_strain) e /= 1.0L - static_cast<long double>(p.nu) * p.nu;
    long double a = static_cast<long double>(p.a_mm) * 1e-3L;
    long double sigma = static_cast<long double>(p.sigma_mpa) * 1e6L;
    long double b = static_cast<long double>(p.thickness_mm) * 1e-3L;
    long double u = pi * a * a * sigma * sigma * b / e;
    long double surface = 4.0L * a * b * p.gamma;
    long double g = pi * a * sigma * sigma / e;
    long double resistance = 2.0L * p.gamma;
    double gd = checked(g), rd = checked(resistance);
    auto classification = std::abs(gd - rd) <= 1e-6 * std::max(gd, rd)
        ? Classification::critical : (gd < rd ? Classification::subcritical : Classification::supercritical);
    return {checked(e), checked(a), checked(sigma), checked(b), checked(2.0L * a * b),
            checked(u), checked(surface), checked(u - surface), gd, rd,
            checked(std::sqrt(2.0L * e * p.gamma / (pi * a)) * 1e-6L),
            sigma > 0 ? checked(2.0L * e * p.gamma / (pi * sigma * sigma) * 1e3L)
                      : std::numeric_limits<double>::infinity(),
            checked(g / resistance), classification,
            checked(static_cast<long double>(p.sigma_mpa) * std::sqrt(pi * a))};
}

std::vector<CurvePoint> curve_points(const Parameters& p, double max_a_mm, int count) {
    p.validate(); positive(max_a_mm, "max_a_mm");
    if (count < 2 || count > 1000000) throw std::invalid_argument("count must be between 2 and 1000000.");
    const Results q = calculate(p);
    const long double coefficient = pi * static_cast<long double>(q.sigma_pa) * q.sigma_pa / q.E_prime_pa;
    std::vector<CurvePoint> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        double a_mm = checked(static_cast<long double>(max_a_mm) * i / (count - 1));
        long double a = static_cast<long double>(a_mm) * 1e-3L;
        long double u = coefficient * a * a * q.B_m;
        long double surface = 4.0L * a * q.B_m * p.gamma;
        points.push_back({a_mm, checked(u), checked(surface), checked(u - surface),
                          checked(coefficient * a), q.R_j_m2});
    }
    return points;
}

const char* state_name(State state) { return state == State::plane_stress ? "plane_stress" : "plane_strain"; }
const char* classification_name(Classification classification) {
    switch (classification) {
        case Classification::subcritical: return "subcritical";
        case Classification::critical: return "critical";
        default: return "supercritical";
    }
}
} // namespace griffith
