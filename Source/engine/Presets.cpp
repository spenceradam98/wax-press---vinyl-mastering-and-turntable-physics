// Factory presets: overrides on top of the parameter defaults.
#include "Engine.h"

namespace wp
{
namespace
{
// Everything that colours the sound switched off: the starting point for the mastering presets.
const std::vector<std::pair<std::string, float>> kClean = {
    { "mode", 0 }, { "stylus", 1 }, { "riaa", 0 }, { "crosstalk", 0 }, { "cart_reso", 0 },
    { "crackle", 0 }, { "pops", 0 }, { "hiss", 0 }, { "rumble", 0 }, { "dust", 0 }, { "warp", 0 }, { "needle", 0 }
};

Preset clean (std::string name, std::string bank, std::vector<std::pair<std::string, float>> extra)
{
    auto v = kClean;
    v.insert (v.end(), extra.begin(), extra.end());   // later entries win
    return { std::move (name), std::move (bank), std::move (v) };
}
} // namespace

const std::vector<std::string>& presetBanks()
{
    static const std::vector<std::string> b = { "Mastering", "Pressings", "Lo-Fi & Beats", "Speeds & Oddities" };
    return b;
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p = {
        // ---- Mastering: honest physics, no surface noise
        clean ("Mastering Check", "Mastering", { { "mono_bass", 150 }, { "hf_limit", 0.3f } }),
        clean ("Inner Groove Check", "Mastering", { { "mono_bass", 150 }, { "hf_limit", 0.3f }, { "needle", 1 }, { "stylus", 0 } }),
        clean ("Whole Side Check", "Mastering", { { "mono_bass", 150 }, { "hf_limit", 0.3f }, { "auto_travel", 1 }, { "side_len", 20 }, { "stylus", 0 } }),
        clean ("Hot Cut", "Mastering", { { "mono_bass", 180 }, { "hf_limit", 0.6f }, { "cut_level", 8 }, { "stylus", 0 }, { "needle", 0.5f } }),
        clean ("Club Cut Check (45)", "Mastering", { { "speed", 1 }, { "mono_bass", 120 }, { "hf_limit", 0.5f }, { "cut_level", 3 } }),

        // ---- Pressings
        { "Fresh Pressing", "Pressings", { { "stylus", 1 }, { "crackle", 0.08f }, { "pops", 0.03f }, { "hiss", 0.1f }, { "rumble", 0.05f },
                                            { "dust", 0.1f }, { "warp", 0.03f }, { "riaa", 0.05f }, { "cart_reso", 0.2f }, { "crosstalk", 0.2f } } },
        { "Audiophile 180g", "Pressings", { { "stylus", 1 }, { "crackle", 0.03f }, { "pops", 0 }, { "hiss", 0.06f }, { "rumble", 0.03f },
                                             { "dust", 0.05f }, { "warp", 0 }, { "riaa", 0.02f }, { "cart_reso", 0.15f }, { "crosstalk", 0.15f }, { "needle", 0.05f } } },
        { "Thrift Store LP", "Pressings", { { "stylus", 2 }, { "tracking", 1.2f }, { "crackle", 0.55f }, { "pops", 0.45f }, { "hiss", 0.35f },
                                             { "rumble", 0.3f }, { "dust", 0.6f }, { "warp", 0.3f }, { "riaa", 0.5f }, { "cart_reso", 0.5f }, { "crosstalk", 0.5f } } },
        { "Inner Groove Ballad", "Pressings", { { "needle", 0.9f }, { "stylus", 0 }, { "crackle", 0.2f }, { "pops", 0.1f }, { "hiss", 0.2f },
                                                 { "dust", 0.3f }, { "warp", 0.08f } } },
        { "Club 12-inch (45 rpm)", "Pressings", { { "speed", 1 }, { "cut_level", 4 }, { "mono_bass", 140 }, { "hf_limit", 0.45f }, { "needle", 0.2f },
                                                   { "stylus", 1 }, { "crackle", 0.1f }, { "pops", 0.05f }, { "hiss", 0.08f }, { "dust", 0.15f } } },
        { "Side B, Last Track", "Pressings", { { "needle", 0.96f }, { "stylus", 2 }, { "tracking", 1.4f }, { "crackle", 0.3f }, { "pops", 0.15f },
                                                { "hiss", 0.25f }, { "dust", 0.4f } } },
        { "Whole Side (Auto travel)", "Pressings", { { "auto_travel", 1 }, { "side_len", 20 }, { "needle", 0 }, { "stylus", 0 }, { "crackle", 0.2f },
                                                      { "pops", 0.1f }, { "hiss", 0.15f } } },

        // ---- Lo-Fi & Beats
        { "Dusty Breaks", "Lo-Fi & Beats", { { "stylus", 2 }, { "crackle", 0.6f }, { "dust", 0.7f }, { "pops", 0.3f }, { "hiss", 0.3f },
                                              { "rumble", 0.2f }, { "riaa", 0.6f }, { "warp", 0.15f }, { "crosstalk", 0.45f }, { "cart_reso", 0.45f } } },
        { "Lo-Fi Beats", "Lo-Fi & Beats", { { "stylus", 0 }, { "crackle", 0.45f }, { "dust", 0.55f }, { "pops", 0.15f }, { "hiss", 0.4f },
                                             { "warp", 0.35f }, { "riaa", 0.7f }, { "cart_reso", 0.6f }, { "needle", 0.6f } } },
        { "Warped Summer", "Lo-Fi & Beats", { { "warp", 0.85f }, { "crackle", 0.3f }, { "pops", 0.2f }, { "hiss", 0.25f }, { "riaa", 0.4f },
                                               { "dust", 0.35f }, { "cart_reso", 0.4f } } },
        { "Basement Crate", "Lo-Fi & Beats", { { "stylus", 2 }, { "tracking", 0.9f }, { "crackle", 0.75f }, { "dust", 0.85f }, { "pops", 0.55f },
                                                { "hiss", 0.5f }, { "rumble", 0.5f }, { "warp", 0.4f }, { "riaa", 0.8f }, { "crosstalk", 0.6f } } },
        { "Sample Flip Glue", "Lo-Fi & Beats", { { "crackle", 0.3f }, { "dust", 0.4f }, { "hiss", 0.2f }, { "pops", 0.08f }, { "riaa", 0.3f },
                                                 { "mix", 0.7f } } },

        // ---- Speeds & Oddities
        { "78 Shellac", "Speeds & Oddities", { { "speed", 2 }, { "stylus", 2 }, { "mono_bass", 300 }, { "crackle", 0.8f }, { "dust", 0.9f },
                                                { "pops", 0.4f }, { "hiss", 0.9f }, { "rumble", 0.4f }, { "riaa", 1 }, { "cart_reso", 0.8f },
                                                { "crosstalk", 1 }, { "warp", 0.2f } } },
        { "Jukebox 45", "Speeds & Oddities", { { "speed", 1 }, { "stylus", 0 }, { "tracking", 3.5f }, { "crackle", 0.35f }, { "pops", 0.2f },
                                                { "hiss", 0.3f }, { "cart_reso", 0.55f }, { "riaa", 0.5f }, { "dust", 0.4f } } },
        { "Light Tracking", "Speeds & Oddities", { { "tracking", 0.6f }, { "needle", 0.7f }, { "stylus", 0 }, { "crackle", 0.2f }, { "hiss", 0.15f } } },
        { "Broken Preamp", "Speeds & Oddities", { { "riaa", 1 }, { "cart_reso", 1 }, { "crosstalk", 0.8f }, { "hiss", 0.3f }, { "rumble", 0.35f } } },
        clean ("Just the Crackle", "Speeds & Oddities", { { "mono_bass", 40 }, { "crackle", 0.5f }, { "dust", 0.5f }, { "pops", 0.2f }, { "hiss", 0.15f } }),
    };
    return p;
}

std::array<float, NumParams> presetValues (const Preset& preset)
{
    std::array<float, NumParams> v {};
    const auto* d = paramDefs();
    for (int i = 0; i < NumParams; ++i) v[(size_t) i] = d[i].def;
    for (const auto& kv : preset.values)
    {
        const int idx = paramIndex (kv.first.c_str());
        if (idx >= 0) v[(size_t) idx] = std::clamp (kv.second, d[idx].min, d[idx].max);
    }
    return v;
}
} // namespace wp
