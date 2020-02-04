#include "control.h"
#include "resource.h"
#include "resourceview.h"
#include "toolbutton.h"
#include "views/whitecanvas.h"
#include "views/stateitem.h"
#include "views/itemframe.h"
#include "views/itemselector.h"
#include "resourcetransform.h"
#include "controltransform.h"

#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QWidget>
#include <QGraphicsScene>
#include <QTransform>
#include <QGraphicsTransform>
#include <QMetaMethod>

#include <map>

char const * Control::EXPORT_ATTR_TYPE = "ctrl_type";

static qreal MIN_SIZE = 120.0;
static qreal MAX_SIZE = 4096.0;

Control * Control::fromItem(QGraphicsItem * item)
{
    return item->data(ITEM_KEY_CONTROL).value<Control *>();
}

ToolButton Control::btnTop = { "top", "置顶", nullptr, ":/showboard/icons/top.svg" };
ToolButton Control::btnCopy = { "copy", "复制", nullptr, ":/showboard/icons/copy.png" };
ToolButton Control::btnFastCopy = { "copy", "快速复制", ToolButton::Checkable, ":/showboard/icons/copy.svg" };
ToolButton Control::btnDelete = { "delete", "关闭", nullptr, ":/showboard/icons/close.png" };

Control::Control(ResourceView *res, Flags flags, Flags clearFlags)
    : flags_((DefaultFlags | flags) & ~clearFlags)
    , res_(res)
    , transform_(nullptr)
    , item_(nullptr)
    , itemObj_(nullptr)
    , realItem_(nullptr)
    , stateItem_(nullptr)
{
    if (res_->flags().testFlag(ResourceView::LargeCanvas)) {
        flags_.setFlag(FullLayout, true);
        flags_.setFlag(CanSelect, false);
        flags_.setFlag(CanRotate, false);
    }
    transform_ = new ControlTransform(res->transform());
    if (res_->flags() & ResourceView::SavedSession) {
        flags_ |= RestoreSession;
        if (res_->flags().testFlag(ResourceView::LargeCanvas)) {
            flags_.setFlag(DefaultFlags, false);
        }
    }
}

Control::~Control()
{
    if (transform_)
        delete transform_;
    if (realItem_)
        delete realItem_;
    realItem_ = nullptr;
    item_ = nullptr;
    transform_ = nullptr;
    res_ = nullptr;
}

void Control::attachTo(QGraphicsItem * parent)
{
    item_ = create(res_);
    itemObj_ = item_->toGraphicsObject();
    if (transform_)
        item_->setTransformations({transform_});
    item_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    realItem_ = item_;
    if (flags_ & WithSelectBar) {
        itemFrame()->addTopBar();
    }
    attaching();
    realItem_->setParentItem(parent);
    loadSettings();
    sizeChanged();
    initPosition();
    relayout();
    flags_ |= Loading;
    whiteCanvas()->onControlLoad(true);
    attached();
    if (flags_ & Loading) {
        stateItem()->setLoading("正在打开：“" + res_->name() + "”");
    }
}

void Control::detachFrom(QGraphicsItem *parent)
{
    detaching();
    if (flags_ & Loading)
        whiteCanvas()->onControlLoad(false);
    if (flags_ & LoadFinished)
        saveSettings();
    (void) parent;
    realItem_->scene()->removeItem(realItem_);
    realItem_->setTransformations({});
    realItem_->setData(ITEM_KEY_CONTROL, QVariant());
    detached();
    //deleteLater();
    delete this;
}

void Control::relayout()
{
    if (flags_ & FullLayout) {
        resize(whiteCanvas()->rect().size());
        sizeChanged();
    } else {

    }
}

void Control::beforeClone()
{
    if (flags_ & LoadFinished)
        saveSettings();
}

void Control::afterClone(ResourceView *)
{
}

void Control::attaching()
{
}

void Control::attached()
{
    loadFinished(true);
}

void Control::detaching()
{
}

void Control::detached()
{
}

