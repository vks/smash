/*
 *
 *    Copyright (c) 2014
 *      Bjørn Bäuchle <baeuchle@fias.uni-frankfurt.de>
 *
 *    GNU General Public License (GPLv3)
 *
 */


#ifndef SRC_INCLUDE_ANGLES_H_
#define SRC_INCLUDE_ANGLES_H_

#include <cmath>


class Angles {
 public:
  Angles();
  // get new angles:
  void distribute_isotropically();
  // update angles:
  void set_phi(const double& phi);
  // set cos(theta) (preferred):
  void set_costheta(const double& cos);
  // set theta (if you only have the angle, not the cosine)
  // In the interface (public functions) we don't specify if theta or
  // costheta is actually stored inside the object, so we don't name the
  // functions to indicate the internals.
  void set_theta(const double& theta);
  // get elements:
  double phi() const;
  double costheta() const;
  // sqrt(1-costheta**2)
  double sintheta() const;
  // x, y and z together give a normalized three vector which, thus,
  // has to be multiplied by its length.
  double x() const;
  double y() const;
  double z() const;
  // in case we really need the polar angle instead of just the cosine,
  // we can return acos(costheta).
  double theta() const;

 private:
  double phi_;
  double costheta_;
};

inline Angles::Angles() : phi_(0), costheta_(0) {}

void inline Angles::distribute_isotropically() {
  // isotropic distribution: phi in [0, 2pi) and cos(theta) in [-1,1]
  phi_ = 2.0 * M_PI * drand48();
  costheta_ = -1.0 + 2.0 * drand48();
}

void inline Angles::set_phi(const double& newphi) {
  phi_ = newphi;
  // check if phi is in 0 .. 2pi. If not, we simply transform it
  // there by subtracting/adding 2pi as often as needed.
  // floor(phi/(2pi) is the number of (2pi)s that we need to subtract
  // (why use a loop if one statement can do it?)
  if (phi_ < 0 || phi_ >= 2.0 * M_PI) {
    phi_ -= 2.0 * M_PI * floor(phi_ / (2.0 * M_PI));
  }
}
void inline Angles::set_costheta(const double& newcos) {
  costheta_ = newcos;
  // check if costheta_ is in -1..1. If not, well. Error handling here
  // is a lot harder than in the above. Still, I will silently do the
  // same as above. Note, though, that costheta = 1 is allowed, even if
  // it cannot be generated by distribute_isotropically().
  if (costheta_ < -1 || costheta_ > 1) {
    char errormsg[50];
    snprintf(errormsg, sizeof(errormsg),
             "Wrong value for costheta (must be in [-1,1]): %g",
             costheta_);
    throw(errormsg);
  }
}
void inline Angles::set_theta(const double& newtheta) {
  // no error handling necessary, because this gives a sensible answer
  // for every real number.
  set_costheta(cos(newtheta));
}

double inline Angles::costheta() const { return costheta_; }
double inline Angles::phi() const { return phi_; }
double inline Angles::sintheta() const {
  return sqrt(1.0 - costheta_*costheta_);
}
double inline Angles::x() const { return sintheta()*cos(phi_); }
double inline Angles::y() const { return sintheta()*sin(phi_); }
double inline Angles::z() const { return costheta_; }
double inline Angles::theta() const { return acos(costheta_); }

#endif  // SRC_INCLUDE_ANGLES_H_
