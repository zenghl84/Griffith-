#include "griffith_core.h"
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace griffith;
int assertions = 0;
void check(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}
void relative(double value, double expected) {
    check(std::abs(value - expected) <= std::max(1e-14, 1e-11 * std::max(std::abs(value), std::abs(expected))),
          "Numeric reference mismatch");
}
void rejects(const std::function<void()>& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Invalid input was accepted");
}

void run_tests() {
    Parameters p;
    auto r = calculate(p);
    check(r.E_prime_pa == 70e9 && r.a_m == 0.0005 && r.sigma_pa == 8e6 && r.B_m == 0.005, "SI conversion");
    relative(r.A_m2, 0.000005); relative(r.S_surface_j, 0.00001);
    relative(r.G_j_m2, 1.4361566416410483); relative(r.U_release_j, 0.0000035903916041026205);
    relative(r.sigma_c_mpa, 9.440697438826297); relative(r.a_c_mm, 0.6963028760270421);
    check(r.classification == Classification::subcritical, "Default classification");

    p.E_gpa = 205; p.nu = 0.3; p.gamma = 1.8; p.a_mm = 1.2; p.sigma_mpa = 15;
    for (State state : {State::plane_stress, State::plane_strain}) {
        p.state = state; auto q = calculate(p);
        relative(std::pow(q.K_mpa_sqrt_m * 1e6, 2) / q.E_prime_pa, q.G_j_m2);
        relative(std::pow(p.sigma_mpa / q.sigma_c_mpa, 2), q.ratio);
        relative(p.a_mm / q.a_c_mm, q.ratio);
        Parameters c = p; c.sigma_mpa = q.sigma_c_mpa;
        check(calculate(c).classification == Classification::critical, "Critical stress");
        relative(calculate(c).G_j_m2, q.R_j_m2);
    }
    p = Parameters{}; p.nu = 0.3; auto stress = calculate(p);
    p.state = State::plane_strain; auto strain = calculate(p); double factor = 1 - p.nu * p.nu;
    relative(strain.E_prime_pa, stress.E_prime_pa / factor);
    relative(strain.G_j_m2, stress.G_j_m2 * factor);
    relative(strain.U_release_j, stress.U_release_j * factor);
    relative(strain.sigma_c_mpa, stress.sigma_c_mpa / std::sqrt(factor));
    relative(strain.a_c_mm, stress.a_c_mm / factor);
    relative(strain.S_surface_j, stress.S_surface_j); relative(strain.K_mpa_sqrt_m, stress.K_mpa_sqrt_m);

    p = Parameters{}; p.a_mm = 1; p.sigma_mpa = 12;
    auto center = calculate(p); p.a_mm -= 1e-4; auto low = calculate(p);
    p.a_mm += 2e-4; auto high = calculate(p); double da = high.A_m2 - low.A_m2;
    relative((high.U_release_j - low.U_release_j) / da, center.G_j_m2);
    relative((high.S_surface_j - low.S_surface_j) / da, center.R_j_m2);
    relative((high.net_release_j - low.net_release_j) / da, center.G_j_m2 - center.R_j_m2);

    p = Parameters{}; p.a_mm = calculate(p).a_c_mm; auto critical = calculate(p);
    check(critical.classification == Classification::critical, "Critical length");
    relative(critical.U_release_j, critical.S_surface_j / 2);
    check(critical.net_release_j < 0, "Critical total energy remains below crossing");
    p.a_mm *= 2; auto crossing = calculate(p);
    relative(crossing.U_release_j, crossing.S_surface_j); relative(crossing.ratio, 2);
    check(crossing.classification == Classification::supercritical, "Energy crossing classification");

    p = Parameters{}; p.thickness_mm = 2; auto thin = calculate(p);
    p.thickness_mm = 8; auto thick = calculate(p);
    relative(thick.A_m2, thin.A_m2 * 4); relative(thick.U_release_j, thin.U_release_j * 4);
    relative(thick.S_surface_j, thin.S_surface_j * 4); relative(thick.net_release_j, thin.net_release_j * 4);
    relative(thick.G_j_m2, thin.G_j_m2); relative(thick.R_j_m2, thin.R_j_m2);
    relative(thick.sigma_c_mpa, thin.sigma_c_mpa); relative(thick.a_c_mm, thin.a_c_mm);

    p = Parameters{}; p.sigma_mpa = 0; auto zero = calculate(p);
    check(std::isinf(zero.a_c_mm) && zero.G_j_m2 == 0 && zero.K_mpa_sqrt_m == 0 && zero.ratio == 0, "Zero stress");
    check(zero.classification == Classification::subcritical, "Zero stress classification");
    auto points = curve_points(p, 4, 5);
    for (int i = 0; i < 5; ++i) check(points[i].a_mm == i && points[i].U_release_j == 0, "Zero stress curve");
    p = Parameters{}; points = curve_points(p, 1);
    check(points.size() == 181 && points.front().a_mm == 0 && points.front().S_surface_j == 0 && points.back().a_mm == 1, "Curve origin/endpoints");
    relative(points[90].G_j_m2, r.G_j_m2); relative(points[90].U_release_j, r.U_release_j);
    relative(points[90].S_surface_j, r.S_surface_j); relative(points[90].net_release_j, r.net_release_j);

    for (double multiplier : {1 - 2e-7, 1.0, 1 + 2e-7}) {
        p = Parameters{}; p.sigma_mpa = r.sigma_c_mpa * multiplier;
        check(calculate(p).classification == Classification::critical, "Critical tolerance");
    }
    p.sigma_mpa = r.sigma_c_mpa * .999; check(calculate(p).classification == Classification::subcritical, "Below tolerance");
    p.sigma_mpa = r.sigma_c_mpa * 1.001; check(calculate(p).classification == Classification::supercritical, "Above tolerance");
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        for (int field : {0, 2, 3, 5}) {
            p = Parameters{};
            double* values[] = {&p.E_gpa, &p.nu, &p.gamma, &p.a_mm, &p.sigma_mpa, &p.thickness_mm};
            *values[field] = invalid; rejects([&] { calculate(p); });
        }
        rejects([&] { curve_points(Parameters{}, invalid); });
    }
    for (double invalid : {-0.1, 0.5, 1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        p = Parameters{}; p.nu = invalid; rejects([&] { calculate(p); });
    }
    for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        p = Parameters{}; p.sigma_mpa = invalid; rejects([&] { calculate(p); });
    }
    p = Parameters{}; p.state = static_cast<State>(99); rejects([&] { calculate(p); });
    for (int count : {0, 1, -2}) rejects([&] { curve_points(Parameters{}, 2, count); });
    p = Parameters{}; p.a_mm = 10; check(calculate(p).a_m == .01, "Direct entry outside slider range");
    p.E_gpa = 1e308; rejects([&] { calculate(p); });
}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--reference") {
            // Machine-readable cases for independent comparison with the Python original.
            std::cout << std::setprecision(17);
            for (int i = 0; i < 24; ++i) {
                Parameters p;
                p.E_gpa = 20 + i * 9; p.nu = 0.1 + (i % 8) * .04;
                p.gamma = 0.4 + i * .2; p.a_mm = 0.03 + i * .13;
                p.sigma_mpa = i * 1.1; p.thickness_mm = 1 + i * .4;
                p.state = i % 2 ? State::plane_strain : State::plane_stress;
                auto r = calculate(p);
                std::cout << p.E_gpa << ',' << p.nu << ',' << p.gamma << ',' << p.a_mm << ','
                          << p.sigma_mpa << ',' << p.thickness_mm << ',' << state_name(p.state) << ','
                          << r.E_prime_pa << ',' << r.A_m2 << ',' << r.U_release_j << ',' << r.S_surface_j << ','
                          << r.net_release_j << ',' << r.G_j_m2 << ',' << r.R_j_m2 << ',' << r.sigma_c_mpa << ','
                          << r.a_c_mm << ',' << r.ratio << ',' << r.K_mpa_sqrt_m << ',' << classification_name(r.classification) << '\n';
            }
        } else { run_tests(); std::cout << "PASS: " << assertions << " physical and input checks.\n"; }
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
