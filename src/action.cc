/*
 *
 *    Copyright (c) 2014-2017
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "include/action.h"

#include <assert.h>
#include <algorithm>
#include <sstream>

#include "include/angles.h"
#include "include/constants.h"
#include "include/kinematics.h"
#include "include/logging.h"
#include "include/pauliblocking.h"
#include "include/processbranch.h"
#include "include/quantumnumbers.h"

namespace smash {

Action::Action(const ParticleList &in_part, double time)
    : incoming_particles_(in_part),
      time_of_execution_(time + in_part[0].position().x0()) {}

Action::~Action() = default;

bool Action::is_valid(const Particles &particles) const {
  return std::all_of(
      incoming_particles_.begin(), incoming_particles_.end(),
      [&particles](const ParticleData &p) { return particles.is_valid(p); });
}

bool Action::is_pauli_blocked(const Particles &particles,
                              const PauliBlocker &p_bl) const {
  // Wall-crossing actions should never be blocked: currently
  // if the action is blocked, a particle continues to propagate in a straight
  // line. This would simply bring it out of the box.
  if (process_type_ == ProcessType::Wall) {
    return false;
  }
  const auto &log = logger<LogArea::PauliBlocking>();
  for (const auto &p : outgoing_particles_) {
    if (p.is_baryon()) {
      const auto f =
          p_bl.phasespace_dens(p.position().threevec(), p.momentum().threevec(),
                               particles, p.pdgcode(), incoming_particles_);
      if (f > Random::uniform(0., 1.)) {
        log.debug("Action ", *this, " is pauli-blocked with f = ", f);
        return true;
      }
    }
  }
  return false;
}

const ParticleList &Action::incoming_particles() const {
  return incoming_particles_;
}

void Action::update_incoming(const Particles &particles) {
  for (auto &p : incoming_particles_) {
    p = particles.lookup(p);
  }
}

FourVector Action::get_interaction_point() const {
  // Estimate for the interaction point in the calculational frame
  FourVector interaction_point = FourVector(0., 0., 0., 0.);
  for (const auto &part : incoming_particles_) {
    interaction_point += part.position();
  }
  interaction_point /= incoming_particles_.size();
  return interaction_point;
}

std::pair<double, double> Action::get_potential_at_interaction_point() const {
  const ThreeVector r = get_interaction_point().threevec();
  double UB, UI3;
  /* Check:
   * 1. Potential is turned on
   * 2. Lattice is turned on
   * 3. Particle is inside the lattice. */
  const bool UB_exist =
            ((UB_lat_ != nullptr) ? UB_lat_->value_at(r, UB) : false);
  const bool UI3_exist =
            ((UI3_lat_ != nullptr) ? UI3_lat_->value_at(r, UI3) : false);
  const double B_pot = (UB_exist ? UB : 0.0);
  const double I3_pot = (UI3_exist ? UI3 : 0.0);
  return std::make_pair(B_pot, I3_pot);
}

void Action::perform(Particles *particles, uint32_t id_process) {
  assert(id_process != 0);
  const auto &log = logger<LogArea::Action>();

  for (ParticleData &p : outgoing_particles_) {
    // store the history info
    if (process_type_ != ProcessType::Wall) {
      p.set_history(p.get_history().collisions_per_particle + 1, id_process,
                    process_type_, time_of_execution_, incoming_particles_);
    }
  }

  // For elastic collisions and box wall crossings it is not necessary to remove
  // particles from the list and insert new ones, it is enough to update their
  // properties.
  particles->update(incoming_particles_, outgoing_particles_,
                    (process_type_ != ProcessType::Elastic) &&
                        (process_type_ != ProcessType::Wall));

  log.debug("Particle map now has ", particles->size(), " elements.");

  /* Check the conservation laws if the modifications of the total kinetic
   * energy of the outgoing particles by the mean field potentials are not
   * taken into account. */
  if (UB_lat_ == nullptr && UI3_lat_ == nullptr) {
     check_conservation(id_process);
  }
}

