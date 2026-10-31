#include "Glass.hpp"
#include <functional>

using namespace geode::prelude;

namespace {
char const* kVert = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;
varying vec2 v_uv;
void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_uv = a_texCoord;
}
)";

char const* kBlur = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 v_uv;
uniform sampler2D CC_Texture0;
uniform vec2 u_dir;
void main() {
    vec2 o1 = u_dir * 1.3846153846;
    vec2 o2 = u_dir * 3.2307692308;
    vec4 c = texture2D(CC_Texture0, v_uv) * 0.2270270270;
    c += (texture2D(CC_Texture0, v_uv + o1) + texture2D(CC_Texture0, v_uv - o1)) * 0.3162162162;
    c += (texture2D(CC_Texture0, v_uv + o2) + texture2D(CC_Texture0, v_uv - o2)) * 0.0702702703;
    gl_FragColor = c;
}
)";

char const* kGlass = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 v_uv;
uniform sampler2D CC_Texture0;
uniform vec2 u_res;
uniform vec2 u_half;
uniform float u_radius;
uniform float u_alpha;
uniform vec3 u_accent;
uniform vec3 u_tint;
float sdRound(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
void main() {
    vec2 p = (v_uv - 0.5) * u_res;
    float d = sdRound(p, u_half, u_radius);
    float mask = 1.0 - smoothstep(-1.0, 0.75, d);
    if (mask <= 0.001) { gl_FragColor = vec4(0.0); return; }
    vec3 bl = texture2D(CC_Texture0, v_uv).rgb;
    float l = dot(bl, vec3(0.299, 0.587, 0.114));
    bl = mix(vec3(l), bl, 0.8);
    vec3 col = mix(bl * 0.5, u_tint, 0.42);
    float g = clamp(p.y / u_half.y * 0.5 + 0.5, 0.0, 1.0);
    col += vec3(0.10, 0.14, 0.16) * pow(g, 2.5);
    float edge = smoothstep(-3.0, 0.0, d) * (1.0 - smoothstep(0.0, 1.0, d));
    col += mix(u_accent, vec3(1.0), 0.45) * edge * 0.5;
    float a = mask * u_alpha;
    gl_FragColor = vec4(col * a, a);
}
)";

CCGLProgram* makeProgram(char const* frag) {
    auto p = new CCGLProgram();
    if (!p->initWithVertexShaderByteArray(kVert, frag)) { delete p; return nullptr; }
    p->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
    p->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
    p->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
    if (!p->link()) { delete p; return nullptr; }
    p->updateUniforms();
    return p;
}

CCGLProgram* blurProgram() {
    static CCGLProgram* p = nullptr; static bool tried = false;
    if (!tried) { tried = true; p = makeProgram(kBlur); }
    return p;
}
CCGLProgram* glassProgram() {
    static CCGLProgram* p = nullptr; static bool tried = false;
    if (!tried) { tried = true; p = makeProgram(kGlass); }
    return p;
}

class ShaderSprite : public CCSprite {
public:
    std::function<void(CCGLProgram*)> m_pre;
    std::pair<Ref<CCRenderTexture>, Ref<CCRenderTexture>> m_extra;
    float m_alpha = 0.f;
    CCPoint m_acc = {0.f, 0.85f}; float m_accB = 0.6f;
    ccColor3B m_accent = {0, 217, 153}, m_tint = {13, 20, 28};

    static ShaderSprite* create(CCTexture2D* tex, CCGLProgram* prog) {
        auto s = new ShaderSprite();
        if (s && s->initWithTexture(tex)) {
            s->autorelease();
            s->setShaderProgram(prog);
            s->setFlipY(true);
            return s;
        }
        delete s; return nullptr;
    }

    void draw() override {
        auto p = this->getShaderProgram();
        p->use();
        if (m_pre) m_pre(p);
        CCSprite::draw();
    }
};

GLint loc(CCGLProgram* p, char const* n) { return glGetUniformLocation(p->getProgram(), n); }
}

namespace xyz::glass {
CCNode* create(CCSize panel, float radius) {
    auto blur = blurProgram(); auto gl = glassProgram();
    auto scene = CCDirector::get()->getRunningScene();
    if (!blur || !gl || !scene) return nullptr;

    auto ws = CCDirector::get()->getWinSize();
    int w = (int)ws.width, h = (int)ws.height;
    Ref<CCRenderTexture> a = CCRenderTexture::create(w, h);
    Ref<CCRenderTexture> b = CCRenderTexture::create(w, h);
    if (!a || !b) return nullptr;

    a->beginWithClear(0, 0, 0, 1);
    scene->visit();
    a->end();

    auto pass = [&](CCRenderTexture* src, CCRenderTexture* dst, float dx, float dy) {
        auto tex = src->getSprite()->getTexture();
        tex->setAntiAliasTexParameters();
        auto s = ShaderSprite::create(tex, blur);
        if (!s) return;
        s->setBlendFunc({GL_ONE, GL_ZERO});
        s->setPosition({ws.width / 2, ws.height / 2});
        float tw = (float)tex->getPixelsWide(), th = (float)tex->getPixelsHigh();
        s->m_pre = [=](CCGLProgram* p) { p->setUniformLocationWith2f(loc(p, "u_dir"), dx / tw, dy / th); };
        dst->beginWithClear(0, 0, 0, 1);
        s->visit();
        dst->end();
    };

    float steps[] = {1.5f, 3.5f, 6.f};
    CCRenderTexture* cur = a; CCRenderTexture* other = b;
    for (float st : steps) {
        pass(cur, other, st, 0); std::swap(cur, other);
        pass(cur, other, 0, st); std::swap(cur, other);
    }

    auto tex = cur->getSprite()->getTexture();
    tex->setAntiAliasTexParameters();
    auto out = ShaderSprite::create(tex, gl);
    if (!out) return nullptr;
    out->setPosition({ws.width / 2, ws.height / 2});
    out->m_pre = [out = out, ws, panel, radius](CCGLProgram* p) {
        p->setUniformLocationWith2f(loc(p, "u_res"), ws.width, ws.height);
        p->setUniformLocationWith2f(loc(p, "u_half"), panel.width / 2, panel.height / 2);
        p->setUniformLocationWith1f(loc(p, "u_radius"), radius);
        p->setUniformLocationWith1f(loc(p, "u_alpha"), out->m_alpha);
        p->setUniformLocationWith3f(loc(p, "u_accent"), out->m_accent.r / 255.f, out->m_accent.g / 255.f, out->m_accent.b / 255.f);
        p->setUniformLocationWith3f(loc(p, "u_tint"), out->m_tint.r / 255.f, out->m_tint.g / 255.f, out->m_tint.b / 255.f);
    };
    out->m_extra = std::make_pair(Ref<CCRenderTexture>(a), Ref<CCRenderTexture>(b));
    return out;
}

void setTheme(CCNode* glass, ccColor3B accent, ccColor3B tint) {
    if (auto s = static_cast<ShaderSprite*>(glass)) { s->m_accent = accent; s->m_tint = tint; }
}

void setAlpha(CCNode* glass, float alpha) {
    if (auto s = static_cast<ShaderSprite*>(glass)) s->m_alpha = alpha;
}
}
