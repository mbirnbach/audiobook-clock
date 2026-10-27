#include "Library.h"

#include "AbsClient.h"
#include "DemoData.h"

#include <QCollator>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocumentFragment>
#include <algorithm>

// ---------------------------------------------------------------- BookModel

BookModel::BookModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BookModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_books.size());
}

QVariant BookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_books.size())
        return {};
    const Book *b = m_books.at(index.row());
    switch (role) {
    case IdRole:
        return b->id;
    case TitleRole:
        return b->title;
    case AuthorRole:
        return b->author();
    case HasCoverRole:
        return b->hasCover;
    }
    return {};
}

QHash<int, QByteArray> BookModel::roleNames() const
{
    return {{IdRole, "bookId"}, {TitleRole, "title"}, {AuthorRole, "author"}, {HasCoverRole, "hasCover"}};
}

void BookModel::reset(const QList<const Book *> &books)
{
    const bool countChange = books.size() != m_books.size();
    beginResetModel();
    m_books = books;
    endResetModel();
    if (countChange)
        emit countChanged();
}

QString BookModel::idAt(int row) const
{
    return row >= 0 && row < m_books.size() ? m_books.at(row)->id : QString();
}

int BookModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_books.size(); ++i)
        if (m_books.at(i)->id == id)
            return i;
    return -1;
}

// ---------------------------------------------------------------- parsing helpers

static QList<SeriesRef> parseSeriesName(const QString &seriesName)
{
    // Minified items carry "Series A #1, Series B #2".
    QList<SeriesRef> out;
    static const QRegularExpression re(R"(^(.*?)(?:\s+#([\d.]+))?$)");
    for (const QString &part : seriesName.split(", ", Qt::SkipEmptyParts)) {
        const auto m = re.match(part.trimmed());
        out.append({m.captured(1), m.captured(2)});
    }
    return out;
}

static QString plainText(const QString &html)
{
    if (!html.contains('<'))
        return html;
    return QTextDocumentFragment::fromHtml(html).toPlainText().simplified();
}

static QList<Chapter> parseChapters(const QJsonArray &arr)
{
    QList<Chapter> out;
    for (const auto &v : arr) {
        const QJsonObject c = v.toObject();
        out.append({c.value("start").toDouble(), c.value("end").toDouble(), c.value("title").toString()});
    }
    return out;
}

static QString sortKey(const QString &title)
{
    static const QRegularExpression article("^(the|a|an|der|die|das)\\s+", QRegularExpression::CaseInsensitiveOption);
    return QString(title).remove(article);
}

static bool inProgress(const Progress &p)
{
    return p.currentTime > 0 && !p.finished;
}

// ---------------------------------------------------------------- Library

Library::Library(AbsClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    m_libraryId = QSettings().value("library/id").toString();
}

QString Library::libraryName() const
{
    for (const auto &l : m_libraries)
        if (l.first == m_libraryId)
            return l.second;
    return {};
}

QVariantList Library::libraries() const
{
    QVariantList out;
    for (const auto &l : m_libraries)
        out.append(QVariantMap{{"id", l.first}, {"name", l.second}});
    return out;
}

const Book *Library::find(const QString &id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.cend() ? nullptr : &m_books.at(*it);
}

void Library::clear()
{
    m_books.clear();
    m_index.clear();
    m_progress.clear();
    m_bookmarks.clear();
    m_libraries.clear();
    m_model.reset({});
    emit librariesChanged();
    bump();
}

void Library::refresh()
{
    if (m_client->demo()) {
        loadDemo();
        return;
    }
    setError({});
    loadLibraries();
}

void Library::selectLibrary(const QString &id)
{
    if (id == m_libraryId)
        return;
    m_libraryId = id;
    QSettings().setValue("library/id", id);
    emit librariesChanged();
    if (!m_client->demo())
        loadItems();
}

void Library::loadDemo()
{
    m_libraries = {{"demo", "Audiobooks"}, {"kids", "Kids"}};
    if (m_libraryId.isEmpty() || m_libraryId == "kids")
        m_libraryId = "demo";
    DemoData::fill(m_books, m_progress, m_bookmarks);
    rebuildModel();
    emit librariesChanged();
}

