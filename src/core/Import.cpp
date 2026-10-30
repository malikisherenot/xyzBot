#include "State.hpp"
#include "../third_party/json.hpp"
#include <fstream>
#include <sstream>

using namespace geode::prelude;

namespace xyz {
static bool importGdr(Macro& m, std::vector<uint8_t> const& data) {
    using json = nlohmann::json;
    json j = json::from_msgpack(data, true, false);
    if (j.is_discarded()) j = json::parse(data, nullptr, false);
    if (j.is_discarded() || !j.contains("inputs")) return false;
    float fps = j.value("framerate", 240.f);
    m = Macro{};
    m.tps = (uint32_t)fps;
    for (auto& i : j["inputs"]) {
        if (!i.contains("frame") || i["frame"].is_null()) continue;
        uint32_t t = i["frame"].get<int>();
        m.inputs.push_back({t, (uint8_t)i["btn"].get<int>(), i["down"].get<bool>(), i.value("2p", false)});
        m.ticks = std::max(m.ticks, t);
    }
    std::stable_sort(m.inputs.begin(), m.inputs.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
    if (j.contains("seed") && j["seed"].is_number()) m.seed = j["seed"].get<uint64_t>();
    return !m.inputs.empty();
}

static bool importText(Macro& m, std::string const& s) {
    m = Macro{};
    std::istringstream in(s); std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("tps=", 0) == 0) { m.tps = std::stoul(line.substr(4)); continue; }
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ls(line); uint32_t t; int b, d, p = 0;
        if (!(ls >> t >> b >> d)) continue;
        ls >> p;
        m.inputs.push_back({t, (uint8_t)b, d != 0, p != 0});
        m.ticks = std::max(m.ticks, t);
    }
    return !m.inputs.empty();
}

bool importMacro(Macro& m, std::filesystem::path const& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    auto ext = file.extension().string();
    if (ext == ".gdr") return importGdr(m, data);
    if (ext == ".txt" || ext == ".csv") return importText(m, std::string(data.begin(), data.end()));
    return false;
}
}