void Control::loadSettings()
{
    int count = 0;
    if (itemObj_) {
        QMetaObject const * meta = itemObj_->metaObject();
        while (meta->className()[0] != 'Q' || meta->className()[1] >= 'a')
            meta = meta->superClass();
        count = meta->propertyCount();
    }
    int index;
    for (QByteArray & k : res_->dynamicPropertyNames()) {
        if (itemObj_ && (index = itemObj_->metaObject()->indexOfProperty(k)) >= count) {
            itemObj_->setProperty(k, res_->property(k));
        } else {
            setProperty(k, res_->property(k));
        }
    }
}

void Control::saveSettings()
{
    for (QByteArray & k : dynamicPropertyNames())
        res_->setProperty(k, property(k));
    for (int i = Control::staticMetaObject.propertyCount();
            i < metaObject()->propertyCount(); ++i) {
        QMetaProperty p = metaObject()->property(i);
        res_->setProperty(p.name(), p.read(this));
    }
    // special one
    res_->setProperty("sizeHint", sizeHint());
    // object item
    int count = 0;
    if (itemObj_) {
        QMetaObject const * meta = itemObj_->metaObject();
        while (meta->className()[0] != 'Q' || meta->className()[1] >= 'a')
            meta = meta->superClass();
        count = meta->propertyCount();
        for (int i = count;
                i < itemObj_->metaObject()->propertyCount(); ++i) {
            QMetaProperty p = itemObj_->metaObject()->property(i);
            res_->setProperty(p.name(), p.read(itemObj_));
        }
    }
    res_->setSaved();
}

void Control::sizeChanged()
{
    QRectF rect = item_->boundingRect();
    QPointF center(rect.center());
    if (flags_ & LoadFinished) {
        if (flags_ & (Adjusting | FullLayout)) {
            item_->setTransform(QTransform::fromTranslate(-center.x(), -center.y()));
        } else {
            // keep top left
            QTransform t = item_->transform();
            center = t.map(center);
            move(center);
            t.translate(-center.x(), -center.y());
            item_->setTransform(t);
        }
    } else {
        item_->setTransform(QTransform::fromTranslate(-center.x(), -center.y()));
    }
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->updateRect();
    if (stateItem_) {
        stateItem_->updateTransform();
    }
    ItemSelector * selector = whiteCanvas()->selector();
    selector->updateSelect(realItem_);
}

QSizeF Control::sizeHint()
{
    return item_->boundingRect().size();
}

static void adjustSizeHint(QSizeF & size, QSizeF const & psize)
{
    if (size.width() < 10.0) {
        size.setWidth(psize.width() * size.width());
    }
    if (size.height() < 0)
        size.setHeight(size.width() * -size.height());
    else if (size.height() < 10.0)
        size.setHeight(psize.height() * size.height());
}

// called before attached
void Control::setSizeHint(QSizeF const & size)
{
    if (size.width() < 10.0 || size.height() < 10.0) {
        QSizeF size2 = size;
        QRectF rect = whiteCanvas()->rect();
        adjustSizeHint(size2, rect.size());
        resize(size2);
    } else {
        resize(size);
    }
}

WhiteCanvas *Control::whiteCanvas()
{
    return static_cast<WhiteCanvas*>(realItem_->parentItem()->parentItem());
}

void Control::resize(QSizeF const & size)
{
    if (flags_ & (LoadFinished | RestoreSession)) {
        return;
    }
    setProperty("delaySizeHint", size);
}

static constexpr qreal CROSS_LENGTH = 20;

Control::SelectMode Control::selectTest(const QPointF &point)
{
    (void) point;
    return NotSelect;
}

Control::SelectMode Control::selectTest(QGraphicsItem * child, QGraphicsItem * item, QPointF const & point)
{
    if (flags_ & FullSelect)
        return Select;
    if (res_->flags().testFlag(ResourceView::LargeCanvas))
        return PassSelect;
    if (item_ != realItem_ && item == realItem_) {
        return static_cast<ItemFrame*>(realItem_)->hitTest(child, point) ? Select : NotSelect;
    } else if (stateItem_ == item && (flags_ & LoadFinished) == 0) {
        return Select;
    } else if (item_ == item && (flags_ & HelpSelect)) {
        QRectF rect = item_->boundingRect();
        rect.adjust(CROSS_LENGTH, CROSS_LENGTH, -CROSS_LENGTH, -CROSS_LENGTH);
        return rect.contains(point) ? NotSelect : Select;
    } else if (item_ == realItem_) {
        return selectTest(point);
    } else {
        return selectTest(realItem_->mapToItem(item_, point));
    }
}