void Library::loadLibraries()
{
    setLoading(true);
    m_client->get("/api/libraries", [this](const QJsonDocument &json, int, const QString &err) {
        if (json.isNull()) {
            setLoading(false);
            setError(err);
            return;
        }
        m_libraries.clear();
        for (const auto &v : json.object().value("libraries").toArray()) {
            const QJsonObject l = v.toObject();
            if (l.value("mediaType").toString() == "book")
                m_libraries.append({l.value("id").toString(), l.value("name").toString()});
        }
        const auto has = [this](const QString &id) {
            return std::any_of(m_libraries.cbegin(), m_libraries.cend(), [&](auto &l) { return l.first == id; });
        };
        if (!has(m_libraryId))
            m_libraryId = has(m_client->defaultLibraryId()) ? m_client->defaultLibraryId()
                          : m_libraries.isEmpty()             ? QString()
                                                              : m_libraries.first().first;
        emit librariesChanged();
        if (m_libraryId.isEmpty()) {
            setLoading(false);
            setError(tr("This account has no audiobook library."));
            return;
        }
        loadItems();
    });
}

void Library::loadItems()
{
    setLoading(true);
    m_pending = 2;
    const QString libId = m_libraryId;
    m_client->get("/api/libraries/" + libId + "/items?minified=1&limit=0",
                  [this, libId](const QJsonDocument &json, int, const QString &err) {
        if (libId != m_libraryId)
            return;
        if (json.isNull()) {
            setError(err);
        } else {
            QHash<QString, Book> old;
            for (const Book &b : std::as_const(m_books))
                old.insert(b.id, b);
            m_books.clear();
            for (const auto &v : json.object().value("results").toArray()) {
                const QJsonObject item = v.toObject();
                const QJsonObject media = item.value("media").toObject();
                const QJsonObject md = media.value("metadata").toObject();
                Book b;
                b.id = item.value("id").toString();
                b.title = md.value("title").toString();
                b.authors = md.value("authorName").toString().split(", ", Qt::SkipEmptyParts);
                b.narrator = md.value("narratorName").toString();
                b.series = parseSeriesName(md.value("seriesName").toString());
                b.description = plainText(md.value("description").toString());
                b.duration = media.value("duration").toDouble();
                b.numChapters = media.value("numChapters").toInt();
                b.hasCover = !media.value("coverPath").toString().isEmpty();
                // Keep chapter details we already fetched.
                const auto it = old.constFind(b.id);
                if (it != old.cend() && it->detailLoaded) {
                    b.chapters = it->chapters;
                    b.detailLoaded = true;
                    if (b.description.isEmpty())
                        b.description = it->description;
                }
                m_books.append(b);
            }
        }
        if (--m_pending == 0)
            rebuildModel();
    });
    loadMe();
}

void Library::loadMe()
{
    m_client->get("/api/me", [this](const QJsonDocument &json, int, const QString &) {
        if (!json.isNull()) {
            const QJsonObject me = json.object();
            m_progress.clear();
            for (const auto &v : me.value("mediaProgress").toArray()) {
                const QJsonObject p = v.toObject();
                if (!p.value("episodeId").toString().isEmpty())
                    continue;
                m_progress.insert(p.value("libraryItemId").toString(),
                                  {p.value("currentTime").toDouble(), p.value("duration").toDouble(),
                                   p.value("isFinished").toBool(), qint64(p.value("lastUpdate").toDouble())});
            }
            m_bookmarks.clear();
            for (const auto &v : me.value("bookmarks").toArray()) {
                const QJsonObject b = v.toObject();
                m_bookmarks[b.value("libraryItemId").toString()].append(
                    {b.value("time").toDouble(), b.value("title").toString(), qint64(b.value("createdAt").toDouble())});
            }
        }
        if (m_pending > 0 && --m_pending == 0)
            rebuildModel();
        else if (m_pending == 0)
            bump();
    });
}

void Library::rebuildModel()
{
    // Books in progress first (most recently listened), then everything else by title.
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(m_books.begin(), m_books.end(), [&](const Book &a, const Book &b) {
        const Progress pa = m_progress.value(a.id), pb = m_progress.value(b.id);
        const bool ia = inProgress(pa), ib = inProgress(pb);
        if (ia != ib)
            return ia;
        if (ia)
            return pa.lastUpdate > pb.lastUpdate;
        return coll.compare(sortKey(a.title), sortKey(b.title)) < 0;
    });
    m_index.clear();
    QList<const Book *> ptrs;
    for (int i = 0; i < m_books.size(); ++i) {
        m_index.insert(m_books.at(i).id, i);
        ptrs.append(&m_books.at(i));
    }
    m_model.reset(ptrs);
    setLoading(false);
    bump();
}

