/*  ------------------------------------------------------------------
    Copyright (c) 2019 Marc Toussaint
    email: marc.toussaint@informatik.uni-stuttgart.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#pragma once

#include "../Core/array.tpp"

struct RRT {
 private:
  unique_ptr<struct sRRT> self;

 public:
  RRT(const arr& q0, double _stepsize);
  double getProposalTowards(arr& proposal, const arr& q);
  void add(const arr& q);

  //some access routines
  double getStepsize();
  uint getNearest();
  uint getParent(uint i);
  uint getNumberNodes();
  arr getNode(uint i);
  void getRandomNode(arr& q);
  arr getRandomNode();
};
