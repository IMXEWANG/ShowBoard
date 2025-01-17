#include "qsshelper.h"
#include "toolbarwidget.h"
#include "core/toolbutton.h"
#include "core/toolbuttonprovider.h"
#include "core/controlview.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPushButton>
#include <QGraphicsLayout>
#include <QPainter>
#include <QMetaMethod>
#include <QLabel>

static QssHelper QSS(":/showboard/qss/toolbar.qss");

ToolbarWidget::ToolbarWidget(QWidget *parent)
    : ToolbarWidget(true, parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
}

ToolbarWidget::ToolbarWidget(bool horizontal, QWidget *parent)
    : QFrameEx(parent)
    , template_(nullptr)
    , dragable_(false)
    , requestVisible_(true)
    , popupPosition_(BottomRight)
    , popUp_(nullptr)
    , provider_(nullptr)
{
    if (horizontal)
        layout_ = new QHBoxLayout(this);
    else
        layout_ = new QVBoxLayout(this);
    setObjectName("toolbarwidget");
    setWindowFlag(Qt::FramelessWindowHint);
    setStyleSheet(QSS);
    if (horizontal) {
        int margins = dp(6);
        layout_->setContentsMargins(margins, margins, margins, margins);
        layout_->setSpacing(dp(8));
    }
    setLayout(layout_);
    QFrameEx::setVisible(false);
}

ToolbarWidget::~ToolbarWidget()
{
    clear();
}

void ToolbarWidget::setButtonTemplate(int typeId)
{
    QMetaObject const * meta = QMetaType::metaObjectForType(typeId);
    if (meta && meta->inherits(&QPushButton::staticMetaObject))
        template_ = meta;
}

void ToolbarWidget::setStyleSheet(const QssHelper &style)
{
    QFrameEx::setStyleSheet(style);
    iconSize_ = style.value("QPushButton", "qproperty-iconSize").toSize();
}

void ToolbarWidget::setDragable()
{
    if (dragable_)
        return;
    dragable_ = true;
    QLabel *dragger = new QLabel(this);
    dragger->setObjectName("dragger");
    //dragger->setScaledContents(true);
    layout_->addWidget(dragger);
    QFrame *splitter = new QFrame(this);
    splitter->setObjectName("draggerSplitter");
    splitter->setFrameShape(QFrame::VLine);
    splitter->setFrameShadow(QFrame::Plain);
    splitter->setLineWidth(2);
    layout_->addWidget(splitter);

}

void ToolbarWidget::setPopupPosition(PopupPosition pos)
{
    popupPosition_ = pos;
}

QWidget *ToolbarWidget::createPopup(const QList<ToolButton *> &buttons)
{
    clearPopup();
    QWidget * popup = createPopupWidget();
    QMap<QWidget *, ToolButton *> map;
    for (ToolButton * b : buttons) {
        addToolButton(popup->layout(), b, map);
    }
    for (auto i = map.keyValueBegin(); i != map.keyValueEnd(); ++i) {
        QPushButton* btn = qobject_cast<QPushButton*>((*i).first);
        ToolButton* button = (*i).second;
        if (btn) {
            connect(btn, &QPushButton::clicked, button, [popup, button] (bool checked) {
                popup->hide();
                button->triggered(checked);
            });
            connect(button, &ToolButton::changed, btn, [btn, button]() {
                applyButton(btn, button);
            });
        }
    }
    popup->layout()->activate();
    popup->resize(popup->sizeHint());
    return popup;
}

ControlView *ToolbarWidget::toGraphicsProxy(ControlView * parent, bool centerPos)
{
    ControlView * item = itemFromWidget(this, parent);
    if (centerPos) {
#ifdef SHOWBOARD_QUICK
#else
        QObject::connect(this, &ToolbarWidget::sizeChanged, static_cast<QGraphicsProxyWidget*>(item), [item](){
            QPointF pos = -item->boundingRect().center();
            item->setTransform(QTransform::fromTranslate(pos.x(), pos.y()));
            item->setTransformOriginPoint(-pos);
        });
#endif
    }
    return item;
}

