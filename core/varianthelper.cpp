#include "varianthelper.h"

#include <QMap>
#include <QSizeF>
#include <QPointF>

static QVariant stringToSize(QString const & str)
{
    int n = str.indexOf('x');
    if (n < 0)
        n = str.indexOf(',');
    if (n < 0)
        return QVariant();
    qreal w = str.left(n).trimmed().toDouble();
    qreal h = str.mid(n + 1).trimmed().toDouble();
    return QSizeF{w, h};
}

static QVariant stringToPos(QString const & str)
{
    int n = str.indexOf(',');
    if (n < 0)
        return QVariant();
    qreal x = str.left(n).trimmed().toDouble();
    qreal y = str.mid(n + 1).trimmed().toDouble();
    return QPointF{x, y};
}

static QMap<int, QVariant (*) (QString const &)> converters;

QVariant VariantHelper::convert(QVariant v, int type)
{
    return convert2(v, type) ? v : QVariant();
}

bool VariantHelper::convert2(QVariant &v, int type)
{
    if (converters.isEmpty()) {
        converters[QMetaType::QSizeF] = stringToSize;
        converters[QMetaType::QPointF] = stringToPos;
    }
    QVariant t = v;
    if (v.canConvert(type) && v.convert(type))
        return true;
    v = t;
    if (v.userType() != QMetaType::QString
            && !v.convert(QMetaType::QString)) {
        v = t;
        return false;
    }
    if (auto c = converters.value(type)) {
        v = c(v.toString());
        return true;
    }
    if (t.userType() != QMetaType::QString && v.convert(type))
        return true;
    v = t;
    return false;
}
