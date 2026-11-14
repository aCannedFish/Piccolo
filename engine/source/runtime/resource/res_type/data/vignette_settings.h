#pragma once

#include "runtime/core/meta/reflection/reflection.h"

#include <algorithm>

namespace Piccolo
{
    // Controls radial darkening in viewport space. A radius of one is half the viewport height.
    REFLECTION_TYPE(VignetteSettings)
    CLASS(VignetteSettings, Fields)
    {
        REFLECTION_BODY(VignetteSettings);

    public:
        bool  m_enabled {false};
        float m_strength {0.35f};
        float m_inner_radius {0.5f};
        float m_outer_radius {1.5f};

        // Keep smoothstep's edges ordered and the attenuation in the range [0, 1].
        void sanitize()
        {
            m_strength     = std::clamp(m_strength, 0.f, 1.f);
            m_inner_radius = std::max(m_inner_radius, 0.f);
            m_outer_radius = std::max(m_outer_radius, m_inner_radius + 0.001f);
        }
    };
} // namespace Piccolo
