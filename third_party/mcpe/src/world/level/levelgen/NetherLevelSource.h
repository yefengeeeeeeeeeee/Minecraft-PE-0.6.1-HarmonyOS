#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__NetherLevelSource_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__NetherLevelSource_H__

#include <map>
#include "../chunk/ChunkSource.h"
#include "../biome/Biome.h"
#include "../../../util/Random.h"
#include "synth/PerlinNoise.h"

class Level;
class LevelChunk;

// First-pass Nether generator. Produces a 16x16x128 chunk with:
//   y=0:        bedrock floor
//   y=1..31:    netherrack with carved caves; lava lake fills y<=31 elsewhere
//   y=32..120:  netherrack with carved tunnels
//   y=121..125: thinning netherrack (occasional ceiling protrusions)
//   y=126..127: bedrock ceiling
//   glowstone clusters embedded along the ceiling
//
// "Caves" are produced by a small per-block hash-driven noise function — not
// as smooth as Perlin but cheap, deterministic, and good enough for a v1.
class NetherLevelSource : public ChunkSource
{
public:
    NetherLevelSource(Level* level, long seed);
    ~NetherLevelSource();

    bool hasChunk(int x, int z);
    LevelChunk* getChunk(int x, int z);
    LevelChunk* create(int x, int z);
    void postProcess(ChunkSource* parent, int x, int z);
    bool tick();
    bool shouldSave();
    Biome::MobList getMobsAt(const MobCategory& mobCategory, int x, int y, int z);
    std::string gatherStats();

private:
    Level* level;
    long seed;
    std::map<int, LevelChunk*> chunkMap;

    // Per-instance Perlin noise generators, seeded from the world seed. Used
    // to produce smooth coherent terrain instead of blocky hash noise.
    Random rng;
    PerlinNoise terrainNoise;
    PerlinNoise detailNoise;
};

#endif
