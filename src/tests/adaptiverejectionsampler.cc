/*
 *
 *    Copyright (c) 2015
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */


#include <cstdio>
#include <iostream>
#include <map>

#include "unittest.h"
#include "../include/adaptiverejectionsampler.h"
#include "../include/distributions.h"

using namespace Smash;


TEST(woods_saxon_distribution_adaptive_rejection_sampling) {
  float radius = 6.4;
  float diffusiveness = 0.54;
  Rejection::AdaptiveRejectionSampler woods_saxon_sampler(
      [&](float x) {
      return x*x*woods_saxon_dist_func(x, radius, diffusiveness);}
      , 0.0, 15.0);

  // this is where we store the distribution.
  std::map<int, int> histogram {};
  // binning width for the distribution:
  constexpr float dx = 0.01;

  for ( int i = 0; i < 1000000; i++ ) {
    float r = woods_saxon_sampler.get_one_sample();
    int bin = r/dx;
    ++histogram[bin];
  }

  float R = radius;
  float value_at_radius = histogram.at(R/dx);
  float expected_at_radius = R*R*woods_saxon_dist_func(R,
                                                       radius, diffusiveness);

  float probes[9] = {1.0, 5.0, 7.2, 8.0, 8.5, .5f*R, 1.1f*R, 1.2f*R, 1.3f*R};
  // now do probe these values:
  for (int i = 0; i < 9; ++i) {
    // value we have simulated:
    float value = histogram.at(probes[i]/dx)/value_at_radius;
    // value we have expected:
    float expec = probes[i]*probes[i]*woods_saxon_dist_func(probes[i],
                            radius, diffusiveness)/expected_at_radius;
    // standard error we expect the histogram to have is 1/sqrt(N); we
    // give 3 sigma "space".
    float margin = 3.0/sqrt(value);
    VERIFY(std::abs(value - expec) < margin) << " x = " << probes[i]
        << ": simulated: " << value
        << " vs. calculated: " << expec
        << " (allowed distance: " << margin << ")";
  }
}


TEST(juttner_distribution_adaptive_rejection_sampling) {
  double mass = 0.938;
  double temperature = 0.15;
  double baryon_chemical_potential = 0.0;
  double fermion_boson_factor = 0.0;

  double lowlim = 0.0;
  double highlim = 15.0;
  Rejection::AdaptiveRejectionSampler juttner_sampler(
      [&](double x) {
      return x*x*juttner_distribution_func(x, mass, temperature,
                 baryon_chemical_potential, fermion_boson_factor);}
      , lowlim, highlim);

  // this is where we store the distribution.
  std::map<int, int> histogram {};
  // binning width for the distribution:
  constexpr double dx = 0.01;

  /** Notice that the probability is really small for r>3 or r->0;
   * Big number of samplings is needed to make sure the test passed.
   * change to the following code to do unittest for scott's method
   * double r = sample_momenta1(temperature, mass);
   * change to the following code to do unittest for native rejection
   * double r = sample_momenta(temperature, mass);
   * notice that the native rejection is slow, for 10million samplings
   * it may take longer than 16s with the new random generator; (and
   * the native rejection can not pass this test) */
  for ( int i = 0; i < 10000000; i++ ) {
    double r = juttner_sampler.get_one_sample();
    // double r = sample_momenta_fast(temperature, mass);
    int bin = r/dx;
    ++histogram[bin];
  }

  double R = 1.0;
  double value_at_radius = histogram.at(R/dx);
  double expected_at_radius = R*R*juttner_distribution_func(R, mass,
           temperature, baryon_chemical_potential, fermion_boson_factor);

  double probes[9] = {0.1, 0.5, 0.7, 1.0, 1.5,
    0.0001f*R, 2.0f*R, 2.5f*R, 3.0f*R};
  // now do probe these values:
  for (int i = 0; i < 9; ++i) {
    // value we have simulated:
    double value = histogram.at(probes[i]/dx)/value_at_radius;
    // value we have expected:
    double expec = probes[i]*probes[i]*juttner_distribution_func(
        probes[i], mass, temperature, baryon_chemical_potential,
        fermion_boson_factor)/expected_at_radius;

    // standard error we expect the histogram to have is 1/sqrt(N); we
    // give 3 sigma "space".
    double margin = 3.0/sqrt(value);
    VERIFY(std::abs(value - expec) < margin) << " x = " << probes[i]
        << ": simulated: " << value
        << " vs. calculated: " << expec
        << " (allowed distance: " << margin << ")";
  }
}
