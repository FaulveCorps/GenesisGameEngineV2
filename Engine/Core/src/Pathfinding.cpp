#include "engine/Pathfinding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace Genesis::Engine {

GridGraph::GridGraph(int width, int height)
    : m_width(width), m_height(height), m_walkable(static_cast<size_t>(width * height), 1) {}

bool GridGraph::InBounds(const GridCoord& c) const {
    return c.x >= 0 && c.y >= 0 && c.x < m_width && c.y < m_height;
}

bool GridGraph::IsWalkable(const GridCoord& c) const {
    if (!InBounds(c)) return false;
    return m_walkable[Index(c)] != 0;
}

void GridGraph::SetWalkable(const GridCoord& c, bool walkable) {
    if (!InBounds(c)) return;
    m_walkable[Index(c)] = walkable ? 1 : 0;
}

size_t GridGraph::Index(const GridCoord& c) const {
    return static_cast<size_t>(c.y * m_width + c.x);
}

static float Heuristic(const GridCoord& a, const GridCoord& b) {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

GridPathResult FindPath(const GridGraph& grid, const GridCoord& start, const GridCoord& goal) {
    GridPathResult result;

    if (!grid.InBounds(start) || !grid.InBounds(goal)) return result;
    if (!grid.IsWalkable(start) || !grid.IsWalkable(goal)) return result;

    if (start == goal) {
        result.success = true;
        result.cost = 0.0f;
        result.path.push_back(start);
        return result;
    }

    const int width = grid.Width();
    const int height = grid.Height();
    const int total = width * height;

    std::vector<float> gScore(static_cast<size_t>(total), std::numeric_limits<float>::infinity());
    std::vector<int> cameFrom(static_cast<size_t>(total), -1);
    std::vector<uint8_t> inClosed(static_cast<size_t>(total), 0);

    auto indexOf = [&](const GridCoord& c) {
        return c.y * width + c.x;
    };

    struct Node {
        GridCoord coord;
        float fScore;
    };

    struct NodeCompare {
        bool operator()(const Node& a, const Node& b) const {
            return a.fScore > b.fScore;
        }
    };

    std::priority_queue<Node, std::vector<Node>, NodeCompare> open;

    int startIndex = indexOf(start);
    gScore[startIndex] = 0.0f;
    open.push(Node{start, Heuristic(start, goal)});

    const GridCoord directions[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    while (!open.empty()) {
        GridCoord current = open.top().coord;
        open.pop();

        int currentIndex = indexOf(current);
        if (inClosed[currentIndex]) continue;
        inClosed[currentIndex] = 1;

        if (current == goal) {
            std::vector<GridCoord> path;
            int idx = currentIndex;
            while (idx >= 0) {
                GridCoord c{ idx % width, idx / width };
                path.push_back(c);
                if (idx == startIndex) break;
                idx = cameFrom[static_cast<size_t>(idx)];
            }
            std::reverse(path.begin(), path.end());
            result.success = true;
            result.cost = gScore[currentIndex];
            result.path = std::move(path);
            return result;
        }

        for (const auto& d : directions) {
            GridCoord neighbor{ current.x + d.x, current.y + d.y };
            if (!grid.IsWalkable(neighbor)) continue;

            int nIndex = indexOf(neighbor);
            float tentative = gScore[currentIndex] + 1.0f;
            if (tentative < gScore[nIndex]) {
                cameFrom[nIndex] = currentIndex;
                gScore[nIndex] = tentative;
                float fScore = tentative + Heuristic(neighbor, goal);
                open.push(Node{neighbor, fScore});
            }
        }
    }

    return result;
}

} // namespace Genesis::Engine
