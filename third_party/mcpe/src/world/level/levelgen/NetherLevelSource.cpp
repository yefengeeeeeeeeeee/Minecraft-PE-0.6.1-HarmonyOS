#include "NetherLevelSource.h"
#include "../Level.h"
#include "../ChunkPos.h"
#include "../tile/Tile.h"
#include "../chunk/LevelChunk.h"
#include "../LevelConstants.h"
#include "../../entity/MobCategory.h"
#include "../../entity/EntityTypes.h"
#include "../biome/Biome.h"
#include <math.h>

namespace {
// Cheap deterministic 32-bit hash mixing world coordinates and seed.
unsigned int hashAt(int x, int y, int z, long seed) {
    unsigned int h = (unsigned int)seed;
    h = h * 0x9E3779B1u + (unsigned int)x;
    h = h * 0x9E3779B1u + (unsigned int)y;
    h = h * 0x9E3779B1u + (unsigned int)z;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

// Smoother "density" sample by quantising coords — 4x2x4 voxel cells share the
// same value, which gives chunky-but-coherent landmasses instead of pepper.
// Returns 0..255.
unsigned int densityAt(int wx, int y, int wz, long seed) {
    return hashAt(wx >> 2, y >> 1, wz >> 2, seed) & 0xFF;
}

inline int blockIndex(int x, int y, int z) {
    return (x << 11) | (z << 7) | y;
}
}

NetherLevelSource::NetherLevelSource(Level* level_, long seed_)
:   level(level_),
    seed(seed_),
    rng(seed_),
    terrainNoise(&rng, 4),   // 4-octave base terrain
    detailNoise(&rng, 2)     // 2-octave fine detail to roughen edges
{
}

NetherLevelSource::~NetherLevelSource()
{
}

bool NetherLevelSource::hasChunk(int x, int z) {
    // Only report chunks we've already generated. Returning true uncondition-
    // ally (as RandomLevelSource does) is only safe when this source is
    // wrapped by a ChunkCache that filters; we are not, since the dimension
    // swap bypasses the cache to avoid stale overworld chunks. Without this
    // filter, Level::getHeightmap -> getChunk neighbour lookups during
    // recalcHeightmap recurse outwards forever.
    return chunkMap.find(ChunkPos::hashCode(x, z)) != chunkMap.end();
}
LevelChunk* NetherLevelSource::create(int x, int z) { return getChunk(x, z); }

LevelChunk* NetherLevelSource::getChunk(int chunkX, int chunkZ) {
    int hashedPos = ChunkPos::hashCode(chunkX, chunkZ);
    std::map<int, LevelChunk*>::iterator it = chunkMap.find(hashedPos);
    if (it != chunkMap.end()) return it->second;
    LOGI("[NetherLS] GEN chunk (%d, %d) tiles: nr=%p bd=%p lv=%p lg=%p\n",
        chunkX, chunkZ,
        Tile::netherrack, Tile::unbreakable, Tile::lava, Tile::lightGem);

    const int LAVA_TOP = 30;       // lava ocean tops out here
    const int FLOOR_TOP = 33;      // landing band tops out here
    const int CAVERN_TOP = 68;     // main open cavern top
    const int DENSE_TOP = 118;     // dense netherrack top
    const int CEILING = 127;

    unsigned char* blocks = new unsigned char[LevelChunk::ChunkBlockCount];
    const unsigned char NETHERRACK = (unsigned char)Tile::netherrack->id;
    const unsigned char BEDROCK    = (unsigned char)Tile::unbreakable->id;
    const unsigned char LAVA       = (unsigned char)Tile::lava->id;
    const unsigned char AIR        = 0;
    const unsigned char GLOWSTONE  = (unsigned char)Tile::lightGem->id;

    for (int x = 0; x < 16; ++x)
    for (int z = 0; z < 16; ++z) {
        int wx = chunkX * 16 + x;
        int wz = chunkZ * 16 + z;

        for (int y = 0; y <= CEILING; ++y) {
            int idx = blockIndex(x, y, z);
            unsigned char block = NETHERRACK;

            // Bedrock floor (always solid at y=0).
            if (y == 0) {
                block = BEDROCK;
            // Bedrock ceiling (always solid at y=127).
            } else if (y == CEILING) {
                block = BEDROCK;
            // Mixed bedrock layer just under the ceiling.
            } else if (y == CEILING - 1) {
                block = ((hashAt(wx, y, wz, seed) % 10) < 5) ? BEDROCK : NETHERRACK;
            // Bedrock irregularity above the floor.
            } else if (y == 1) {
                block = ((hashAt(wx, y, wz, seed) % 10) < 6) ? BEDROCK : NETHERRACK;
            } else {
                // Smooth 3D Perlin density. Scaling determines feature size:
                // smaller = larger features. 0.05 horizontally, 0.10 vertically
                // gives ~12-block wide cavern openings and slightly stretched
                // vertical formations.
                float base = terrainNoise.getValue(wx * 0.05f, y * 0.10f, wz * 0.05f);
                float detail = detailNoise.getValue(wx * 0.20f, y * 0.20f, wz * 0.20f);
                float density = base + detail * 0.20f;

                // Per-y threshold: above this -> netherrack, below -> air/lava.
                // Bias more solid near the top and bottom, more open in the
                // middle cavern band.
                float threshold;
                if (y <= LAVA_TOP) {
                    threshold = 0.85f;        // very open below the lava line
                } else if (y <= FLOOR_TOP) {
                    threshold = -0.15f;       // mostly solid floor
                } else if (y <= CAVERN_TOP) {
                    threshold = 0.55f;        // open cavern with stalactites
                } else if (y <= DENSE_TOP) {
                    // Linear ramp from open at y=CAVERN_TOP to dense further up
                    float t = (float)(y - CAVERN_TOP) / (float)(DENSE_TOP - CAVERN_TOP);
                    threshold = 0.55f - 0.55f * t;  // 0.55 -> 0.00
                } else {
                    threshold = -0.25f;       // nearly solid under the ceiling
                }

                bool solid = density > threshold;
                if (solid) {
                    block = NETHERRACK;
                } else if (y <= LAVA_TOP) {
                    block = LAVA;
                } else {
                    block = AIR;
                }
            }

            blocks[idx] = block;
        }

    }

    // Glowstone clusters — large hanging blobs that drip down from the
    // ceiling. Anchors live on a coarse 24-block grid; each cell rolls a die
    // to decide whether it gets a cluster, where in the cell, and how big.
    // We look at the 3x3 neighbourhood of cells so clusters straddle chunk
    // boundaries seamlessly.
    const int CELL = 24;
    const int chunkBaseCellX = (chunkX * 16) / CELL - 1;
    const int chunkBaseCellZ = (chunkZ * 16) / CELL - 1;

    for (int cdx = 0; cdx <= 2; ++cdx)
    for (int cdz = 0; cdz <= 2; ++cdz) {
        int cellX = chunkBaseCellX + cdx;
        int cellZ = chunkBaseCellZ + cdz;

        // 1-in-3 cells have an anchor.
        unsigned int h = hashAt(cellX, 0, cellZ, seed ^ 0xC0FFEEu);
        if ((h % 3) != 0) continue;

        // Anchor world position within the cell.
        int ax = cellX * CELL + (int)((h >> 4)  & 0x1F) % CELL;
        int az = cellZ * CELL + (int)((h >> 9)  & 0x1F) % CELL;
        // Anchor Y just below the ceiling.
        int ay = CEILING - 2 - (int)((h >> 14) & 0x3);

        // Cluster radius 4-7 blocks.
        float radius = 4.0f + (float)((h >> 17) & 0x3);

        // Compute bounding box of this cluster in chunk-local coords.
        int minX = ax - (int)radius - chunkX * 16;
        int maxX = ax + (int)radius - chunkX * 16;
        int minZ = az - (int)radius - chunkZ * 16;
        int maxZ = az + (int)radius - chunkZ * 16;
        if (maxX < 0 || minX >= 16) continue;
        if (maxZ < 0 || minZ >= 16) continue;

        if (minX < 0)  minX = 0;
        if (maxX > 15) maxX = 15;
        if (minZ < 0)  minZ = 0;
        if (maxZ > 15) maxZ = 15;

        // Anisotropic distance — count Y less so clusters drip down further.
        const float Y_STRETCH = 0.55f;

        // Cluster extends down ~radius*2 blocks max
        int yLo = ay - (int)(radius / Y_STRETCH);
        int yHi = ay + 1;
        if (yLo < 1) yLo = 1;
        if (yHi > CEILING - 1) yHi = CEILING - 1;

        for (int x = minX; x <= maxX; ++x)
        for (int z = minZ; z <= maxZ; ++z)
        for (int y = yLo; y <= yHi; ++y) {
            int wx = chunkX * 16 + x;
            int wz = chunkZ * 16 + z;
            float fx = (float)(wx - ax);
            float fz = (float)(wz - az);
            float fy = (float)(y  - ay) * Y_STRETCH;
            float d2 = fx * fx + fy * fy + fz * fz;
            float r2 = radius * radius;
            if (d2 > r2) continue;
            // Soft falloff: add a per-block jitter so the cluster surface is
            // a bit lumpy instead of a perfect sphere.
            unsigned int j = hashAt(wx, y, wz, seed ^ 0xF00DBABEu);
            float jitter = ((j & 0xFF) / 255.0f) * 0.8f;
            if (d2 + jitter * jitter * radius > r2) continue;

            int idx = blockIndex(x, y, z);
            // Only convert netherrack; never overwrite air, lava, or bedrock.
            if (blocks[idx] == NETHERRACK) {
                blocks[idx] = GLOWSTONE;
            }
        }
    }

    LOGI("[NetherLS::getChunk] terrain pass done, creating LevelChunk\n");
    LevelChunk* chunk = new LevelChunk(level, blocks, chunkX, chunkZ);
    // Insert into map BEFORE recalcHeightmap. Heightmap/lighting code can
    // call back into level->getChunk for neighbour lookups; if our chunk is
    // not yet registered, each callback generates a fresh neighbour, which
    // recurses again — infinite chunk generation. The ChunkCache wrapper
    // we used to sit behind handled this; since we generate raw, we must
    // self-protect.
    chunkMap.insert(std::make_pair(hashedPos, chunk));
    LOGI("[NetherLS::getChunk] inserted into map, recalcHeightmap\n");
    chunk->recalcHeightmap();
    LOGI("[NetherLS::getChunk] recalcHeightmap done, returning\n");
    return chunk;
}

void NetherLevelSource::postProcess(ChunkSource* parent, int x, int z) {}
bool NetherLevelSource::tick() { return false; }
bool NetherLevelSource::shouldSave() { return true; }

Biome::MobList NetherLevelSource::getMobsAt(const MobCategory& mobCategory, int x, int y, int z)
{
    // Only monster spawns in the Nether — peaceful animals/water creatures
    // never appear. Just zombie pigmen for now; ghasts/magma cubes didn't
    // exist in PE 0.6.1.
    Biome::MobList list;
    if (&mobCategory == &MobCategory::monster) {
        // (mobClassId, probabilityWeight, minCount, maxCount). Weight 100
        // makes pigmen the only enemy the spawner can roll here.
        list.insert(list.end(), Biome::MobSpawnerData(MobTypes::PigZombie, 100, 4, 4));
    }
    return list;
}

std::string NetherLevelSource::gatherStats() {
    return "NetherLevelSource";
}
