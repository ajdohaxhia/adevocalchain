# Preset research and design notes — 1.1.0

The 24 presets added in 1.1.0 are **original parameter designs** informed by
publicly described production directions and common genre chains. They are not
official artist presets, not endorsed by any artist, and not exact recreations
of any record. Artist names in preset descriptions are descriptive references
only.

## Sources reviewed

| Source | What informed the designs |
|---|---|
| [Logic Studio Training — How to Build Pro Vocal Chains in Logic Pro by Genre](https://logicstudiotraining.com/logic-pro-vocal-chains/) | Base insert order (gain → EQ → compressor → de-esser → character EQ → saturation → limiter), Pop brightness/compression targets, and separate genre directions for pop, soul/R&B, hip-hop, rock and country. |
| [Antares — AutoTune for Rap & Hip-Hop](https://www.antarestech.com/blog/auto-tune-for-rap-hip-hop-how-trap-producers-use-pitch-correction) | Hard-tune practice: near-zero retune speed, deliberate scale lock, and the difference between correction and an audible effect. |
| [Waves StudioVerse — The Weeknd Vocal Chain](https://www.waves.com/studioverse/chain/28f671ca-fada-49d0-b1f6-746cf293c264) | Direction for airy, dark, spacious R&B/falsetto leads. |
| [Sound on Sound — Inside Track: The Weeknd](https://www.soundonsound.com/techniques/inside-track-weeknd) | Layered backing-vocal treatment and stereo group thinking. |
| [Equipboard — Burna Boy](https://equipboard.com/pros/burna-boy) | Afrobeats direction: multiple timed delay/backing-vocal auxiliaries rather than one static echo. |
| [Mix with the Masters — Billie Eilish, Inside the Track](https://mixwiththemasters.com/zh/videos/jon-castelli-aron-forbes-billie-eilish-birds-of-a-feather/part/2) | Intimacy, close-mic leveling and restrained space. |
| [bchillmix — Billie Eilish style presets](https://bchillmix.com/blogs/news/best-billie-eilish-style-vocal-presets) | Whisper-pop direction and close dynamic control. |
| [bchillmix — Travis Scott-style vocal depth](https://bchillmix.com/blogs/news/how-to-sound-like-travis-scott-without-overprocessing-your-vocals) | Hard-tuned melodic trap direction without overprocessing. |
| [bchillmix — UK drill vocal presets](https://bchillmix.com/blogs/news/best-uk-drill-vocal-preset-settings-for-dark-vocals) | Dark drill direction and tight high-pass/leveling. |
| [bchillmix — Gospel vocal presets](https://bchillmix.com/blogs/news/best-gospel-vocal-preset-settings-for-choir-layers) | Gospel lift, layered-vocal brightness and plate direction. |
| [bchillmix — Nashville country presets](https://bchillmix.com/blogs/news/best-country-vocal-preset-settings-for-nashville-sound) | Natural country vocal direction and restrained space. |
| [bchillmix — Tems-style Afrobeats vocals](https://bchillmix.com/blogs/news/how-to-get-tems-style-vocals-in-fl-studio-or-bandlab) | Warm Afrobeats depth and rhythmic throw direction. |
| [Gearspace — Drake vocal chain discussion](https://gearspace.com/threads/drake-vocal-chain-with-comparison.1310272/) | Community discussion of close, conversational rap presentation (unverified). |
| [Toontrack — Lead Vocals EZmix pack](https://www.toontrack.com/product/lead-vocals-ezmix-pack/) | Commercial reference for the range of lead-vocal starting points. |

These sources vary in authority. Studio interviews and manufacturer guides were
weighted above SEO preset round-ups. No source published a complete,
verifiable artist chain, and none was copied. The settings below were chosen to
fit this plugin's fixed topology and were then render-tested.

## Mapping of new presets

### Artist-Inspired Leads

| Preset | Direction | Design |
|---|---|---|
| After Hours Air | The Weeknd-style nocturnal falsetto | Slow-ish retune, dark plate, filtered dotted delay, airy high shelf. |
| Rodeo Warble | Travis Scott-style hard-tune trap | Retune 1 ms, dense compression, dark presence, short plate, feedback-safe echo. |
| Low Light Warble | Future-style melodic trap | Hard tune, very dense leveling, dark top, tape saturation, short room. |
| Toronto Close | Drake-style conversational rap | No tuning, low-mid warmth, restrained top, 95 ms stereo slap, short room. |
| Whisper Cinematic | Billie Eilish-style whisper | Very gentle correction, heavy smooth leveling, restrained dark plate, no rhythmic echo. |
| Cinematic Diva | Modern pop-diva ballad | Gentle correction, wide dynamics, long plate, quiet quarter echo, open top. |

### Modern Rap Chains

| Preset | Direction |
|---|---|
| Auto-Tune Melody | Fast melodic-trap tuning with a bright, saturated lead and ducked eighth echo. |
| UK Drill Dark | Tight high-pass, fast leveling, dark top and short dark room. |
| Emo Rap Wide | Saturated, pitch-locked lead with ping-pong echo and long plate. |
| Rage Unison | Bright, saturated rage/hyperpop lead with fast tuning and sixteenth echo. |
| Latin Trap Heat | Warm, forward Latin-trap lead with dotted echo and short bright plate. |
| Afrobeats Bounce | Warm Afrobeats lead with a bouncy eighth echo and open top. |

### Pop, Soul & Rock

| Preset | Direction |
|---|---|
| Maximal Pop Stack | Glossy, dense pop stack with bright shelves and a compact plate. |
| Neo Soul Warmth | Round low-mids, slow leveling, tape warmth, almost no ambience. |
| Nashville Story | Natural country lead, small room, 120 ms slap, gentle leveling. |
| Arena Rock Edge | Fast compression, midrange bite, saturated edge and short hall. |
| Gospel Lift | Bright, controlled gospel lead with wide plate. |
| Bedroom Pop Dry | Dry, mid-forward bedroom-pop lead with soft saturation and tiny room. |

### Broadcast & Live

| Preset | Direction |
|---|---|
| Podcast Broadcast | Cleanup, presence, firm leveling and −1 dBFS ceiling for speech. |
| Voiceover Master | Conservative cleanup, smooth leveling, −1 dBFS ceiling, no effects. |
| Live Stage | Feedback-safe high-pass, firm leveling and short room for stage vocals. |
| Worship Ambient | Long plate and dotted-quarter echo for modern worship. |
| Interview Clean | Strong noise reduction and leveling, no musical effects. |
| Phone Memo Rescue | Band-limited, saturated rescue for phone-memo cameos. |

## Validation discipline

- Every new preset is a complete 82-parameter state.
- All 60 presets are rendered by the headless suite and checked for finite output.
- Preset IDs are stable (`factory.<bank>.<name>`).
- Duplicate full states are rejected by the generator.
- The new bank JSON is embedded in the plugin and mirrored at
  `presets/ademusic-vocal-rack-factory.json`.
- No artist name is used as a preset name.

## Artist/trademark disclaimer

Artist and product names are used only to describe a production direction. This
project is not affiliated with, endorsed by, or sponsored by any artist, label,
or audio-plugin manufacturer. All parameter values are original.
