#include "AbsClient.h"
#include "AppSettings.h"
#include "CoverProvider.h"
#include "DeviceControl.h"
#include "Library.h"
#include "Palette.h"
#include "PairingServer.h"
#include "PlaybackController.h"
#include "audio/QtAudioBackend.h"
#include "audio/SimulatedBackend.h"

#include <QCommandLineParser>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // No GPU on the clock: render Qt Quick with the software (QPainter) backend everywhere,
    // so the desktop build shows what the device can do. --gpu overrides for comparison.
    bool gpu = false;
    for (int i = 1; i < argc; ++i)
        gpu |= qstrcmp(argv[i], "--gpu") == 0;
    if (!gpu)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

    QGuiApplication app(argc, argv);
    app.setOrganizationName("abs-clock");
    app.setApplicationName("nightstand");
    app.setApplicationVersion(NIGHTSTAND_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Audiobookshelf nightstand player");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"demo", "Run with the built-in sample library and simulated audio."});
    parser.addOption({"speed", "Simulated playback speed in demo mode.", "factor", "1"});
    parser.addOption({"gpu", "Use the default (GPU) scene graph backend."});
    parser.addOption({"fps", "Show and log rendered frames per second."});
    parser.addOption({"rotate", "Rotate the UI by <degrees> (portrait framebuffers).", "degrees", "0"});
    parser.addOption({"fake-wifi","Simulate wpa_supplicant (desktop testing of the Wi-Fi screens)."});
    // Development aid: render one screen to a PNG and quit.
    parser.addOption({"shot", "Save a screenshot to <file> and exit.", "file"});
    parser.addOption({"screen", "Screen to show with --shot (library, player, ...).", "name"});
    parser.addOption({"set", "With --shot: set Ui properties, e.g. overlay=chapters,tab=series.", "props"});
    parser.process(app);
    const bool demo = parser.isSet("demo");

    QFontDatabase::addApplicationFont(":/qt/qml/Nightstand/fonts/InstrumentSans.ttf");
    QFont font("Instrument Sans");
    font.setPixelSize(16);
    font.setHintingPreference(QFont::PreferNoHinting);
    app.setFont(font);

    AppSettings settings;
    AbsClient client(demo);
    Library library(&client);
    PaletteService palette(&library);
    CoverStore covers(&client, &palette);
    AudioBackend *backend = demo ? static_cast<AudioBackend *>(new SimulatedBackend(parser.value("speed").toDouble()))
                                 : new QtAudioBackend;
    PlaybackController player(&client, &library, &settings, backend);
    backend->setParent(&player);
    DeviceControl device;
    if (demo || parser.isSet("fake-wifi"))
        device.setSimulated(true);
    PairingServer pairing(&client, &device);

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &player, &PlaybackController::stop);

    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Abs", &client);
    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Settings", &settings);
    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Library", &library);
    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Palette", &palette);
    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Player", &player);
    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Device", &device);
    qmlRegisterSingletonInstance("Nightstand.Core", 1, 0, "Pairing", &pairing);

    QQmlApplicationEngine engine;

    // Language can change at runtime (Settings → Display); retranslate() refreshes every qsTr().
    QTranslator translator;
    QString currentLanguage;
    const auto applyLanguage = [&] {
        const QString lang = settings.language();
        if (lang == currentLanguage)
            return;
        currentLanguage = lang;
        QCoreApplication::removeTranslator(&translator);
        if (lang != "en" && translator.load(":/i18n/nightstand_" + lang + ".qm"))
            QCoreApplication::installTranslator(&translator);
        QLocale::setDefault(lang == "de" ? QLocale(QLocale::German, QLocale::Germany)
                                         : QLocale(QLocale::English, QLocale::UnitedKingdom));
        engine.retranslate();
    };
    applyLanguage();
    QObject::connect(&settings, &AppSettings::changed, &engine, applyLanguage);
    engine.addImageProvider("cover", new CoverProvider(&covers));
    engine.addImageProvider("bg", new BackgroundProvider);
    engine.addImageProvider("qr", new QrImageProvider);

    const QString platform = QGuiApplication::platformName();
    const bool embedded = platform == "linuxfb" || platform == "eglfs" || platform == "vkkhrdisplay";
    engine.setInitialProperties({{"fullScreen", embedded}, {"stageRotation", parser.value("rotate").toInt()}});

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Nightstand", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window)
        device.attach(window);

    if (parser.isSet("fps") && window) {
        // Frames actually presented per second; idle screens legitimately show 0-1.
        auto *frames = new int(0);
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [frames] { ++*frames; });
        auto *timer = new QTimer(window);
        QObject::connect(timer, &QTimer::timeout, window, [frames, &engine] {
            if (QObject *ui = engine.singletonInstance<QObject *>("Nightstand", "Ui"))
                ui->setProperty("fps", *frames);
            qInfo("fps %d", *frames);
            *frames = 0;
        });
        timer->start(1000);
    }

    if (parser.isSet("shot") && window) {
        QTimer::singleShot(400, &app, [&] {
            QObject *ui = engine.singletonInstance<QObject *>("Nightstand", "Ui");
            if (parser.isSet("screen"))
                QMetaObject::invokeMethod(ui, "go", Q_ARG(QVariant, parser.value("screen")));
            for (const QString &kv : parser.value("set").split(',', Qt::SkipEmptyParts)) {
                const QString key = kv.section('=', 0, 0), value = kv.section('=', 1);
                if (key == "detailId")
                    QMetaObject::invokeMethod(ui, "openDetail", Q_ARG(QVariant, value));
                else if (key == "play")
                    QMetaObject::invokeMethod(ui, "play", Q_ARG(QVariant, value), Q_ARG(QVariant, QVariant()));
                else if (key == "call") {
                    // call=fn[:arg][@ms], e.g. call=nextChapter or call=seek:3000@6000 (default 4 s)
                    const QString spec = value.section('@', 0, 0);
                    const int at = value.contains('@') ? value.section('@', 1).toInt() : 4000;
                    QTimer::singleShot(at, &player, [&player, spec] {
                        const QString fn = spec.section(':', 0, 0), arg = spec.section(':', 1);
                        if (arg.isEmpty())
                            QMetaObject::invokeMethod(&player, fn.toUtf8());
                        else if (fn == "extendSleep" || fn == "setSleepMinutes")
                            QMetaObject::invokeMethod(&player, fn.toUtf8(), Q_ARG(int, arg.toInt()));
                        else
                            QMetaObject::invokeMethod(&player, fn.toUtf8(), Q_ARG(double, arg.toDouble()));
                        qInfo("call %s -> pos=%.1f playing=%d sleep=%s fading=%d", qPrintable(spec), player.position(),
                              player.playing(), qPrintable(player.sleepMode()), player.sleepFading());
                    });
                }
                else if (key == "sleep" && value == "eoc")
                    player.setSleepEndOfChapter();
                else if (key == "sleep")
                    player.setSleepMinutes(value.toInt());
                else
                    ui->setProperty(key.toUtf8(), value);
            }
        });
        QTimer::singleShot(qEnvironmentVariableIntValue("NIGHTSTAND_SHOT_MS") ?: 2500, &app, [&] {
            const QVariantMap prog = library.progress(player.bookId());
            qInfo("shot: books=%d library=%s error=%s book=%s pos=%.1f chapter=%d/%d playing=%d loading=%d "
                  "playerError=%s sleep=%s fading=%d rate=%.2f savedProgress=%.1f",
                  library.books()->rowCount(), qPrintable(library.libraryName()), qPrintable(library.error()),
                  qPrintable(player.bookId()), player.position(), player.chapterIndex(), player.chapterCount(),
                  player.playing(), player.loading(), qPrintable(player.error()), qPrintable(player.sleepMode()),
                  player.sleepFading(), player.rate(), prog.value("time").toDouble());
            window->grabWindow().save(parser.value("shot"));
            // Pausing sends a progress sync; give it a moment before quitting.
            player.pause();
            QTimer::singleShot(1500, &app, &QCoreApplication::quit);
        });
    }

    return app.exec();
}
