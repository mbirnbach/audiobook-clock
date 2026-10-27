#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QVariant>

class AbsClient;

struct Chapter
{
    double start = 0;
    double end = 0;
    QString title;
};

struct SeriesRef
{
    QString name;
    QString sequence;
};

struct Book
{
    QString id;
    QString title;
    QStringList authors;
    QString narrator;
    QString description;
    QList<SeriesRef> series;
    double duration = 0;
    int numChapters = 0;
    bool hasCover = false;
    bool detailLoaded = false;
    QList<Chapter> chapters;
    double hue = -1; // fixed palette hue (demo data); -1 = derive from cover

    QString author() const { return authors.join(", "); }
};

struct Progress
{
    double currentTime = 0;
    double duration = 0;
    bool finished = false;
    qint64 lastUpdate = 0;
};

struct Bookmark
{
    double time = 0;
    QString title;
    qint64 createdAt = 0;
};

class BookModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { IdRole = Qt::UserRole + 1, TitleRole, AuthorRole, HasCoverRole };

    explicit BookModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reset(const QList<const Book *> &books);
    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE int indexOf(const QString &id) const;

signals:
    void countChanged();

private:
    QList<const Book *> m_books;
};

// The selected library: books, the user's progress and bookmarks. QML reads details through
// the invokables below and re-evaluates bindings when `revision` changes.
class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(BookModel *books READ books CONSTANT)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString libraryId READ libraryId NOTIFY librariesChanged)
    Q_PROPERTY(QString libraryName READ libraryName NOTIFY librariesChanged)
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY librariesChanged)

public:
    explicit Library(AbsClient *client, QObject *parent = nullptr);

    BookModel *books() { return &m_model; }
    int revision() const { return m_revision; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    QString libraryId() const { return m_libraryId; }
    QString libraryName() const;
    QVariantList libraries() const;

    const Book *find(const QString &id) const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectLibrary(const QString &id);
    Q_INVOKABLE void clear();

    Q_INVOKABLE QVariantMap book(const QString &id) const;
    Q_INVOKABLE QVariantList chapters(const QString &id) const;
    Q_INVOKABLE int chapterAt(const QString &id, double time) const;
    Q_INVOKABLE QVariantMap progress(const QString &id) const;
    // kind: "series" or "authors"
    Q_INVOKABLE QVariantList groups(const QString &kind) const;
    Q_INVOKABLE QVariantList bookmarks(const QString &id) const;
    Q_INVOKABLE void addBookmark(const QString &id, double time);
    Q_INVOKABLE void removeBookmark(const QString &id, double time);
    Q_INVOKABLE void ensureDetail(const QString &id);
    Q_INVOKABLE QString mostRecentId() const;
    Q_INVOKABLE QString letterOf(const QString &text) const;
    // { "A": firstRow, ..., "#": row, "now": 0 } for the cover flow order
    Q_INVOKABLE QVariantMap bookLetters() const;

    // Called by the player so labels follow playback without a server round trip.
    void setLocalProgress(const QString &id, double time, double duration);
    void setChapters(const QString &id, const QList<Chapter> &chapters);

signals:
    void revisionChanged();
    void loadingChanged();
    void errorChanged();
    void librariesChanged();
    void detailLoaded(const QString &id);

private:
    void loadLibraries();
    void loadItems();
    void loadMe();
    void loadDemo();
    void rebuildModel();
    void bump();
    void setLoading(bool l);
    void setError(const QString &e);

    AbsClient *m_client;
    BookModel m_model;
    QList<Book> m_books;
    QHash<QString, int> m_index;
    QHash<QString, Progress> m_progress;
    QHash<QString, QList<Bookmark>> m_bookmarks;
    QList<QPair<QString, QString>> m_libraries; // id, name
    QString m_libraryId;
    QSet<QString> m_detailPending;
    int m_revision = 0;
    int m_pending = 0;
    bool m_loading = false;
    QString m_error;
};