void ToolbarWidget::setToolButtons(QList<ToolButton *> const & buttons)
{
    setButtons(layout_, buttons, buttons_);
    QFrameEx::setVisible(requestVisible_ && !buttons.empty());
}

void ToolbarWidget::updateButton(ToolButton *button)
{
    for (QWidget * w : buttons_.keys()) {
        if (buttons_.value(w) == button) {
            updateButton(qobject_cast<QPushButton*>(w), button);
            return;
        }
    }
}

void ToolbarWidget::updateToolButtons()
{
    for (QWidget * w : buttons_.keys()) {
        updateButton(qobject_cast<QPushButton*>(w), buttons_.value(w));
    }
}

void ToolbarWidget::showPopupButtons(const QList<ToolButton *> &buttons)
{
    setButtons(popUp_->layout(), buttons, popupButtons_);
    popUp_->setVisible(!popupButtons_.isEmpty());
}

static int row = 0;
static int col = 0;

void ToolbarWidget::clearPopup()
{
    row = col = 0;
    if (popUp_) {
        popUp_->hide();
        clearButtons(popUp_->layout(), popupButtons_);
    }
}

void ToolbarWidget::attachProvider(ToolButtonProvider *provider)
{
    if (provider == provider_)
        return;
    if (provider_) {
        QObject::disconnect(provider_, &ToolButtonProvider::buttonsChanged, this, &ToolbarWidget::updateProvider);
    }
    provider_ = provider;
    if (provider_) {
        QObject::connect(provider_, &ToolButtonProvider::buttonsChanged,
                         this, &ToolbarWidget::updateProvider);
        updateProvider();
    } else {
        clear();
        QFrameEx::setVisible(false);
    }
}

void ToolbarWidget::buttonClicked()
{
    QWidget * widget = qobject_cast<QWidget *>(sender());
    buttonClicked(widget);
}

void ToolbarWidget::addToolButton(QLayout* layout, ToolButton * button, QMap<QWidget *, ToolButton *>& buttons)
{
    QWidget * parent = layout->parentWidget();
    QWidget * widget = nullptr;
    if (button == &ToolButton::SPLITTER) {
        QFrame *splitter = new QFrame(parent);
        splitter->setFrameShape(QFrame::VLine); // WARNNING: fix bug of 3906, adding this later when bug fixed
        splitter->setFrameShadow(QFrame::Plain);
        splitter->setLineWidth(2);
        widget = splitter;
    } else if (button == &ToolButton::LINE_BREAK) {
        ++row; col = 0;
        return;
    } else if (button == &ToolButton::LINE_SPLITTER) {
        if (col > 0) ++row; col = -1;
        QWidget * border = new QFrame(parent);
        border->setObjectName("lineBorder");
        QGridLayout * layout2 = new QGridLayout(border);
        QFrame *splitter = new QFrame(border);
        splitter->setFrameShape(QFrame::HLine);
        splitter->setFrameShadow(QFrame::Plain);
        splitter->setLineWidth(0);
        layout2->setContentsMargins(10, 10, 10, 10);
        layout2->addWidget(splitter, 0, 0);
        widget = border;
        widget->show(); // must show, not known why?
    } else if (button == &ToolButton::PLACE_HOOLDER) {
        QGridLayout *gridLayout = qobject_cast<QGridLayout*>(layout);
        if (gridLayout)
            ++col;
        return;
    } else if (button->isCustomWidget()) {
        widget = button->getCustomWidget();
        ToolButton::action_t action([this, widget]() {
            buttonClicked(widget);
        });
        widget->setProperty(ToolButton::ACTION_PROPERTY, QVariant::fromValue(action));
    } else {
        QPushButton * btn = template_
                ? qobject_cast<QPushButton *>(template_->newInstance())
                : new QPushButton;
        btn->setParent(parent);
        btn->addAction(button);
        btn->setIconSize(iconSize_);
        applyButton(btn, button);
        void (ToolbarWidget::*slot)() = &ToolbarWidget::buttonClicked;
        QObject::connect(btn, &QPushButton::clicked, this, slot);
        widget = btn;
    }
    if (!button->isCustomWidget())
        widget->setObjectName(button->name());
    QGridLayout *gridLayout = qobject_cast<QGridLayout*>(layout);
    if (gridLayout) {
        if (col == -1) {
            gridLayout->addWidget(widget, row, 0, 1, -1);
            ++row; col = 0;
        } else {
            if (button->isCustomWidget()) {
                gridLayout->setContentsMargins(0, 0, 0, 0);
            } else {
                gridLayout->setContentsMargins(10, 10, 10, 10);
            }
            gridLayout->addWidget(widget, row, col);
            ++col;
        }
    } else {
        layout->addWidget(widget);
    }
    widget->show(); // let size valid
    if (button) {
        buttons.insert(widget, button);
    }
}

