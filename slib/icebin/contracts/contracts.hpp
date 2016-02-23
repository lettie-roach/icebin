#pragma once

#include <string>

namespace icebin {

/** Indicates the grid this field is supposed to be on. */
const unsigned GRID_BITS = 3;

const unsigned ATMOSPHERE = 1;
const unsigned ICE = 2;
const unsigned ELEVATION = 3;

/** This field is returned at initialization time, before the first coupling. */
const unsigned INITIAL = 4;

extern std::string to_str(unsigned int flags);

// =====================================================
// Virtual Functions that dispatch on GCMCoupler and IceModel

extern void setup_contracts(GCMCoupler &coupler, IceModel &model);
// ======================================================



}



