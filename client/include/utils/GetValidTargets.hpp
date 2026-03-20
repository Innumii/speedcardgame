#ifndef GET_VALID_TARGETS_HPP
#define GET_VALID_TARGETS_HPP

#include <vector>
#include "objects/Card.h"
#include "states/Playing.hpp"

// Utility function to determine valid targets for a given card and source lane.

std::vector<int> getValidTargets(const Playing& playing, const Card& card, int sourceLane);

#endif