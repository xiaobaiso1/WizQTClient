#include <QtGlobal>
#include <QApplication>
#include <QMessageBox>
#include <QIcon>
#include <QDir>
#include <QPixmapCache>
#include <QTranslator>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>

#include <extensionsystem/pluginmanager.h>
#include "wizmainwindow.h"
#include "wizupdaterprogressdialog.h"
#include "wizLoginDialog.h"
#include "share/wizsettings.h"
#include "share/wizwin32helper.h"
#include "share/wizDatabaseManager.h"

using namespace ExtensionSystem;


static inline QStringList getPluginPaths()
{
    QStringList rc;
    // Figure out root:  Up one from 'bin'
    QDir rootDir = QApplication::applicationDirPath();
    rootDir.cdUp();
    const QString rootDirPath = rootDir.canonicalPath();
#if !defined(Q_OS_MAC)
    // 1) "plugins" (Win/Linux)
    QString pluginPath = rootDirPath;
    pluginPath += QLatin1Char('/');
    pluginPath += QLatin1String("/share/wiznote/plugins");
    rc.push_back(pluginPath);
#else
    // 2) "PlugIns" (OS X)
    QString pluginPath = rootDirPath;
    pluginPath += QLatin1String("/PlugIns");
    rc.push_back(pluginPath);
#endif
    // 3) <localappdata>/plugins/<ideversion>
    //    where <localappdata> is e.g.
    //    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
    //    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
    //    "~/Library/Application Support/QtProject/Qt Creator" on Mac
#if QT_VERSION >= 0x050000
    pluginPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
    pluginPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
    pluginPath += QLatin1Char('/');

#if !defined(Q_OS_MAC)
    pluginPath += QLatin1String("wiznote");
#else
    pluginPath += QLatin1String("WizNote");
#endif
    pluginPath += QLatin1String("/plugins/");
    rc.push_back(pluginPath);
    return rc;
}


#ifdef Q_OS_MAC
#  define SHARE_PATH "/../Resources"
#else
#  define SHARE_PATH "/../share/wiznote"
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setApplicationName(QObject::tr("WizNote"));
    QApplication::setWindowIcon(QIcon(":/logo.png"));

#ifdef Q_OS_MAC
    // enable switch between qt widget and alien widget(cocoa)
    // refer to: https://bugreports.qt-project.org/browse/QTBUG-11401
    a.setAttribute(Qt::AA_NativeWindows);
#endif

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       QCoreApplication::applicationDirPath() + QLatin1String(SHARE_PATH));

    QSettings* settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                        QLatin1String("wiz"), QLatin1String("wiznote"));

    QSettings *globalSettings = new QSettings(QSettings::IniFormat, QSettings::SystemScope,
                                              QLatin1String("wiz"), QLatin1String("wiznote"));

    // use 3 times(30M) of Qt default usage
    int nCacheSize = settings->value("Common/Cache", 10240*3).toInt();
    QPixmapCache::setCacheLimit(nCacheSize);

//#ifdef Q_OS_WIN
//    QString strDefaultFontName = settings.GetString("Common", "DefaultFont", "");
//    QFont f = WizCreateWindowsUIFont(a, strDefaultFontName);
//    a.setFont(f);
//#endif

    PluginManager pluginManager;
    PluginManager::setFileExtension(QLatin1String("pluginspec"));
    PluginManager::setGlobalSettings(globalSettings);
    PluginManager::setSettings(settings);

    const QStringList pluginPaths = getPluginPaths();
    PluginManager::setPluginPaths(pluginPaths);

    QString strUserId = settings->value("Users/DefaultUser", "").toString();
    QString strPassword;

    CWizUserSettings userSettings(strUserId);

    // setup locale for welcome dialog
    QString strLocale = userSettings.locale();
    QLocale::setDefault(strLocale);

    QTranslator translatorWizNote;
    QString strLocaleFile = WizGetLocaleFileName(strLocale);
    translatorWizNote.load(strLocaleFile);
    a.installTranslator(&translatorWizNote);

    QTranslator translatorQt;
    strLocaleFile = WizGetQtLocaleFileName(strLocale);
    translatorQt.load(strLocaleFile);
    a.installTranslator(&translatorQt);

    // check update if needed
    //CWizUpdaterDialog updater;
    //if (updater.checkNeedUpdate()) {
    //    updater.show();
    //    updater.doUpdate();
    //    int ret = a.exec();
    //    QProcess::startDetached(argv[0], QStringList());
    //    return ret;
    //}

    // figure out auto login or manually login
    bool bFallback = true;

    bool bAutoLogin = userSettings.autoLogin();
    strPassword = userSettings.password();

    if (bAutoLogin && !strPassword.isEmpty()) {
        bFallback = false;
    }

    // manually login
    CWizLoginDialog loginDialog(strUserId, strLocale);
    if (bFallback) {
        if (QDialog::Accepted != loginDialog.exec())
            return 0;

        strUserId = loginDialog.userId();
        strPassword = loginDialog.password();
    }

    // reset locale for current user.
    userSettings.setUser(strUserId);
    strLocale = userSettings.locale();

    a.removeTranslator(&translatorWizNote);
    strLocaleFile = WizGetLocaleFileName(strLocale);
    translatorWizNote.load(strLocaleFile);
    a.installTranslator(&translatorWizNote);

    a.removeTranslator(&translatorQt);
    strLocaleFile = WizGetQtLocaleFileName(strLocale);
    translatorQt.load(strLocaleFile);
    a.installTranslator(&translatorQt);

    CWizDatabaseManager dbMgr(strUserId);
    if (!dbMgr.openAll()) {
        QMessageBox::critical(NULL, "", QObject::tr("Can not open database"));
        return 0;
    }

    dbMgr.db().SetPassword(::WizEncryptPassword(strPassword));

    MainWindow w(dbMgr);

    settings->setValue("Users/DefaultUser", strUserId);
    PluginManager::loadPlugins();

    w.show();
    w.init();

    int ret = a.exec();

    // clean up
    QString strTempPath = ::WizGlobal()->GetTempPath();
    ::WizDeleteAllFilesInFolder(strTempPath);

    if (w.isLogout()) {
        QProcess::startDetached(argv[0], QStringList());
    }

    return ret;
}
