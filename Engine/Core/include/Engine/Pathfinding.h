#pragma once

#include <vector>
#include <cstdint>

namespace Genesis::Engine {

struct GridCoord {
    int x = 0;
    int y = 0;

    bool operator==(const GridCoord& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const GridCoord& other) const {
        return !(*this == other);
    }
};

struct GridPathResult {
    bool success = false;
    float cost = 0.0f;
    std::vector<GridCoord> path;
};

class GridGraph {
public:
    GridGraph(int width, int height);

    int Width() const { return m_width; }
    int Height() const { return m_height; }

    bool InBounds(const GridCoord& c) const;
    bool IsWalkable(const GridCoord& c) const;

    void SetWalkable(const GridCoord& c, bool walkable);

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_walkable;

    size_t Index(const GridCoord& c) const;
};

GridPathResult FindPath(const GridGraph& grid, const GridCoord& start, const GridCoord& goal);

} // namespace Genesis::Engine