static qreal polygonArea(QPolygonF const & p)
{
    qreal area = 0;
    int j = 0;
    for (int i = 1; i < p.size(); ++i) {
        area += (p[j].x() + p[i].x()) * (p[j].y() - p[i].y());
        j = i;
    }
    return qAbs(area) / 2.0;
}

void Control::initPosition()
{
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->updateRect();
    if (flags_ & (FullLayout | RestoreSession))
        return;
    QGraphicsItem *parent = realItem_->parentItem();
    QRectF rect = parent->boundingRect();
    Control * canvasControl = fromItem(whiteCanvas());
    if (canvasControl) {
        rect = parent->mapFromScene(parent->scene()->sceneRect()).boundingRect();
        if (!(flags_ & AutoPosition))
            res_->transform().translate(rect.center());
    }
    if (!(flags_ & AutoPosition))
        return;
    QPolygonF polygon;
    for (QGraphicsItem * c : parent->childItems()) {
        if (c == realItem_ || Control::fromItem(c)->flags() & FullLayout)
            continue;
        polygon = polygon.united(c->mapToParent(c->boundingRect()));
    }
    qreal dx = rect.width() / 3.0;
    qreal dy = rect.height() / 3.0;
    rect.adjust(0, 0, -dx - dx, -dy - dy);
    qreal minArea = dx * dy;
    QPointF pos;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            qreal area = polygonArea(polygon.intersected(rect));
            if (qFuzzyIsNull(minArea - area) && i == 1 && j == 1) {
                pos = rect.center();
            } else if (minArea > area) {
                minArea = area;
                pos = rect.center();
            }
            rect.adjust(dx, 0, dx, 0);
        }
        qreal dx3 = -dx * 3;
        rect.adjust(dx3, dy, dx3, dy);
    }
    res_->transform().translate(pos);
}

void Control::loadFinished(bool ok, QString const & iconOrMsg)
{
    if (ok) {
        if (iconOrMsg.isNull()) {
            if (stateItem_) {
                QList<QGraphicsTransform*> trs = stateItem_->transformations();
                stateItem_->setTransformations({});
                for (QGraphicsTransform* tr: trs)
                    delete tr;
                delete stateItem_;
                stateItem_ = nullptr;
            }
        } else {
            stateItem()->setLoaded(iconOrMsg);
        }
        initScale();
        sizeChanged();
        flags_ |= LoadFinished;
    } else {
        QString msg = res_->name() + "\n"
                + (iconOrMsg.isEmpty() ? "加载失败，点击即可重试" : iconOrMsg);
        stateItem()->setFailed(msg);
        QObject::connect(stateItem(), &StateItem::clicked, this, &Control::reload);
        sizeChanged();
    }
    flags_ &= ~Loading;
    whiteCanvas()->onControlLoad(false);
}

void Control::initScale()
{
    QSizeF ps = whiteCanvas()->rect().size();
    QSizeF size = item_->boundingRect().size();
    if (res_->flags().testFlag(ResourceView::LargeCanvas)) {
        Control * canvasControl = fromItem(whiteCanvas());
        if (canvasControl) {
            canvasControl->flags_.setFlag(CanScale, flags_.testFlag(CanScale));
            flags_.setFlag(DefaultFlags, 0);
            if (flags_ & RestoreSession) {
                return;
            }
            canvasControl->resize(size);
            size -= ps;
            QPointF d(size.width() / 2, size.height() / 2);
            canvasControl->move(d);
            return;
        }
    }
    if (flags_ & (FullLayout | LoadFinished | RestoreSession)) {
        return;
    }
    Control * canvasControl = fromItem(whiteCanvas());
    if (canvasControl) {
        ps = item_->scene()->sceneRect().size();
    }
    QVariant delaySizeHint = property("delaySizeHint");
    if (delaySizeHint.isValid()) {
        ps = delaySizeHint.toSizeF();
        delaySizeHint.clear();
        setProperty("delaySizeHint", delaySizeHint);
    }
    if (item_ != realItem_) {
        QRectF padding(static_cast<ItemFrame *>(realItem_)->padding());
        ps.setWidth(ps.width() - padding.width());
        ps.setHeight(ps.height() - padding.height());
    }
    qreal scale = 1.0;
    while (size.width() > ps.width() || size.height() > ps.height()) {
        size /= 2.0;
        scale /= 2.0;
    }
    if (flags_ & ExpandScale) {
        while (size.width() * 2.0 < ps.width() && size.height() * 2.0 < ps.height()) {
            size *= 2.0;
            scale *= 2.0;
        }
    }
    if (flags_ & LayoutScale) {
        resize(size);
    } else {
        res_->transform().scaleTo(scale);
    }
    if (canvasControl) {
        qreal s = 1 / canvasControl->resource()->transform().scale().m11();
        res_->transform().scale(QSizeF(s, s));
    }
    if (item_ != realItem_) {
        res_->transform().translate(
                    -static_cast<ItemFrame *>(realItem_)->padding().center());
    }
}