void ToolbarWidget::applyButton(QPushButton * btn, ToolButton *button)
{
    //btn->setIconSize(QSize(40,40));
    btn->setIcon(button->getIcon(btn->iconSize()));
    btn->setText(((button->isPopup()) && !button->text().isEmpty())
                 ? button->text() + " ▼" : button->text());
    if (button->isCheckable()) {
        btn->setCheckable(true);
        btn->setChecked(button->isChecked());
    }
    btn->setEnabled(button->isEnabled());
}

void ToolbarWidget::updateButton(QPushButton * btn, ToolButton *button)
{
    applyButton(btn, button);
    if (button->unionUpdate()) {
        QList<QObject*> list = children();
        int n = list.indexOf(btn);
        for (int i = n - 1; i >= 0; --i) {
            QWidget * widget2 = qobject_cast<QWidget *>(list[i]);
            if (widget2 == nullptr || (button = buttons_.value(widget2)) == nullptr)
                continue;
            if (button->unionUpdate()) {
                QPushButton * btn2 = qobject_cast<QPushButton *>(widget2);
                applyButton(btn2, button);
            } else {
                break;
            }
        }
        for (int i = n + 1; i < list.size(); ++i) {
            QWidget * widget2 = qobject_cast<QWidget *>(list[i]);
            if (widget2 == nullptr || (button = buttons_.value(widget2)) == nullptr)
                continue;
            if (button->unionUpdate()) {
                QPushButton * btn2 = qobject_cast<QPushButton *>(widget2);
                applyButton(btn2, button);
            } else {
                break;
            }
        }
    }
}

void ToolbarWidget::clear()
{
    clearPopup();
    popupParents_.clear();
    clearButtons(layout_, buttons_);
}

void ToolbarWidget::setButtons(QLayout *layout, const QList<ToolButton *> &buttons, QMap<QWidget *, ToolButton *> & map)
{
    clearButtons(layout, map);
    ToolButton * lastButton = nullptr;
    for (ToolButton * b : buttons) {
        if (b == lastButton)
            continue;
        if (!b->isVisible())
            continue;
        if (b == &ToolButton::SPLITTER) {
            if (lastButton == nullptr)
                continue;
            lastButton = b;
            continue;
        }
        if (lastButton == &ToolButton::SPLITTER) {
            addToolButton(layout, &ToolButton::SPLITTER, map);
        }
        addToolButton(layout, b, map);
        lastButton = b;
    }
    QWidget * container = layout->parentWidget();
    layout->activate();
    QGraphicsProxyWidget * proxy = container->graphicsProxyWidget();
    if (proxy) {
        proxy->resize(container->minimumSize());
    }
}

void ToolbarWidget::clearButtons(QLayout *layout, QMap<QWidget *, ToolButton *> &buttons)
{
    for (QWidget * widget : buttons.keys()) {
        layout->removeWidget(widget);
        ToolButton * btn = buttons.value(widget);
        if (btn && btn->isDynamic())
            delete btn;
        if (btn && btn->isCustomWidget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->setProperty(ToolButton::ACTION_PROPERTY, QVariant());
        } else {
            widget->deleteLater();
        }
    }
    buttons.clear();
}

void ToolbarWidget::createPopup()
{
    popUp_ = createPopupWidget();
    QGraphicsProxyWidget * proxy = graphicsProxyWidget();
    if (proxy) {
        proxy = new QGraphicsProxyWidget(proxy);
        proxy->setWidget(popUp_);
        proxy->hide();
    } else {
        popUp_->setParent(parentWidget());
        popUp_->hide();
    }
}

