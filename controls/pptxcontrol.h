#ifndef PPTXCONTROL_H
#define PPTXCONTROL_H

#include "core/control.h"

#include <QUrl>

class ResourceView;
class PowerPoint;

class PptxControl : public Control
{
    Q_OBJECT

    Q_PROPERTY(int slideNumber READ slideNumber WRITE setSlideNumber)

public:
    Q_INVOKABLE PptxControl(ResourceView * res);

    virtual ~PptxControl() override;

public:
    int slideNumber();

    void setSlideNumber(int n);

public slots:
    void open();
    void show(int page = 0); // 0 for current page, 1 for first page
    void next();
    void prev();
    void jump(int page);
    void hide();
    void close();

private:
    void opened(int total);
    void reopened();
    void thumbed(QPixmap pixmap);
    void showed();
    void closed();
    void failed(QString const & msg);

protected:
    virtual QGraphicsItem * create(ResourceView * res) override;

    virtual QString toolsString(QString const & parent) const override;

    virtual void attached() override;

    virtual void detached() override;

private:
    virtual bool eventFilter(QObject *obj, QEvent * event) override;

private:
    void open(QUrl const & url);

    void showStopButton();

private:
    PowerPoint * powerpoint_;
    QWidget * stopButton_;
};

#endif // PPTXCONTROL_H