void Control::setSize(const QSizeF &size)
{
    QSizeF size2(size);
    adjustSizeHint(size2, whiteCanvas()->rect().size());
    if (flags_ & Adjusting) {
        setProperty("delayResize", size2);
    } else {
        resize(size2);
        sizeChanged();
    }
}

void Control::move(QPointF & delta)
{
    res_->transform().translate(delta);
}

bool Control::scale(QRectF &rect, const QRectF &direction, QPointF &delta)
{
    QRectF padding;
    if (realItem_ != item_)
        padding = itemFrame()->padding();
    qreal limitSize[] = {MIN_SIZE, MAX_SIZE};
    bool result = res_->transform().scale(rect, direction, delta, padding,
                            flags_ & KeepAspectRatio, flags_ & LayoutScale, limitSize);
    if (!result)
        return false;
    QRectF origin = rect;
    if (item_ != realItem_) {
        static_cast<ItemFrame *>(realItem_)->updateRectToChild(origin);
    }
    if (flags_ & LayoutScale) {
        resize(origin.size());
        sizeChanged();
    }
    return true;
}

void Control::gesture(const QPointF &from1, const QPointF &from2, QPointF &to1, QPointF &to2)
{
    qreal layoutScale = 1.0;
    qreal * pLayoutScale = (flags_ & LayoutScale) ? &layoutScale : nullptr;
    qreal limitScale[] = {1.0, 10.0};
    if (flags_ & CanScale) {
        QSizeF size = item_->boundingRect().size();
        limitScale[0] = qMin(MIN_SIZE / size.width(), MIN_SIZE / size.height());
        limitScale[1] = qMin(MAX_SIZE / size.width(), MAX_SIZE / size.height());
    }
    res_->transform().gesture(from1, from2, to1, to2,
                              flags_ & CanMove, flags_ & CanScale, flags_ & CanRotate, limitScale, pLayoutScale);
    if (pLayoutScale) {
        QRectF rect = realItem_->boundingRect();
        rect.setWidth(rect.width() * layoutScale);
        rect.setHeight(rect.height() * layoutScale);
        if (item_ != realItem_) {
            static_cast<ItemFrame *>(realItem_)->updateRectToChild(rect);
        }
        resize(rect.size());
        sizeChanged();
    } else if (item_ != realItem_) {
        static_cast<ItemFrame *>(realItem_)->updateRect();
    }
}

void Control::rotate(QPointF const & from, QPointF & to)
{
    res_->transform().rotate(from, to);
}

void Control::rotate(QPointF const & center, QPointF const & from, QPointF &to)
{
    res_->transform().rotate(center, from, to);
}

QRectF Control::boundRect() const
{
    QRectF rect = realItem_->boundingRect();
    if (item_ == realItem_) {
        rect.moveCenter({0, 0});
        QTransform const & scale = res_->transform().scale();
        rect = QRectF(rect.x() * scale.m11(), rect.y() * scale.m22(),
                      rect.width() * scale.m11(), rect.height() * scale.m22());
    }
    if (!(flags_ & LoadFinished) && stateItem_) {
        rect |= stateItem_->boundingRect();
    }
    return rect;
}

