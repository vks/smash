/*
 *
 *    Copyright (c) 2014-2019
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/scatteractionmulti.h"

#include "smash/logging.h"

namespace smash {
static constexpr int LScatterActionMulti = LogArea::ScatterActionMulti::id;

ScatterActionMulti::ScatterActionMulti(const ParticleList& in_plist,
                                       double time)
    : Action(in_plist, time), total_probability_(0.) {}

void ScatterActionMulti::add_reaction(CollisionBranchPtr p) {
  add_process<CollisionBranch>(p, reaction_channels_, total_probability_);
}

void ScatterActionMulti::add_reactions(CollisionBranchList pv) {
  add_processes<CollisionBranch>(std::move(pv), reaction_channels_,
                                 total_probability_);
}

double ScatterActionMulti::get_total_weight() const {
  // TODO(stdnmr) Weight probability with xs_scaling factor?
  return total_probability_;
}

double ScatterActionMulti::get_partial_weight() const {
  // TODO(stdnmr) Weight probability with xs_scaling factor?
  return partial_probability_;
}

void ScatterActionMulti::add_possible_reactions(double dt,
                                                const double gcell_vol,
                                                const bool three_to_one) {
  if (three_to_one && incoming_particles().size() == 3 &&
      three_different_pions(incoming_particles()[0], incoming_particles()[1],
                            incoming_particles()[2])) {
    // 3pi -> omega
    const ParticleTypePtr type_omega = ParticleType::try_find(0x223);
    if (type_omega) {
      add_reaction(make_unique<CollisionBranch>(
          *type_omega, probability_three_pi_to_one(*type_omega, dt, gcell_vol),
          ProcessType::MultiParticleThreePionsToOmega));
    }
  }
}

void ScatterActionMulti::generate_final_state() {
  logg[LScatterActionMulti].debug("Incoming particles: ", incoming_particles_);

  /* Decide for a particular final state. */
  const CollisionBranch* proc =
      choose_channel<CollisionBranch>(reaction_channels_, total_probability_);
  process_type_ = proc->get_type();
  outgoing_particles_ = proc->particle_list();
  partial_probability_ = proc->weight();

  logg[LScatterActionMulti].debug("Chosen channel: ", process_type_,
                                  outgoing_particles_);

  switch (process_type_) {
    case ProcessType::MultiParticleThreePionsToOmega:
      /* n->1 annihilation */
      annihilation();
      break;
    default:
      throw InvalidScatterActionMulti(
          "ScatterActionMulti::generate_final_state: Invalid process type " +
          std::to_string(static_cast<int>(process_type_)) + " was requested.");
  }

  /* The production point of the new particles.  */
  FourVector middle_point = get_interaction_point();

  for (ParticleData& new_particle : outgoing_particles_) {
    // Boost to the computational frame
    new_particle.boost_momentum(
        -total_momentum_of_outgoing_particles().velocity());
    /* Set positions of the outgoing particles */
    new_particle.set_4position(middle_point);
  }
}

double ScatterActionMulti::probability_three_pi_to_one(
    const ParticleType& type_out, double dt, const double gcell_vol) const {
  const double e1 = incoming_particles()[0].momentum().x0();
  const double e2 = incoming_particles()[1].momentum().x0();
  const double e3 = incoming_particles()[2].momentum().x0();
  const double sqrts = sqrt_s();

  const double gamma_decay = type_out.get_partial_width(
      sqrts, {&incoming_particles()[0].type(), &incoming_particles()[1].type(),
              &incoming_particles()[2].type()});

  const int spin_deg = type_out.spin_degeneracy();
  const double I_3_pi = 0.07514;  // approx. ATM (value at omega pole mass)
  const double ph_sp_3 =
      1. / (8 * M_PI * M_PI * M_PI) * 1. / (16 * sqrts * sqrts) * I_3_pi;

  const double spec_f_val = type_out.spectral_function(sqrts);

  return dt / (gcell_vol * gcell_vol) * M_PI / (4. * e1 * e2 * e3) *
         gamma_decay / ph_sp_3 * spec_f_val * std::pow(hbarc, 5.0) * spin_deg;
}

void ScatterActionMulti::annihilation() {
  if (outgoing_particles_.size() != 1) {
    std::string s =
        "Annihilation: "
        "Incorrect number of particles in final state: ";
    s += std::to_string(outgoing_particles_.size()) + ".";
    throw InvalidScatterActionMulti(s);
  }
  // Set the momentum of the formed particle in its rest frame.
  outgoing_particles_[0].set_4momentum(
      total_momentum_of_outgoing_particles().abs(), 0., 0., 0.);

  // TODO(stdnmr) Set formation (time) of outgoing particle?

  logg[LScatterActionMulti].debug("Momentum of the new particle: ",
                                  outgoing_particles_[0].momentum());
}

bool ScatterActionMulti::three_different_pions(
    const ParticleData& data_a, const ParticleData& data_b,
    const ParticleData& data_c) const {
  // We want a combination of pi+, pi- and pi0
  const PdgCode pdg_a = data_a.pdgcode();
  const PdgCode pdg_b = data_b.pdgcode();
  const PdgCode pdg_c = data_c.pdgcode();

  return (pdg_a.is_pion() && pdg_b.is_pion() && pdg_c.is_pion()) &&
         (pdg_a != pdg_b && pdg_b != pdg_c && pdg_c != pdg_a);
}

void ScatterActionMulti::format_debug_output(std::ostream& out) const {
  out << "MultiParticleScatter of " << incoming_particles_;
  if (outgoing_particles_.empty()) {
    out << " (not performed)";
  } else {
    out << " to " << outgoing_particles_;
  }
}

}  // namespace smash
