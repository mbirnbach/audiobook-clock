#include "DemoData.h"

#include <QDateTime>
#include <cmath>

namespace {

struct Raw
{
    const char *id, *title, *author, *narrator, *series;
    int seq;
    double duration;
    double hue;
    const char *description;
    QStringList chapterTitles;
    int chapterCount;
};

const QList<Raw> &raw()
{
    static const QList<Raw> data = {
        {"hound", "The Hound of the Baskervilles", "Arthur Conan Doyle", "Thomas Hale", "Sherlock Holmes", 3, 22920, 150,
         "A family curse, a spectral hound and the fog of Dartmoor. Holmes sends Watson ahead to Baskerville Hall and "
         "works the case from the shadows.",
         {"Mr. Sherlock Holmes", "The Curse of the Baskervilles", "The Problem", "Sir Henry Baskerville",
          "Three Broken Threads", "Baskerville Hall", "The Stapletons of Merripit House", "First Report of Dr. Watson",
          "The Light upon the Moor", "Extract from the Diary of Dr. Watson", "The Man on the Tor", "Death on the Moor",
          "Fixing the Nets", "The Hound of the Baskervilles", "A Retrospection"},
         0},
        {"twenty", "Twenty Thousand Leagues Under the Sea", "Jules Verne", "Claire Ashdown", "Voyages Extraordinaires", 6,
         44400, 225, "Professor Aronnax is taken aboard the Nautilus and travels the oceans with the enigmatic Captain Nemo.",
         {}, 47},
        {"monte", "The Count of Monte Cristo", "Alexandre Dumas", "Bernard Ives", nullptr, 0, 190800, 25,
         "Betrayed on his wedding day and imprisoned for fourteen years, Edmond Dantès escapes with a fortune and a plan.",
         {}, 117},
        {"dracula", "Dracula", "Bram Stoker", "Mara Quinn", nullptr, 0, 55800, 15,
         "Told through letters and diaries, a solicitor’s trip to Transylvania sets a slow, cold dread in motion.", {}, 27},
        {"study", "A Study in Scarlet", "Arthur Conan Doyle", "Thomas Hale", "Sherlock Holmes", 1, 16800, 40,
         "Watson meets Holmes for the first time and is drawn into a murder in Brixton.", {}, 14},
        {"sign", "The Sign of Four", "Arthur Conan Doyle", "Thomas Hale", "Sherlock Holmes", 2, 16200, 70,
         "A yearly pearl, a missing father and a pact sworn in India bring Mary Morstan to Baker Street.", {}, 12},
        {"eighty", "Around the World in Eighty Days", "Jules Verne", "Claire Ashdown", "Voyages Extraordinaires", 11, 25200,
         85, "Phileas Fogg wagers half his fortune that he can circle the globe in eighty days.", {}, 37},
        {"journey", "Journey to the Center of the Earth", "Jules Verne", "Claire Ashdown", "Voyages Extraordinaires", 3,
         29400, 50, "A coded runic note leads Professor Lidenbrock and his nephew down an Icelandic volcano.", {}, 45},
        {"frank", "Frankenstein", "Mary Shelley", "Ruth Calder", nullptr, 0, 30600, 280,
         "Victor Frankenstein builds a living being and spends the rest of his life fleeing it.", {}, 24},
        {"time", "The Time Machine", "H. G. Wells", "Owen Marsh", nullptr, 0, 12000, 195,
         "A Victorian inventor travels to the year 802,701 and finds humanity split in two.", {}, 12},
        {"treasure", "Treasure Island", "Robert Louis Stevenson", "Owen Marsh", nullptr, 0, 25200, 100,
         "Young Jim Hawkins finds a map in a dead pirate’s sea chest and sails for Skeleton Island.", {}, 34},
        {"moby", "Moby-Dick", "Herman Melville", "Bernard Ives", nullptr, 0, 86400, 250,
         "Ishmael ships aboard the Pequod under Captain Ahab, who is hunting one particular white whale.", {}, 135},
    };
    return data;
}

} // namespace

void DemoData::fill(QList<Book> &books, QHash<QString, Progress> &progress, QHash<QString, QList<Bookmark>> &bookmarks)
{
    books.clear();
    for (const Raw &r : raw()) {
        Book b;
        b.id = r.id;
        b.title = QString::fromUtf8(r.title);
        b.authors = {QString::fromUtf8(r.author)};
        b.narrator = r.narrator;
        if (r.series)
            b.series = {{r.series, QString::number(r.seq)}};
        b.duration = r.duration;
        b.description = QString::fromUtf8(r.description);
        b.hue = r.hue;
        // Uneven chapter lengths, same formula as the mockup.
        const int n = r.chapterTitles.isEmpty() ? r.chapterCount : int(r.chapterTitles.size());
        QList<double> w;
        double sum = 0;
        for (int i = 0; i < n; ++i) {
            w.append(1 + 0.45 * std::sin(i * 2.7 + r.hue));
            sum += w.last();
        }
        double t = 0;
        for (int i = 0; i < n; ++i) {
            const double d = i == n - 1 ? r.duration - t : std::round(w.at(i) / sum * r.duration);
            b.chapters.append({t, t + d, r.chapterTitles.isEmpty() ? QStringLiteral("Chapter %1").arg(i + 1)
                                                                    : r.chapterTitles.at(i)});
            t += d;
        }
        b.numChapters = n;
        b.detailLoaded = true;
        books.append(b);
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    progress.clear();
    progress.insert("hound", {22920 * 0.38, 22920, false, now});
    progress.insert("twenty", {44400 * 0.64, 44400, false, now - 86400000});
    progress.insert("monte", {190800 * 0.12, 190800, false, now - 2 * 86400000});
    progress.insert("dracula", {55800 * 0.05, 55800, false, now - 3 * 86400000});
    progress.insert("study", {16800, 16800, true, now - 9 * 86400000});

    bookmarks.clear();
    bookmarks.insert("hound", {{5520, "The Problem", now - 86400000 + 3600000},
                               {7310, "Sir Henry Baskerville", now - 3 * 86400000}});
}
