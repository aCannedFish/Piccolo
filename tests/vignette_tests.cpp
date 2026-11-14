#include "test_checks.h"

#include "_generated/serializer/all_serializer.h"
#include "runtime/resource/res_type/data/vignette_settings.h"
#include "runtime/resource/res_type/global/global_rendering.h"

#include <iostream>

using namespace Piccolo;

namespace
{
    // Old rendering configurations must keep their original appearance.
    void testLegacySettings()
    {
        GlobalRenderingRes resource;
        Serializer::read(Json::object {}, resource);
        require(!resource.m_vignette.m_enabled, "Missing vignette configuration disables the effect");
    }

    // Settings can be edited freely, then sanitized before recording a draw.
    void testParameterBounds()
    {
        VignetteSettings settings;
        settings.m_strength     = 2.f;
        settings.m_inner_radius = -1.f;
        settings.m_outer_radius = -2.f;
        settings.sanitize();
        requireNear(settings.m_strength, 1.f, "Strength is bounded above");
        requireNear(settings.m_inner_radius, 0.f, "Inner radius is nonnegative");
        require(settings.m_outer_radius > settings.m_inner_radius, "Smoothstep edges remain distinct");

        settings.m_strength     = -1.f;
        settings.m_inner_radius = 2.f;
        settings.m_outer_radius = 1.f;
        settings.sanitize();
        requireNear(settings.m_strength, 0.f, "Strength is bounded below");
        require(settings.m_outer_radius > 2.f, "Reversed radii produce a valid transition");
    }

    void testSettingsRoundTrip()
    {
        VignetteSettings settings;
        settings.m_enabled      = true;
        settings.m_strength     = 0.6f;
        settings.m_inner_radius = 0.25f;
        settings.m_outer_radius = 1.25f;
        const Json       saved  = Serializer::write(settings);
        VignetteSettings restored;
        Serializer::read(saved, restored);
        require(restored.m_enabled, "Serialization preserves enabled state");
        requireNear(restored.m_strength, 0.6f, "Serialization preserves strength");
        requireNear(restored.m_inner_radius, 0.25f, "Serialization preserves inner radius");
        requireNear(restored.m_outer_radius, 1.25f, "Serialization preserves outer radius");
    }
} // namespace

int main()
{
    try
    {
        testLegacySettings();
        testParameterBounds();
        testSettingsRoundTrip();
        std::cout << "Passed 3 vignette configuration cases.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