double Action::kinetic_energy_cms() const {
//  const auto &log = logger<LogArea::Action>();
  /* scale_B returns the difference of the total force scales of the skyrme
   * potential between the initial and final states. */
  double scale_B = 0.0;
  /* scale_I3 returns the difference of the total force scales of the symmetry
   * potential between the initial and final states. */
  double scale_I3 = 0.0;
// std::string in_particle = "";
// std::string out_particle = "";
// std::string in_scale = "";
// std::string out_scale = "";
// std::string in_position = "";
// std::string out_position = "";
  for (const auto &p_in : incoming_particles_) {
       /* Get the force scale of the incoming particle. */
       const auto scale = ((pot_ != nullptr) ? pot_->force_scale(p_in.type())
                           : std::make_pair(0.0, 0));
       scale_B += scale.first;
       scale_I3 += scale.second * p_in.type().isospin3_rel();
   //   in_particle += p_in.type().name() + "(" + std::to_string(p_in.id())+")";
   //   in_scale += ("(" + std::to_string(scale.first) + ", "
   //      + std::to_string(scale.second * p_in.type().isospin3_rel()) + ") ");
   //   in_position += ("(" + std::to_string(p_in.position().x1()) + ", "
   //                + std::to_string(p_in.position().x2()) + ", "
   //                + std::to_string(p_in.position().x3()) + ") ");
  }
  for (const auto &p_out : outgoing_particles_) {
       const auto scale = ((pot_ != nullptr) ? pot_->force_scale(p_out.type())
                           : std::make_pair(0.0, 0));
       scale_B -= scale.first;
       scale_I3 -= scale.second * p_out.type().isospin3_rel();
   //   out_particle += p_out.type().name();
   //   out_scale += ("(" + std::to_string(scale.first) + ", "
   //      + std::to_string(scale.second * p_out.type().isospin3_rel()) + ") ");
   //   out_position += ("(" + std::to_string(p_out.position().x1()) + ", "
   //                + std::to_string(p_out.position().x2()) + ", "
   //                + std::to_string(p_out.position().x3()) + ") ");
  }
  const auto potentials = get_potential_at_interaction_point();
  /* Rescale to get the potential difference between the 
   * initial and final state.*/
  const double B_pot_diff = potentials.first * scale_B;
  const double I3_pot_diff = potentials.second * scale_I3;
// if (scale_B > really_small) {
//    log.info("reaction: ", in_particle, "->", out_particle, ", DU = ", 
//             std::to_string(B_pot_diff + I3_pot_diff));
//    log.info("force scale: ", in_scale, " |  ", out_scale);
//    log.info("position: ", in_position, " |  ", out_position);
// }
  return sqrt_s() + B_pot_diff + I3_pot_diff;
}

double Action::kinetic_energy_cms(std::pair<double, double> potentials,
           ParticleTypePtrList p_out_types) const {
  /* scale_B returns the difference of the total force scales of the skyrme
   * potential between the initial and final states. */
  double scale_B = 0.0;
  /* scale_I3 returns the difference of the total force scales of the symmetry
   * potential between the initial and final states. */
  double scale_I3 = 0.0;
// std::string in_particle = "";
// std::string out_particle = "";
// std::string in_scale = "";
// std::string out_scale = "";
// std::string in_position = "";
  for (const auto &p_in : incoming_particles_) {
       /* Get the force scale of the incoming particle. */
       const auto scale = ((pot_ != nullptr) ? pot_->force_scale(p_in.type())
                           : std::make_pair(0.0, 0));
       scale_B += scale.first;
       scale_I3 += scale.second * p_in.type().isospin3_rel();
    //  in_particle += p_in.type().name() + "(" + std::to_string(p_in.id())+")";
    //  in_scale += ("(" + std::to_string(scale.first) + ", "
    //     + std::to_string(scale.second * p_in.type().isospin3_rel()) + ") ");
    //  in_position += ("(" + std::to_string(p_in.position().x1()) + ", "
    //               + std::to_string(p_in.position().x2()) + ", "
    //               + std::to_string(p_in.position().x3()) + ") ");
  }
  for (const auto &p_out : p_out_types) {
       const auto scale = ((pot_ != nullptr) ? pot_->force_scale(*p_out)
                           : std::make_pair(0.0, 0));
       scale_B -= scale.first;
       scale_I3 -= scale.second * p_out->isospin3_rel();
     // out_particle += p_out->name();
     // out_scale += ("(" + std::to_string(scale.first) + ", "
     //    + std::to_string(scale.second * p_out->isospin3_rel()) + ") ");
  }
  /* Rescale to get the potential difference between the 
   * initial and final state.*/
  const double B_pot_diff = potentials.first * scale_B;
  const double I3_pot_diff = potentials.second * scale_I3;
 //if (scale_B > really_small) {
 //   std::cout << "reaction: " << in_particle << "->" << out_particle << ", DU = " <<
 //            std::to_string(B_pot_diff + I3_pot_diff) << std::endl;
 //   std::cout << "force scale: " << in_scale << " |  " << out_scale << std::endl;
 //   std::cout << "position: " << in_position  << std::endl;
 //}
  return sqrt_s() + B_pot_diff + I3_pot_diff;
}