void ToolbarWidget::updateProvider()
{
    QList<ToolButton *> buttons;
    provider_->getToolButtons(buttons);
    setToolButtons(buttons);
}

void ToolbarWidget::buttonClicked(QWidget * widget)
{
    QPushButton* btn = qobject_cast<QPushButton*>(widget);
    ToolButton * button = popupButtons_.value(widget);
    if (!button) {
        button = buttons_.value(widget);
        if (!button) return;
        bool isPopup = !popupButtons_.empty();
        clearPopup();
        if (isPopup && popupParents_.endsWith(button)) {
            popupParents_.pop_back();
            if (btn && button->isCheckable()) {
                btn->setChecked(button->isChecked());
            }
            return;
        }
        popupParents_.clear();
    }
    popupParents_.append(button);
    if (btn && button->isPopup()) {
        if (button->isCheckable()) {
            btn->setChecked(button->isChecked());
        }
        QList<ToolButton *> buttons;
        getPopupButtons(buttons, popupParents_);
        if (popUp_ == nullptr) {
            createPopup();
        }
        showPopupButtons(buttons);
        QGraphicsProxyWidget * proxy = graphicsProxyWidget();
        if (proxy) {
            QGraphicsProxyWidget * proxy2 = popUp_->graphicsProxyWidget();
            proxy2->setPos(popupPosition(btn));
        } else {
            QPoint pos = popupPosition(btn).toPoint();
            popUp_->move(pos);
        }
    } else {
        if (btn) {
            for (QAction* a : btn->actions())
                a->triggered(btn->isChecked());
        }
        onButtonClicked(button);
        if (buttons_.empty()) // may delete & clear
            return;
        popupParents_.pop_back();
        for (int i = 0; i < popupParents_.size(); ++i) {
            if (popupParents_[i]->isOptionsGroup()) {
                button = popupParents_[i];
                break;
            }
        }
        if (button->needUpdate()) {
            for (QWidget * w : buttons_.keys()) {
                if (buttons_.value(w) == button) {
                    updateButton(qobject_cast<QPushButton*>(w), button);
                    layout_->activate();
                    if (graphicsProxyWidget()) {
                        graphicsProxyWidget()->resize(minimumSize());
                    }
                    break;
                }
            }
        }
        if (!popupButtons_.empty()) {
            clearPopup();
            popupParents_.pop_back();
        }
    }
}

QPointF ToolbarWidget::popupPosition(QPushButton *btn)
{
    QPointF topLeft = btn->mapTo(this, QPoint(0, 0));
    QSizeF size = popUp_->size();
    constexpr qreal PADDING = 15;
    switch (popupPosition_) {
    case TopLeft:
        topLeft += QPointF(-size.width() + btn->width(), -size.height() - PADDING);
        break;
    case TopCenter:
        topLeft += QPointF(-(size.width() - btn->width()) / 2, -size.height() - PADDING);
        break;
    case TopRight:
        topLeft += QPointF(0, -size.height() - PADDING);
        break;
    case BottomLeft:
        topLeft += QPointF(-size.width() + btn->width(), btn->height() + PADDING);
        break;
    case BottomCenter:
        topLeft += QPointF(-(size.width() - btn->width()) / 2, btn->height() + PADDING);
        break;
    case BottomRight:
        topLeft += QPointF(0, btn->height() + PADDING);
        break;
    case LeftTop:
        topLeft += QPointF(-size.width() - PADDING, -size.height() + btn->height());
        break;
    case LeftCenter:
        topLeft += QPointF(-size.width() - PADDING, (-size.height() + btn->height()) / 2);
        break;
    case LeftBottom:
        topLeft += QPointF(-size.width() - PADDING, 0);
        break;
    case RightTop:
        topLeft += QPointF(btn->width() + PADDING, -size.height() + btn->height());
        break;
    case RightCenter:
        topLeft += QPointF(btn->width() + PADDING, (-size.height() + btn->height()) / 2);
        break;
    case RightBottom:
        topLeft += QPointF(btn->width() + PADDING, 0);
        break;
    }
    QGraphicsItem* parent = graphicsProxyWidget();
    QRectF bound = parent ? parent->mapRectFromScene(parent->scene()->sceneRect())
                          : QRectF{mapFrom(window(),QPoint()), window()->size()};
    QRectF bound2(topLeft, size);
    if (bound2.left() < bound.left())
        topLeft.setX(bound.left());
    else if (bound2.right() > bound.right())
        topLeft.setX(bound.left() + bound.right() - bound2.right());
    if (bound2.top() < bound.top())
        topLeft.setY(topLeft.y() + size.height() + btn->height() + 30);
    else if (bound2.bottom() > bound.bottom())
        topLeft.setY(topLeft.y() - size.height() - btn->height() - 30);
    if (parent) {
        return parent->mapToItem(popUp_->graphicsProxyWidget()->parentItem(), topLeft);
    } else {
        return mapToGlobal(topLeft.toPoint());
    }
}

