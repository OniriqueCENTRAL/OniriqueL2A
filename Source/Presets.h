#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "ui/OniriquePresets.h"

//==============================================================================
//  Les presets d'usine, rangés par usage (VOCALS, DRUMS, BASS & INSTRUMENTS, MIX BUS). Un réglage non cité reprend sa
//  valeur d'usine. Valeurs en unités du réglage (dB, %), ATTACK et RELEASE de 1 (lent) à 7 (rapide), les choix par leur
//  rang (RATIO 0 4:1 … 3 20:1, 4 ALL | version 0 BLACK, 1 BLUE | MODE 0 COMPRESS, 1 LIMIT).
//==============================================================================
namespace solopresets
{
enum Tags { vocals = 1, drums = 2, instruments = 4, mixbus = 8 };

inline juce::StringArray tagNames() { return { "Vocals", "Drums", "Bass & Instruments", "Mix bus" }; }

inline std::vector<onirique::PresetBank::Stock> stockPresets()
{
    using namespace params;
#if SOLO_LA2A
    return {
        { "INIT", 0, {} },
        { "Vocal Leveler", vocals, { { peak, 45 }, { gain, 4 } } },
        { "Gentle Vocal", vocals, { { peak, 30 }, { gain, 2 } } },
        { "Thick Vocal", vocals, { { peak, 60 }, { gain, 7 } } },
        { "Bass Smoother", instruments, { { peak, 50 }, { gain, 5 } } },
        { "Acoustic Guitar", instruments, { { peak, 35 }, { gain, 3 } } },
        { "Limit Mode Vocal", vocals, { { peak, 55 }, { gain, 6 }, { limit, 1 } } },
        { "Parallel Opto", drums | mixbus, { { peak, 70 }, { gain, 10 }, { mix, 45 } } },
        { "Bus Glue", mixbus, { { peak, 20 }, { gain, 1 } } },
    };
#else
    return {
        { "INIT", 0, {} },
        { "Vocal Control", vocals, { { input, 8 }, { attack, 4 }, { release, 5 } } },
        { "Rap Upfront", vocals, { { input, 12 }, { ratio, 1 }, { attack, 6 }, { release, 6 } } },
        { "Blue Grit Vocal", vocals, { { revision, 1 }, { input, 14 }, { ratio, 1 }, { attack, 5 }, { release, 5 } } },
        { "All Buttons Smash", drums, { { input, 18 }, { ratio, 4 }, { attack, 6 }, { release, 6 }, { output, -6 } } },
        { "Parallel Drums", drums, { { input, 20 }, { ratio, 4 }, { attack, 7 }, { release, 7 }, { mix, 40 } } },
        { "Snappy Snare", drums, { { input, 10 }, { ratio, 0 }, { attack, 2 }, { release, 6 } } },
        { "Bass Tight", instruments, { { input, 10 }, { ratio, 1 }, { attack, 4 }, { release, 4 } } },
        { "Bus Glue", mixbus, { { input, 4 }, { ratio, 0 }, { attack, 2 }, { release, 3 }, { mix, 70 } } },
    };
#endif
}
} // namespace solopresets
