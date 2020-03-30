#ifndef FLOATWIDGETMANAGER_H
#define FLOATWIDGETMANAGER_H

#include "ShowBoard_global.h"

#include <QMap>
#include <QObject>

class ToolButton;

class SHOWBOARD_EXPORT FloatWidgetManager : public QObject
{
    Q_OBJECT
public:
    static FloatWidgetManager* from(QWidget * widget);

    static QPoint getPopupPosition(QWidget * widget, ToolButton* attachButton);

    enum Flag {
        PositionAtCenter = 1,
        FullLayout = 2,
        RaiseOnShow = 16,
        RaiseOnFocus = 32,
        HideOnLostFocus = 64,
        HideOthersOnShow = 128,
        DisableActionsOnShow = 256,
    };

    Q_DECLARE_FLAGS(Flags, Flag)

public:
    void setTaskBar(QWidget * bar, int disableActions = 0);

private:
    FloatWidgetManager(QWidget * main);

public:
    // add/show/raise the widget, see @raiseWidget
    void addWidget(QWidget* widget, Flags flags = {RaiseOnFocus});

    // add/show/raise the widget, see @raiseWidget
    void addWidget(QWidget* widget, ToolButton* attachButton,
                   Flags flags = {RaiseOnShow, RaiseOnFocus, HideOnLostFocus});

    // remove/hide the widget
    void removeWidget(QWidget* widget);

    // move z-order after all widgets managed here,
    //  but may not last of all siblings
    void raiseWidget(QWidget* widget);

    void setWidgetFlags(QWidget* widget, Flags flags);

    // save visibility for restore later
    void saveVisibility();

    // call show on widget, you can call it yourself
    void showWidget(QWidget* widget);

    // call hide on widget, you can call it yourself
    void hideWidget(QWidget* widget);

    // hide all widgets managed here
    void hideAll(QWidget* except = nullptr);

    // restore visibility
    void restoreVisibility();

    // save action state for restore later
    void saveActionState();

    // disable taskbar actions
    void disableActions();

    // restore action state
    void restoreActionState();

protected:
    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setWidgetFlags2(QWidget* widget, Flags flags);

    void relayout(QWidget * widget, Flags flags);

    void focusChanged(QWidget* old, QWidget* now);

    QPoint popupPos(QWidget * widget, ToolButton* attachButton);

    void removeDestroyWidget();

private:
    QWidget * main_;
    QWidget * taskBar_;
    QWidget * widgetOn_;
    QList<QWidget*> widgets_;
    QMap<QWidget*, Flags> widgetFlags_;
    QList<int> saveStates_;
    int disableActions_;
    QList<int> saveActionStates_;
};

#endif // FLOATWIDGETMANAGER_H