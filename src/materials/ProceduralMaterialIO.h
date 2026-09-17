#pragma once
#include "../Material.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
inline QString EncodeFabric(const Material& m) {
    if(!m.fabric.enabled)return {};
    QJsonObject o{{"version",1},{"weave",m.fabric.weave},{"seed",double(m.fabric.seed)},{"useUV",m.fabric.useUV}};
#define WRITE_FABRIC(name,value,lo,hi,label) o[#name]=double(m.fabric.name);
    DOM_FABRIC_PARAMETERS(WRITE_FABRIC)
#undef WRITE_FABRIC
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}
inline bool DecodeFabric(const QString& text,Material& m) {
    m.fabric={};if(text.isEmpty())return true;
    const auto doc=QJsonDocument::fromJson(text.toUtf8());if(!doc.isObject())return false;
    const auto o=doc.object();if(o.value("version").toInt()!=1)return false;
    auto p=m.fabric;
    if(o.contains("useUV")&&!o.value("useUV").isBool())return false;
    p.useUV=o.value("useUV").toBool();
#define READ_FABRIC(name,defaultValue,lo,hi,label) {if(o.contains(#name)&&!o.value(#name).isDouble())return false;double v=o.value(#name).toDouble(defaultValue);if(!std::isfinite(v)||v<lo||v>hi)return false;p.name=float(v);}
    DOM_FABRIC_PARAMETERS(READ_FABRIC)
#undef READ_FABRIC
    if(!o.value("weave").isDouble()||!o.value("seed").isDouble())return false;
    double weave=o.value("weave").toDouble(),seed=o.value("seed").toDouble();
    if(!std::isfinite(weave)||weave<0||weave>3||std::floor(weave)!=weave
        ||!std::isfinite(seed)||seed<0||seed>4294967295.0||std::floor(seed)!=seed)return false;
    p.weave=int(weave);p.seed=std::uint32_t(seed);p.enabled=true;m.fabric=p;return true;
}
inline QString EncodePlaster(const Material& m) {
    if(!m.plaster.enabled)return {};
    QJsonObject o{{"version",1},{"seed",double(m.plaster.seed)},{"highQuality",m.plaster.highQuality}};
#define WRITE(name,value,lo,hi,label) o[#name]=double(m.plaster.name);
    DOM_PLASTER_PARAMETERS(WRITE)
#undef WRITE
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}
inline bool DecodePlaster(const QString& text,Material& m){
    m.plaster={};if(text.isEmpty())return true;
    auto doc=QJsonDocument::fromJson(text.toUtf8());if(!doc.isObject())return false;
    auto o=doc.object();if(o.value("version").toInt()!=1)return false;
    auto p=m.plaster;
#define READ(name,defaultValue,lo,hi,label) {if(o.contains(#name)&&!o.value(#name).isDouble())return false;double v=o.value(#name).toDouble(defaultValue);if(!std::isfinite(v)||v<lo||v>hi)return false;p.name=float(v);}
    DOM_PLASTER_PARAMETERS(READ)
#undef READ
    if(std::floor(p.pattern)!=p.pattern)return false;
    if(o.contains("seed")&&!o.value("seed").isDouble())return false;
    if(o.contains("highQuality")&&!o.value("highQuality").isBool())return false;
    double seed=o.value("seed").toDouble(1);if(!std::isfinite(seed)||seed<0||seed>4294967295.0||std::floor(seed)!=seed)return false;
    p.seed=std::uint32_t(seed);p.highQuality=o.value("highQuality").toBool();p.enabled=true;m.plaster=p;return true;
}

inline QString EncodePerforation(const Material& m) {
    if(!m.perforation.enabled)return {};
    QJsonObject o{{"version",1},{"pattern",m.perforation.pattern},{"useUV",m.perforation.useUV}};
#define WRITE_PERFORATION(name,value,lo,hi,label) o[#name]=double(m.perforation.name);
    DOM_PERFORATION_PARAMETERS(WRITE_PERFORATION)
#undef WRITE_PERFORATION
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}
inline bool DecodePerforation(const QString& text,Material& m) {
    m.perforation={};if(text.isEmpty())return true;
    const auto doc=QJsonDocument::fromJson(text.toUtf8());if(!doc.isObject())return false;
    const auto o=doc.object();if(o.value("version").toInt()!=1)return false;
    auto p=m.perforation;
    if(!o.value("useUV").isBool()||!o.value("pattern").isDouble())return false;
    double pattern=o.value("pattern").toDouble();if(pattern!=0&&pattern!=1)return false;
    p.pattern=int(pattern);p.useUV=o.value("useUV").toBool();
#define READ_PERFORATION(name,defaultValue,lo,hi,label) {if(o.contains(#name)&&!o.value(#name).isDouble())return false;double v=o.value(#name).toDouble(defaultValue);if(!std::isfinite(v)||v<lo||v>hi)return false;p.name=float(v);}
    DOM_PERFORATION_PARAMETERS(READ_PERFORATION)
#undef READ_PERFORATION
    p.enabled=true;m.perforation=p;return true;
}
