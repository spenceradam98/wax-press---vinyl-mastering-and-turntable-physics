// The parameter table: the single source of truth for the engine, the plugin's
// AudioProcessorValueTreeState, the presets and the editor.
#pragma once

namespace wp
{
enum class Kind { Float, Choice, Bool };

struct ParamDef
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    float centre;          // value shown at 12 o'clock for skewed ranges (0 = linear)
    const char* unit;      // also picks the value formatter (see the plugin's formatValue)
    const char* choices;   // '|' separated, for Choice
};

// X(enumName, id, display name, kind, min, max, default, centre, unit, choices)
#define WP_PARAMS(X) \
    X (MonoBass,   "mono_bass",   "Mono Bass",          Float,  40, 300, 120, 120, "monoHz", "") \
    X (HfLimit,    "hf_limit",    "HF Limiter",         Float,   0, 1, 0.3f, 0, "", "") \
    X (CutLevel,   "cut_level",   "Cut Level",          Float, -12, 12, 0, 0, "dB", "") \
    X (Needle,     "needle",      "Needle Position",    Float,   0, 1, 0.15f, 0, "pos", "") \
    X (AutoTravel, "auto_travel", "Auto Travel",        Bool,    0, 1, 0, 0, "", "") \
    X (SideLength, "side_len",    "Side Length",        Float,  12, 24, 18, 0, "min", "") \
    X (Speed,      "speed",       "Groove Speed",       Choice,  0, 2, 0, 0, "", "33|45|78") \
    X (Stylus,     "stylus",      "Stylus",             Choice,  0, 2, 0, 0, "", "Conical|Elliptical|Worn") \
    X (Tracking,   "tracking",    "Tracking Force",     Float, 0.5f, 4, 2, 0, "g", "") \
    X (CartReso,   "cart_reso",   "Cartridge Resonance",Float,   0, 1, 0.3f, 0, "", "") \
    X (Crosstalk,  "crosstalk",   "Crosstalk",          Float,   0, 1, 0.3f, 0, "", "") \
    X (Riaa,       "riaa",        "RIAA Accuracy",      Float,   0, 1, 0.15f, 0, "riaa", "") \
    X (Crackle,    "crackle",     "Crackle",            Float,   0, 1, 0.25f, 0, "", "") \
    X (Pops,       "pops",        "Pops",               Float,   0, 1, 0.15f, 0, "", "") \
    X (Hiss,       "hiss",        "Hiss",               Float,   0, 1, 0.2f, 0, "", "") \
    X (Rumble,     "rumble",      "Rumble",             Float,   0, 1, 0.15f, 0, "", "") \
    X (Dust,       "dust",        "Dust Density",       Float,   0, 1, 0.35f, 0, "", "") \
    X (Warp,       "warp",        "Warp",               Float,   0, 1, 0.1f, 0, "", "") \
    X (Mode,       "mode",        "Mode",               Choice,  0, 1, 1, 0, "", "Mastering Check|Character") \
    X (Mix,        "mix",         "Mix",                Float,   0, 1, 1, 0, "", "") \
    X (Output,     "output",      "Output",             Float, -24, 12, 0, 0, "dB", "")

enum P : int
{
#define WP_ENUM(e, ...) e,
    WP_PARAMS (WP_ENUM)
#undef WP_ENUM
    NumParams
};

inline const ParamDef* paramDefs()
{
#define WP_DEF(e, id, name, kind, mn, mx, df, ct, unit, ch) ParamDef { id, name, Kind::kind, (float) (mn), (float) (mx), (float) (df), (float) (ct), unit, ch },
    static const ParamDef defs[NumParams] = { WP_PARAMS (WP_DEF) };
#undef WP_DEF
    return defs;
}

// Index for an id string, or -1.
int paramIndex (const char* id);

// Mono Bass at or below this frequency means Off.
constexpr float kMonoBassOff = 50.0f;

// Record geometry (12-inch LP): outer and inner groove radius in mm.
constexpr double kOuterRadiusMm = 146.0, kInnerRadiusMm = 60.0;
inline double rpmFor (int speedIndex) { return speedIndex == 2 ? 78.26 : (speedIndex == 1 ? 45.0 : 33.333); }
} // namespace wp
