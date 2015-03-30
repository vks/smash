/*
 *
 *    Copyright (c) 2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "unittest.h"
#include "../include/boxmodus.h"
#include "../include/configuration.h"
#include "../include/experiment.h"
#include "../include/nucleus.h"
#include "../include/pauliblocking.h"
#include "../include/potentials.h"

#include <boost/filesystem.hpp>

using namespace Smash;

TEST(init_particle_types) {
  ParticleType::create_type_list(
      "# NAME MASS[GEV] WIDTH[GEV] PDG\n"
      "mock_De 0.1 0.0 2114\n"
      "proton 0.938 0.0 2212\n"
      "neutron 0.938 0.0 2112\n");
}

/* Checks if phase space density gives correct result
   for a particular simple case: one particle in the phase-space sphere.
*/
TEST(phase_space_density) {
  Configuration conf(TEST_CONFIG_PATH);
  conf["Collision_Term"]["Pauli_Blocking"]["Spatial_Averaging_Radius"] = 1.86;
  conf["Collision_Term"]["Pauli_Blocking"]["Momentum_Averaging_Radius"] = 0.08;
  conf["Collision_Term"]["Pauli_Blocking"]["Gaussian_Cutoff"] = 2.2;

  ExperimentParameters param{{0.f, 1.f}, 1.f, 1, 1.0};
  PauliBlocker *pb = new PauliBlocker(conf["Collision_Term"]["Pauli_Blocking"], param);
  Particles part;
  PdgCode pdg = 0x2112;
  ParticleData one_particle{ParticleType::find(pdg)};
  one_particle.set_4position(FourVector(0.0, 0.0, 0.0, 0.0));
  one_particle.set_4momentum(0.0, 0.0, 0.0, 0.0);
  part.add_data(one_particle);
  VERIFY(part.size() == 1);
  for(int i = 0; i < 30; i++) {
    ThreeVector r(i/30.0*4.06, 0.0, 0.0), p(0.0, 0.0, 0.0);
    const float f = pb->phasespace_dens(r, p, part, pdg);
    std::cout << "r[fm] = " << r.x1() << " f = " << f << std::endl;
    // const float f_expected = 6.6217f;
    // COMPARE_RELATIVE_ERROR(f, f_expected, 1.e-4) << f << " ?= " << f_expected;
  }
}

/*TEST(phase_space_density_box) {
  Configuration conf(TEST_CONFIG_PATH);
  conf["Modi"]["Box"]["Initial_Condition"] = 1;
  conf["Modi"]["Box"]["Length"] = 5.0;
  conf["Modi"]["Box"]["Temperature"] = 1.0;
  conf["Modi"]["Box"]["Start_Time"] = 0.0;
  conf.take({"Modi", "Box", "Init_Multiplicities"});
  conf["Modi"]["Box"]["Init_Multiplicities"]["2114"] = 10000;
  conf["Collision_Term"]["Pauli_Blocking"]["Spatial_Averaging_Radius"] = 1.86;
  conf["Collision_Term"]["Pauli_Blocking"]["Momentum_Averaging_Radius"] = 0.32;
  conf["Collision_Term"]["Pauli_Blocking"]["Gaussian_Cutoff"] = 2.2;
  // Number of test-particles
  const size_t ntest = 10;
  ExperimentParameters param{{0.f, 1.f}, 1.f, ntest, 1.0};
  PauliBlocker *pb = new PauliBlocker(conf["Collision_Term"]["Pauli_Blocking"], param);
  BoxModus *b = new BoxModus(conf["Modi"], param);
  Particles Pbox;
  b->initial_conditions(&Pbox, param);
  ThreeVector r(0.0, 0.0, 0.0);
  ThreeVector p;
  PdgCode pdg = 0x2114;
  float f;
  for (int i = 1; i < 100; i++) {
    p = ThreeVector(0.0, 0.0, 5.0/100*i);
    f = pb->phasespace_dens(r, p, Pbox, pdg);
   // std::cout << 5.0/100*i << "  " << f << std::endl;
  }
}*/

TEST(phase_space_density_nucleus) {
  Configuration conf(TEST_CONFIG_PATH);
  conf["Collision_Term"]["Pauli_Blocking"]["Spatial_Averaging_Radius"] = 1.86;
  conf["Collision_Term"]["Pauli_Blocking"]["Momentum_Averaging_Radius"] = 0.08;
  conf["Collision_Term"]["Pauli_Blocking"]["Gaussian_Cutoff"] = 2.2;

  // Gold nuclei with 1000 test-particles
  std::map<PdgCode, int> lead_list = {{0x2212, 79}, {0x2112, 118}};
  int Ntest = 100;
  Nucleus Au;
  Au.fill_from_list(lead_list, Ntest);
  Au.set_parameters_automatic();
  Au.arrange_nucleons();
  Au.generate_fermi_momenta();

  Particles part_Au;
  Au.copy_particles(&part_Au);

  ExperimentParameters param{{0.f, 1.f}, 1.f, Ntest, 1.0};
  PauliBlocker *pb = new PauliBlocker(conf["Collision_Term"]["Pauli_Blocking"], param);

  ThreeVector r(0.0, 0.0, 0.0);
  ThreeVector p;
  PdgCode pdg = 0x2212;
  float f;
  for (int i = 1; i < 100; i++) {
    p = ThreeVector(0.0, 0.0, 0.5/100*i);
    f = pb->phasespace_dens(r, p, part_Au, pdg);
    std::cout << 0.5/100*i << "  " << f << std::endl;
  }
}
