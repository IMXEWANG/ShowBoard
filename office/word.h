#ifndef WORD_H
#define WORD_H

#include <QObject>
#include <QPixmap>

class QAxObject;

class Word : public QObject
{
    Q_OBJECT
public:
    Word(QObject * parent = nullptr);

    virtual ~Word() override;

public:
    void open(QString const & file);

    void thumb(int page);

    void show(int page);

    void hide();

    void jump(int page);

    void prev();

    void next();

    void close();

public:
    int page()
    {
        return page_;
    }

    void setPage(int n)
    {
        page_ = n;
    }

signals:
    void opened(int total);

    void reopened();

    void failed(QString const & msg);

    void showed();

    void thumbed(QPixmap pixmap);

    void closed();

private slots:
    void onPropertyChanged(const QString &name);

    void onSignal(const QString &name, int argc, void *argv);

    void onException(int code, const QString &source, const QString &desc, const QString &help);

private:
    void reopen();

private:
    static QAxObject * application_;

private:
    QAxObject * documents_;
    QString file_;
    QAxObject * document_;
    int total_;
    int page_;
    QAxObject * view_;
};

#endif // WORD_H