QString Library::letterOf(const QString &text) const
{
    // First letter without accents (Ä -> A); digits and symbols go under "#".
    const QString key = sortKey(text).normalized(QString::NormalizationForm_D);
    for (const QChar c : key) {
        if (c.isLetter())
            return c.toUpper();
        if (!c.isMark() && !c.isSpace())
            return QStringLiteral("#");
    }
    return QStringLiteral("#");
}

QVariantMap Library::bookLetters() const
{
    // Row of the first book per letter in the alphabetical part of the cover flow (books in
    // progress come first and are reached with "now" = row 0).
    QVariantMap out;
    bool anyInProgress = false;
    for (int i = 0; i < m_books.size(); ++i) {
        if (inProgress(m_progress.value(m_books.at(i).id))) {
            anyInProgress = true;
            continue;
        }
        const QString l = letterOf(m_books.at(i).title);
        if (!out.contains(l))
            out.insert(l, i);
    }
    if (anyInProgress)
        out.insert(QStringLiteral("now"), 0);
    return out;
}

QString Library::mostRecentId() const
{
    QString best;
    qint64 bestTime = -1;
    for (auto it = m_progress.cbegin(); it != m_progress.cend(); ++it) {
        if (!it->finished && it->currentTime > 0 && it->lastUpdate > bestTime && m_index.contains(it.key())) {
            best = it.key();
            bestTime = it->lastUpdate;
        }
    }
    return best;
}

QVariantMap Library::book(const QString &id) const
{
    const Book *b = find(id);
    if (!b)
        return {};
    QString seriesLabel;
    if (!b->series.isEmpty()) {
        seriesLabel = b->series.first().name;
        if (!b->series.first().sequence.isEmpty())
            seriesLabel += tr(" · Book %1").arg(b->series.first().sequence);
    }
    return {{"id", b->id},
            {"title", b->title},
            {"author", b->author()},
            {"narrator", b->narrator},
            {"description", b->description},
            {"seriesLabel", seriesLabel},
            {"duration", b->duration},
            {"numChapters", b->detailLoaded ? int(b->chapters.size()) : b->numChapters},
            {"hasCover", b->hasCover},
            {"detailLoaded", b->detailLoaded}};
}

QVariantList Library::chapters(const QString &id) const
{
    QVariantList out;
    const Book *b = find(id);
    if (!b)
        return out;
    for (int i = 0; i < b->chapters.size(); ++i) {
        const Chapter &c = b->chapters.at(i);
        out.append(QVariantMap{{"index", i}, {"title", c.title}, {"start", c.start}, {"duration", c.end - c.start}});
    }
    return out;
}

int Library::chapterAt(const QString &id, double time) const
{
    const Book *b = find(id);
    if (!b)
        return -1;
    int k = 0;
    for (int i = 0; i < b->chapters.size(); ++i)
        if (b->chapters.at(i).start <= time)
            k = i;
    return b->chapters.isEmpty() ? -1 : k;
}

QVariantMap Library::progress(const QString &id) const
{
    const Book *b = find(id);
    const Progress p = m_progress.value(id);
    const double dur = b && b->duration > 0 ? b->duration : p.duration;
    return {{"time", p.currentTime}, {"duration", dur}, {"finished", p.finished}};
}