std::pair<double, double> Action::sample_masses() const {
  const ParticleType &t_a = outgoing_particles_[0].type();
  const ParticleType &t_b = outgoing_particles_[1].type();
  // start with pole masses
  std::pair<double, double> masses = {t_a.mass(), t_b.mass()};

  const double cms_kin_energy = kinetic_energy_cms();

  if (cms_kin_energy < t_a.min_mass_kinematic() + t_b.min_mass_kinematic()) {
    const std::string reaction = incoming_particles_[0].type().name() +
                                 incoming_particles_[1].type().name() + "→" +
                                 t_a.name() + t_b.name();
    throw InvalidResonanceFormation(
        reaction + ": not enough energy, " + std::to_string(cms_kin_energy) +
        " < " + std::to_string(t_a.min_mass_kinematic()) + " + " +
        std::to_string(t_b.min_mass_kinematic()));
  }

  /* If one of the particles is a resonance, sample its mass. */
  if (!t_a.is_stable() && t_b.is_stable()) {
    masses.first = t_a.sample_resonance_mass(t_b.mass(), cms_kin_energy);
  } else if (!t_b.is_stable() && t_a.is_stable()) {
    masses.second = t_b.sample_resonance_mass(t_a.mass(), cms_kin_energy);
  } else if (!t_a.is_stable() && !t_b.is_stable()) {
    // two resonances in final state
    masses = t_a.sample_resonance_masses(t_b, cms_kin_energy);
  }
  return masses;
}

void Action::sample_angles(std::pair<double, double> masses) {
  const auto &log = logger<LogArea::Action>();

  ParticleData *p_a = &outgoing_particles_[0];
  ParticleData *p_b = &outgoing_particles_[1];

  const double cms_kin_energy = kinetic_energy_cms();

  const double pcm = pCM(cms_kin_energy, masses.first, masses.second);
  if (!(pcm > 0.0)) {
    log.warn("Particle: ", p_a->pdgcode(), " radial momentum: ", pcm);
    log.warn("Ektot: ", cms_kin_energy, " m_a: ", masses.first,
             " m_b: ", masses.second);
  }
  /* Here we assume an isotropic angular distribution. */
  Angles phitheta;
  phitheta.distribute_isotropically();

  p_a->set_4momentum(masses.first, phitheta.threevec() * pcm);
  p_b->set_4momentum(masses.second, -phitheta.threevec() * pcm);

  log.debug("p_a: ", *p_a, "\np_b: ", *p_b);
}

void Action::sample_2body_phasespace() {
  /* This function only operates on 2-particle final states. */
  assert(outgoing_particles_.size() == 2);
  // first sample the masses
  const std::pair<double, double> masses = sample_masses();
  // after the masses are fixed (and thus also pcm), sample the angles
  sample_angles(masses);
}

void Action::check_conservation(const uint32_t id_process) const {
  QuantumNumbers before(incoming_particles_);
  QuantumNumbers after(outgoing_particles_);
  if (before != after) {
    std::stringstream particle_names;
    for (const auto &p : incoming_particles_) {
      particle_names << p.type().name();
    }
    particle_names << " vs. ";
    for (const auto &p : outgoing_particles_) {
      particle_names << p.type().name();
    }
    particle_names << "\n";
    const auto &log = logger<LogArea::Action>();
    std::string err_msg = before.report_deviations(after);
    log.error() << particle_names.str() << err_msg;
    // Pythia does not conserve energy and momentum at high energy, so we just
    // print the error and continue.
    if ((process_type_ == ProcessType::StringSoft) ||
        (process_type_ == ProcessType::StringHard)) {
      return;
    }
    if (id_process == ID_PROCESS_PHOTON) {
      throw std::runtime_error("Conservation laws violated in photon process");
    } else {
      throw std::runtime_error("Conservation laws violated in process " +
                               std::to_string(id_process));
    }
  }
}

std::ostream &operator<<(std::ostream &out, const ActionList &actions) {
  out << "ActionList {\n";
  for (const auto &a : actions) {
    out << "- " << a << '\n';
  }
  return out << '}';
}

}  // namespace smash
