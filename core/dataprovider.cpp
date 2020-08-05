#include "dataprovider.h"

#include <qexport.h>

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace QtPromise;

REGISTER_DATA_RPOVIDER(DataDataProvider,"data")
REGISTER_DATA_RPOVIDER(FileDataProvider,"file,qrc,")
REGISTER_DATA_RPOVIDER(HttpDataProvider,"http,https")

DataProvider::DataProvider(QObject *parent)
    : QObject(parent)
{
}

/* DataDataProvider */

DataDataProvider::DataDataProvider(QObject *parent)
    : DataProvider(parent)
{
}

QtPromise::QPromise<QSharedPointer<QIODevice> > DataDataProvider::getStream(const QUrl &url, bool all)
{
    (void) url;
    (void) all;
    return QPromise<QSharedPointer<QIODevice>>::resolve(nullptr);
}

/* FileDataProvider */

FileDataProvider::FileDataProvider(QObject *parent)
    : DataProvider(parent)
{
}

QtPromise::QPromise<QSharedPointer<QIODevice> > FileDataProvider::getStream(const QUrl &url, bool all)
{
    (void) all;
    QString path = url.scheme() == "qrc" ? ":" + url.path() : url.toLocalFile();
    QSharedPointer<QIODevice> file(new QFile(path));
    if (file->open(QFile::ReadOnly | QFile::ExistingOnly)) {
        return QPromise<QSharedPointer<QIODevice>>::resolve(file);
    } else {
        qDebug() << "Resource file error" << file->errorString();
        return QPromise<QSharedPointer<QIODevice>>::reject(std::invalid_argument("打开失败，请重试"));
    }
}

/* HttpDataProvider */

HttpDataProvider::HttpDataProvider(QObject *parent)
    : DataProvider(parent)
{
    network_ = new QNetworkAccessManager();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QtPromise::QPromise<QSharedPointer<QIODevice>> HttpDataProvider::getStream(const QUrl &url, bool all)
{
    QNetworkRequest request(url);
    QSharedPointer<HttpStream> reply(new HttpStream(network_->get(request)));
    return QPromise<QSharedPointer<QIODevice>>([reply, all](
                                     const QPromiseResolve<QSharedPointer<QIODevice>>& resolve,
                                     const QPromiseReject<QSharedPointer<QIODevice>>& reject) {

        auto error = [reply, reject](QNetworkReply::NetworkError e) {
            qDebug() << "Resource NetworkError " << e << reply->errorString();
            reject(std::invalid_argument("network|打开失败，请检查网络再试"));
        };
        if (all) {
            auto finished = [reply, resolve, error]() {
                if (reply->reply()->error()) {
                    error(reply->reply()->error());
                } else {
                    resolve(reply);
                }
            };
            QObject::connect(reply.get(), &HttpStream::finished, finished);
        } else {
            auto readyRead = [reply, resolve]() {
                resolve(reply);
            };
            QObject::connect(reply.get(), &HttpStream::readyRead, readyRead);
        }
        QObject::connect(reply.get(), &HttpStream::error, error);
    });
}

HttpStream::HttpStream(QNetworkReply *reply)
    : reply_(reply)
{
    open(ReadOnly);
    reopen();
}

HttpStream::~HttpStream()
{
    delete reply_;
}

void HttpStream::onError(QNetworkReply::NetworkError e)
{
    if (e <= QNetworkReply::UnknownNetworkError
            || e >= QNetworkReply::ProtocolUnknownError
            /*|| e == QNetworkReply::OperationCanceledError*/) {
        data_.append(reply_->readAll());
        qint64 size = pos() + data_.size();
        QNetworkRequest request = reply_->request();
        qDebug() << "HttpStream retry" << size;
        if (size > 0)
            request.setRawHeader("Range", "bytes=" + QByteArray::number(size) + "-");
        QNetworkReply * reply = reply_->manager()->get(request);
        std::swap(reply, reply_);
        //delete reply;
        reopen();
        return;
    }
    emit error(e);
}

void HttpStream::onFinished()
{
    if (sender() == reply_) {
        emit readChannelFinished();
        emit finished();
    } else {
        sender()->deleteLater();
    }
}

void HttpStream::reopen()
{
    QObject::connect(reply_, &QNetworkReply::finished, this, &HttpStream::onFinished);
    QObject::connect(reply_, &QNetworkReply::readyRead, this, &HttpStream::readyRead);
    void (QNetworkReply::*p)(QNetworkReply::NetworkError) = &QNetworkReply::error;
    QObject::connect(reply_, p, this, &HttpStream::onError);
    //pos_ = 0;
}

qint64 HttpStream::readData(char *data, qint64 maxlen)
{
    if (!data_.isEmpty()) {
        if (maxlen > data_.size())
            maxlen = data_.size();
        memcpy(data, data_, static_cast<size_t>(maxlen));
        data_.remove(0, static_cast<int>(maxlen));
        return maxlen;
    }
    if (!reply_->isOpen())
        return 0;
    qint64 result = reply_->read(data, maxlen);
    /*
    if (result) {
        if (pos_ == 0) {
            QByteArray msg;
            for (auto h : reply_->rawHeaderPairs())
                msg.append(h.first + ':' + h.second + '\n');
            qDebug() << msg;
        }
        pos_ += result;
        if (pos_ > 10000000) {
            QTimer::singleShot(0, reply_, &QNetworkReply::abort);
        }
    }
    */
    return result;
}

qint64 HttpStream::writeData(const char *, qint64)
{
    assert(false);
    return 0;
}
