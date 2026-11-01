#include "LayoutData.hpp"
#include "../core/State.hpp"
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <map>

using namespace geode::prelude;

namespace {
std::vector<std::string> split(std::string const& s, char c) {
    std::vector<std::string> out; std::string cur;
    for (char ch : s) { if (ch == c) { out.push_back(cur); cur.clear(); } else cur += ch; }
    out.push_back(cur);
    return out;
}

std::string join(std::vector<std::string> const& v, std::string const& sep = ",") {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) { r += v[i]; if (i + 1 != v.size()) r += sep; }
    return r;
}

int toInt(std::string const& s) { return numFromString<int>(s).unwrapOr(0); }
float toFloat(std::string const& s) { return numFromString<float>(s).unwrapOr(0.f); }
}

namespace xyz {
std::string layoutString(std::string levelString) {
    if (levelString.empty()) return "";
    std::string decomp = ZipUtils::decompressString(levelString.c_str(), true, 0);
    auto parts = split(decomp, ';');
    if (parts.empty()) return levelString;
    auto settings = split(parts[0], ',');
    if (settings.size() < 2) return levelString;
    settings[1] = newColors;
    std::string first = join(settings);

    std::unordered_set<int> important;
    for (size_t i = 0; i + 1 < settings.size(); ++i)
        if (settings[i] == "kA36") { if (toInt(settings[i + 1]) != 0) important.insert(toInt(settings[i + 1])); break; }

    std::vector<LevelObject> objs;
    for (size_t i = 1; i < parts.size(); ++i) {
        auto res = split(parts[i], ',');
        std::map<int, std::string> props;
        size_t hidden = 0;
        for (size_t k = 0; k + 1 < res.size(); k += 2) {
            auto id = numFromString<int>(res[k]);
            if (id.isErr()) continue;
            if (res[k] == "135") hidden = k;
            props[id.unwrap()] = res[k + 1];
        }
        objs.push_back({res, props, hidden, parts[i]});
        if (!props.contains(1)) continue;
        auto it = importantTriggerIDs.find(toInt(props.at(1)));
        if (it == importantTriggerIDs.end()) continue;
        for (int pid : it->second)
            if (props.contains(pid) && toInt(props[pid]) != 0) important.insert(toInt(props[pid]));
    }

    std::string out;
    for (auto& o : objs) {
        auto vec = o.vecString; auto& props = o.props; size_t hidden = o.hiddenIndex;
        if (!props.contains(1)) continue;
        int oid = toInt(props.at(1));
        if (decoObjectIDs.contains(oid) || props.contains(121)) {
            if (!props.contains(57)) continue;
            for (auto& el : split(props.at(57), '.')) {
                if (el.empty() || !important.contains(toInt(el))) continue;
                if (!props.contains(135)) { vec.push_back("135"); vec.push_back("1"); }
                out += ";" + join(vec);
                break;
            }
            continue;
        }
        if (!solidObjectIDs.contains(oid)) { out += ";" + o.ogString; continue; }
        if (props.contains(129) && toFloat(props[129]) <= 0.f && hidden == 0) {
            vec.push_back("135"); vec.push_back("1");
            out += ";" + join(vec);
            continue;
        }
        if (hidden != 0) {
            vec.erase(vec.begin() + hidden); vec.erase(vec.begin() + hidden);
            out += ";" + join(vec);
        } else out += ";" + o.ogString;
    }
    return first + out + ";";
}
}

static bool g_layout = false;
bool xyz::layoutOn() { return g_layout && PlayLayer::get(); }

class $modify(XyzLayoutTools, LevelTools) {
    static bool verifyLevelIntegrity(gd::string v1, int v2) {
        if (g_layout) return true;
        return LevelTools::verifyLevelIntegrity(v1, v2);
    }
};

class $modify(XyzLayoutPL, PlayLayer) {
    void addObject(GameObject* obj) {
        if (!g_layout) return PlayLayer::addObject(obj);
        if (excludedTriggerIDs.contains(obj->m_objectID)) return;
        PlayLayer::addObject(obj);
        obj->m_activeMainColorID = -1;
        obj->m_activeDetailColorID = -1;
        obj->m_detailUsesHSV = false;
        obj->m_baseUsesHSV = false;
        obj->m_hasNoGlow = true;
        obj->m_isHide = obj->m_objectID == 2065;
        obj->setOpacity(obj->m_objectID == 2065 ? 0 : 255);
        obj->setVisible(obj->m_objectID != 2065);
    }

    bool init(GJGameLevel* level, bool a, bool b) {
        g_layout = xyz::cfg().layout;
#ifdef GEODE_IS_ANDROID
        if (robtopLevelIDs.contains(level->m_levelID.value())) g_layout = false;
#endif
        std::string old = level->m_levelString;
        if (g_layout) level->m_levelString = xyz::layoutString(old);
        bool ok = PlayLayer::init(level, a, b);
        if (g_layout) level->m_levelString = old;
        return ok;
    }
};
