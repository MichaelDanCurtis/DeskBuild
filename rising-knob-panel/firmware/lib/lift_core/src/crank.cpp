#include "crank.h"

#include <cmath>

namespace crank {

float heightMm(float angleDeg) {
    if (angleDeg <= 0.0f) return 0.0f;
    if (angleDeg >= 180.0f) return kTravelMm;
    const float phi = angleDeg * static_cast<float>(M_PI) / 180.0f;
    const float s = kCrankRadiusMm * std::sin(phi);
    // Link pin height minus its height at bottom dead centre.
    return -kCrankRadiusMm * std::cos(phi) + std::sqrt(kLinkLengthMm * kLinkLengthMm - s * s) +
           kCrankRadiusMm - kLinkLengthMm;
}

}  // namespace crank