QVariantList Library::groups(const QString &kind) const
{
    struct Entry { const Book *book; QString seq; };
    QMap<QString, QList<Entry>> map;
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    for (const Book &b : m_books) {
        if (kind == "series") {
            for (const SeriesRef &s : b.series)
                map[s.name].append(Entry{&b, s.sequence});
        } else {
            for (const QString &a : b.authors)
                map[a].append(Entry{&b, QString()});
        }
    }
    QStringList names = map.keys();
    std::sort(names.begin(), names.end(), [&](const QString &a, const QString &b) { return coll.compare(a, b) < 0; });
    QVariantList out;
    for (const QString &name : names) {
        QList<Entry> entries = map.value(name);
        if (kind == "series")
            std::stable_sort(entries.begin(), entries.end(),
                             [](const Entry &a, const Entry &b) { return a.seq.toDouble() < b.seq.toDouble(); });
        QVariantList books;
        for (const Entry &e : entries)
            books.append(QVariantMap{{"id", e.book->id}, {"title", e.book->title}, {"hasCover", e.book->hasCover},
                                     {"author", e.book->author()}, {"sequence", e.seq}, {"duration", e.book->duration}});
        out.append(QVariantMap{{"name", name}, {"count", int(entries.size())}, {"books", books}});
    }
    return out;
}

QVariantList Library::bookmarks(const QString &id) const
{
    QList<Bookmark> list = m_bookmarks.value(id);
    std::sort(list.begin(), list.end(), [](const Bookmark &a, const Bookmark &b) { return a.time < b.time; });
    QVariantList out;
    for (const Bookmark &m : list)
        out.append(QVariantMap{{"time", m.time}, {"title", m.title}, {"createdAt", m.createdAt}});
    return out;
}

void Library::addBookmark(const QString &id, double time)
{
    time = std::floor(time);
    const int ci = chapterAt(id, time);
    const Book *b = find(id);
    const QString title = b && ci >= 0 ? b->chapters.at(ci).title : tr("Bookmark");
    auto &list = m_bookmarks[id];
    for (const Bookmark &m : std::as_const(list))
        if (m.time == time)
            return;
    list.append(Bookmark{time, title, QDateTime::currentMSecsSinceEpoch()});
    bump();
    m_client->post("/api/me/item/" + id + "/bookmark", {{"time", time}, {"title", title}}, nullptr);
}

void Library::removeBookmark(const QString &id, double time)
{
    auto &list = m_bookmarks[id];
    list.removeIf([time](const Bookmark &m) { return m.time == time; });
    bump();
    m_client->del("/api/me/item/" + id + "/bookmark/" + QString::number(time, 'f', 0), nullptr);
}

void Library::ensureDetail(const QString &id)
{
    const Book *b = find(id);
    if (!b || b->detailLoaded || m_detailPending.contains(id) || m_client->demo())
        return;
    m_detailPending.insert(id);
    m_client->get("/api/items/" + id + "?expanded=1", [this, id](const QJsonDocument &json, int, const QString &) {
        m_detailPending.remove(id);
        const auto it = m_index.constFind(id);
        if (json.isNull() || it == m_index.cend())
            return;
        Book &b = m_books[*it];
        const QJsonObject media = json.object().value("media").toObject();
        const QJsonObject md = media.value("metadata").toObject();
        b.chapters = parseChapters(media.value("chapters").toArray());
        b.detailLoaded = true;
        const QString desc = plainText(md.value("description").toString());
        if (!desc.isEmpty())
            b.description = desc;
        QStringList narrators;
        for (const auto &n : md.value("narrators").toArray())
            narrators.append(n.toString());
        if (!narrators.isEmpty())
            b.narrator = narrators.join(", ");
        if (media.value("duration").toDouble() > 0)
            b.duration = media.value("duration").toDouble();
        bump();
        emit detailLoaded(id);
    });
}

void Library::setLocalProgress(const QString &id, double time, double duration)
{
    Progress &p = m_progress[id];
    p.currentTime = time;
    if (duration > 0)
        p.duration = duration;
    p.finished = duration > 0 && time >= duration - 1;
    p.lastUpdate = QDateTime::currentMSecsSinceEpoch();
    bump();
}

void Library::setChapters(const QString &id, const QList<Chapter> &chapters)
{
    const auto it = m_index.constFind(id);
    if (it == m_index.cend() || chapters.isEmpty())
        return;
    Book &b = m_books[*it];
    if (b.detailLoaded && b.chapters.size() == chapters.size())
        return;
    b.chapters = chapters;
    b.detailLoaded = true;
    bump();
}

void Library::bump()
{
    ++m_revision;
    emit revisionChanged();
}

void Library::setLoading(bool l)
{
    if (m_loading == l)
        return;
    m_loading = l;
    emit loadingChanged();
}

void Library::setError(const QString &e)
{
    if (m_error == e)
        return;
    m_error = e;
    emit errorChanged();
}