void ToolbarWidget::getPopupButtons(QList<ToolButton *> &buttons, const QList<ToolButton *> &parents)
{
    if (provider_)
        provider_->getToolButtons(buttons, parents);
    else
        emit popupButtonsRequired(buttons, popupParents_);
}

void ToolbarWidget::onButtonClicked(ToolButton *)
{
    if (provider_)
        provider_->handleToolButton(popupParents_);
    else
        emit buttonClicked(popupParents_);
}

QWidget *ToolbarWidget::createPopupWidget()
{
    QFrameEx * widget = new QFrameEx();
    widget->setWindowFlags(Qt::FramelessWindowHint);
    widget->setObjectName("popupwidget");
    widget->setStyleSheet(QSS);
    widget->setLayout(new QGridLayout());
    return widget;
}

void ToolbarWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    emit sizeChanged(event->size());
}

bool ToolbarWidget::event(QEvent *event)
{
    if (event->type() == QEvent::TouchBegin) {
        event->accept();
        return true;
    }
    return QFrameEx::event(event);
}

void ToolbarWidget::setVisible(bool visible)
{
    requestVisible_ = visible;
    visible &= !buttons_.empty();
    QFrame::setVisible(visible);
    if (!visible && popUp_) {
        QGraphicsProxyWidget * proxy = popUp_->graphicsProxyWidget();
        if (proxy)
            proxy->hide();
        else
            popUp_->hide();
    }
}

static QPointF lastPosition(-1, -1);

void ToolbarWidget::mousePressEvent(QMouseEvent *event)
{
    if (dragable_)
        lastPosition = event->screenPos();
}

void ToolbarWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (dragable_ && lastPosition.x() >= 0) {
        QPointF d = event->screenPos() - lastPosition;
        lastPosition = event->screenPos();
        if (graphicsProxyWidget())
            graphicsProxyWidget()->moveBy(d.x(), d.y());
        else
            move(d.toPoint());
    }
}

void ToolbarWidget::mouseReleaseEvent(QMouseEvent *event)
{
    (void)event;
    lastPosition = QPointF(-1, -1);
}

QFrameEx::QFrameEx(QWidget *parent)
    : QFrame(parent)
{
}

void QFrameEx::setStyleSheet(const QssHelper &style)
{
    QFrame::setStyleSheet(style);
    QByteArray objname = "#" + objectName().toUtf8();
    borderRadius_ = style.value(objname, "border-radius").toInt();
    borderPen_ = style.value(objname, "border").toPen();
}

void QFrameEx::paintEvent(QPaintEvent * event)
{
    if (graphicsProxyWidget()) {
        QRect r(1, 1, width() - 2, height() - 2);
        QPainter p(this);
        p.setRenderHints(QPainter::HighQualityAntialiasing);
        p.setPen(borderPen_);
        p.setBrush(palette().brush(QPalette::Window));
        p.drawRoundedRect(r, borderRadius_, borderRadius_);
    }
    QFrame::paintEvent(event);
}

void QFrameEx::connectNotify(const QMetaMethod &signal)
{
    if (signal ==  QMetaMethod::fromSignal(&QObject::destroyed)) {
        if (graphicsProxyWidget())
            setAttribute(Qt::WA_TranslucentBackground);
    }
}
