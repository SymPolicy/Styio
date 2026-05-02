#pragma once
#ifndef STYIO_GENERAL_IR_H_
#define STYIO_GENERAL_IR_H_

/*
  Compatibility aggregator for Styio GenIR domains:

  - SG  = Styio General. Default/general IR nodes.
  - SIO = Styio Input/Output. Files, standard streams, future network,
          and filesystem IO nodes.
  - SC  = Styio Collection. List, dictionary, and future data structures.

  New IR nodes should be added to their domain header first.
*/

#include "SGIR.hpp"
#include "SCIR.hpp"
#include "SIOIR.hpp"

#endif
