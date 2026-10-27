#pragma once

#include "Library.h"

// Offline sample library from the mockup, for running the UI without a server (--demo).
namespace DemoData {
void fill(QList<Book> &books, QHash<QString, Progress> &progress, QHash<QString, QList<Bookmark>> &bookmarks);
}
