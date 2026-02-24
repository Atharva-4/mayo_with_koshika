#ifndef STL_MERGER_H
#define STL_MERGER_H

#include <string>
#include <vector>
#include <fstream>

#include "STLCutter.h"   // 🔥 reuse existing definitions

namespace Mayo {

    class STLMerger {
    public:
        STLMerger();
        bool loadSTL(const std::string& filePath);
        void merge();
        bool saveMerged(const std::string& outputPath);

    private:
        std::vector<Facet> m_allFacets;
        std::vector<Facet> m_mergedFacets;

        Vec3 computeNormal(const Vec3& a, const Vec3& b, const Vec3& c);
        bool parseFacet(std::ifstream& in, Facet& f);
    };

} // namespace Mayo

#endif