void Control::select(bool selected)
{
    flags_.setFlag(Selected, selected);
    if (realItem_ != item_)
        static_cast<ItemFrame *>(realItem_)->setSelected(selected);
}

void Control::adjusting(bool be)
{
    flags_.setFlag(Adjusting, be);
    if (!be) {
        QVariant delayResize = property("delayResize");
        if (delayResize.isValid()) {
            setProperty("delayResize", QVariant());
            resize(delayResize.toSizeF());
            sizeChanged();
        }
    }
}

ItemFrame * Control::itemFrame()
{
    if (item_ != realItem_) {
        return static_cast<ItemFrame*>(realItem_);
    }
    ItemFrame * frame = new ItemFrame(item_);
    realItem_ = frame;
    ControlTransform* ct = static_cast<ControlTransform*>(transform_)->addFrameTransform();
    realItem_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    realItem_->setTransformations({ct});
    return frame;
}

void Control::loadStream()
{
    QWeakPointer<int> l = life();
    res_->resource()->getStream().then([this, l] (QSharedPointer<QIODevice> stream) {
        if (l.isNull()) return;
        onStream(stream.get());
        loadFinished(true);
    }).fail([this, l](std::exception& e) {
        loadFinished(true, e.what());
    });
}

void Control::loadData()
{
    QWeakPointer<int> l = life();
    res_->resource()->getData().then([this, l] (QByteArray data) {
        if (l.isNull()) return;
        onData(data);
        loadFinished(true);
    }).fail([this, l](std::exception& e) {
        loadFinished(false, e.what());
    });
}

void Control::loadText()
{
    QWeakPointer<int> l = life();
    res_->resource()->getText().then([this, l] (QString text) {
        if (l.isNull()) return;
        onText(text);
        loadFinished(true);
    }).fail([this, l](std::exception& e) {
        loadFinished(true, e.what());
    });
}

void Control::reload()
{
    QObject::disconnect(stateItem(), &StateItem::clicked, this, &Control::reload);
    if (!(flags_ & LoadFinished)) {
        flags_ |= Loading;
        stateItem()->setLoading("正在打开：“" + res_->name() + "”");
        whiteCanvas()->onControlLoad(true);
        attached(); // reload
    }
}

void Control::onStream(QIODevice *stream)
{
    (void) stream;
    throw std::runtime_error("Not implemets onStream");
}

void Control::onData(QByteArray data)
{
    (void) data;
    throw std::runtime_error("Not implemets onData");
}

void Control::onText(QString text)
{
    (void) text;
    throw std::runtime_error("Not implemets onText");
}

void Control::getToolButtons(QList<ToolButton *> &buttons, const QList<ToolButton *> &parents)
{
    ToolButtonProvider::getToolButtons(buttons, parents);
    if (parents.isEmpty()) {
        btnFastCopy.flags.setFlag(ToolButton::Checked, false);
        if (!buttons.empty())
            buttons.append(&ToolButton::SPLITTER);
        if (res_->canMoveTop())
            buttons.append(&btnTop);
        if (res_->flags() & ResourceView::CanCopy)
            buttons.append(&btnCopy);
        if (res_->flags().testFlag(ResourceView::CanFastCopy))
            buttons.append(&btnFastCopy);
        if (res_->flags() & ResourceView::CanDelete)
            buttons.append(&btnDelete);
        if (buttons.endsWith(&ToolButton::SPLITTER))
            buttons.pop_back();
    }
}

void Control::setOption(QByteArray const & key, QVariant value)
{
    ToolButtonProvider::setOption(key, value);
    res_->setOption(key, value);
}

QVariant Control::getOption(const QByteArray &key)
{
    QVariant p = property(key);
    if (!p.isValid())
        p = res_->property(key);
    return p;
}

StateItem * Control::stateItem()
{
    if (stateItem_)
        return stateItem_;
    stateItem_ = new StateItem(item_);
    stateItem_->showBackground(item_ == realItem_);
    stateItem_->setData(ITEM_KEY_CONTROL, QVariant::fromValue(this));
    ControlTransform * ct = new ControlTransform(static_cast<ControlTransform*>(transform_), true, false, false);
    stateItem_->setTransformations({ct});
    return stateItem_;
}

