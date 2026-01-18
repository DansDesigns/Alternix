#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QFrame>
#include <QStackedWidget>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

// -----------------------------------------------------
// Alternix compact button style (same as Storage page)
// -----------------------------------------------------
static QString altBtnStyle(const QString &txtColor)
{
    return QString(
        "QPushButton {"
        " background:#444444;"
        " color:%1;"
        " border:1px solid #222222;"
        " border-radius:16px;"
        " font-size:22px;"
        " font-weight:bold;"
        " padding:6px 16px;"
        "}"
        "QPushButton:hover { background:#555555; }"
        "QPushButton:pressed { background:#333333; }"
    ).arg(txtColor);
}

static QPushButton* makeBtn(const QString &txt, const QString &color = "white")
{
    QPushButton *b = new QPushButton(txt);
    b->setStyleSheet(altBtnStyle(color));
    b->setMinimumSize(140, 54);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return b;
}

// Simple helper to grab stdout (ignore errors/exit code)
static QString runCmd(const QString &cmd)
{
    QProcess p;
    p.start("/bin/sh", {"-c", cmd});
    p.waitForFinished();
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

// -----------------------------------------------------
// EmulationPage
// -----------------------------------------------------
class EmulationPage : public QWidget
{
public:
    explicit EmulationPage(QStackedWidget *stack);

private:
    QStackedWidget *m_stack = nullptr;

    QLabel *m_wayInfo  = nullptr;
    QLabel *m_wineInfo = nullptr;

    // helpers
    bool runCmdOk(const QString &cmd, QString &output);

    QString buildWaydroidInfoHtml();
    QString buildWineInfoHtml();

    void refreshWaydroid();
    void refreshWine();

    void startWaydroid();
    void stopWaydroid();

    void startWine();
    void stopWine();
};

// -----------------------------------------------------
// Constructor
// -----------------------------------------------------
EmulationPage::EmulationPage(QStackedWidget *stack)
    : QWidget(stack), m_stack(stack)
{
    setStyleSheet("background:#282828; color:white; font-family:Sans;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);
    root->setSpacing(10);

    // Title
    QLabel *title = new QLabel("Emulation");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:42px; font-weight:bold;");
    root->addWidget(title);

    // Scroll area (no visible scrollbars, kinetic scrolling)
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);

    QWidget *wrap = new QWidget(scroll);
    QVBoxLayout *wrapLay = new QVBoxLayout(wrap);
    wrapLay->setSpacing(10);
    wrapLay->setContentsMargins(0,0,0,0);

    // Outer big rounded card
    QFrame *outer = new QFrame(wrap);
    outer->setStyleSheet("QFrame { background:#3a3a3a; border-radius:40px; }");
    QVBoxLayout *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(50, 30, 50, 30);
    outerLay->setSpacing(30);

    // =================================================
    // Waydroid card
    // =================================================
    QFrame *cardWaydroid = new QFrame(outer);
    cardWaydroid->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");
    QVBoxLayout *wayLay = new QVBoxLayout(cardWaydroid);
    wayLay->setContentsMargins(30, 30, 30, 30);
    wayLay->setSpacing(16);

    QLabel *wayLabel = new QLabel("Waydroid info");
    wayLabel->setAlignment(Qt::AlignCenter);
    wayLabel->setStyleSheet("font-size:28px; font-weight:bold;");
    wayLay->addWidget(wayLabel);

    m_wayInfo = new QLabel("Loading...");
    m_wayInfo->setWordWrap(true);
    m_wayInfo->setAlignment(Qt::AlignCenter);
    m_wayInfo->setStyleSheet(
        "QLabel { background:#3a3a3a; border-radius:20px; font-size:24px; padding:20px; }"
    );
    wayLay->addWidget(m_wayInfo);

    // Buttons row: Start / Stop / Refresh
    QHBoxLayout *wayBtnRow = new QHBoxLayout();
    wayBtnRow->setSpacing(16);

    QPushButton *wayStart = makeBtn("Start");
    QPushButton *wayStop  = makeBtn("Stop", "#CC6666");
    QPushButton *wayRefresh = makeBtn("Refresh");

    wayBtnRow->addWidget(wayStart);
    wayBtnRow->addWidget(wayStop);
    wayBtnRow->addWidget(wayRefresh);

    wayLay->addLayout(wayBtnRow);
    outerLay->addWidget(cardWaydroid);

    // =================================================
    // Wine card
    // =================================================
    QFrame *cardWine = new QFrame(outer);
    cardWine->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");
    QVBoxLayout *wineLay = new QVBoxLayout(cardWine);
    wineLay->setContentsMargins(30, 30, 30, 30);
    wineLay->setSpacing(16);

    QLabel *wineLabel = new QLabel("Wine info");
    wineLabel->setAlignment(Qt::AlignCenter);
    wineLabel->setStyleSheet("font-size:28px; font-weight:bold;");
    wineLay->addWidget(wineLabel);

    m_wineInfo = new QLabel("Loading...");
    m_wineInfo->setWordWrap(true);
    m_wineInfo->setAlignment(Qt::AlignCenter);
    m_wineInfo->setStyleSheet(
        "QLabel { background:#3a3a3a; border-radius:20px; font-size:24px; padding:20px; }"
    );
    wineLay->addWidget(m_wineInfo);

    QHBoxLayout *wineBtnRow = new QHBoxLayout();
    wineBtnRow->setSpacing(16);

    QPushButton *wineStart = makeBtn("Start");
    QPushButton *wineStop  = makeBtn("Stop", "#CC6666");
    QPushButton *wineRefresh = makeBtn("Refresh");

    wineBtnRow->addWidget(wineStart);
    wineBtnRow->addWidget(wineStop);
    wineBtnRow->addWidget(wineRefresh);

    wineLay->addLayout(wineBtnRow);
    outerLay->addWidget(cardWine);

    // =================================================
    wrapLay->addWidget(outer);
    wrapLay->addStretch();

    scroll->setWidget(wrap);
    root->addWidget(scroll);

    // Back button pinned at bottom
    QPushButton *back = makeBtn("❮");
    back->setFixedSize(140, 60);
    connect(back, &QPushButton::clicked, this, [this]() {
        if (m_stack)
            m_stack->setCurrentIndex(0);
    });
    root->addWidget(back, 0, Qt::AlignCenter);

    // Connections
    connect(wayStart,   &QPushButton::clicked, this, [this]() { startWaydroid();  refreshWaydroid(); });
    connect(wayStop,    &QPushButton::clicked, this, [this]() { stopWaydroid();   refreshWaydroid(); });
    connect(wayRefresh, &QPushButton::clicked, this, [this]() { refreshWaydroid(); });

    connect(wineStart,   &QPushButton::clicked, this, [this]() { startWine();  refreshWine(); });
    connect(wineStop,    &QPushButton::clicked, this, [this]() { stopWine();   refreshWine(); });
    connect(wineRefresh, &QPushButton::clicked, this, [this]() { refreshWine(); });

    // Initial info
    refreshWaydroid();
    refreshWine();
}

// -----------------------------------------------------
// Helpers
// -----------------------------------------------------
bool EmulationPage::runCmdOk(const QString &cmd, QString &output)
{
    QProcess p;
    p.start("/bin/sh", {"-c", cmd});
    if (!p.waitForFinished())
        return false;

    output = QString::fromLocal8Bit(p.readAllStandardOutput())
           + QString::fromLocal8Bit(p.readAllStandardError());

    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}

// Deeper Waydroid info (hybrid container+session view)
QString EmulationPage::buildWaydroidInfoHtml()
{
    QString hasWaydroid = runCmd("command -v waydroid >/dev/null 2>&1 && echo yes || echo no");
    if (hasWaydroid.trimmed() != "yes") {
        return "Waydroid not installed or not in PATH.";
    }

    QString version = runCmd("waydroid -V 2>/dev/null");
    QString contState = runCmd("systemctl is-active waydroid-container.service 2>/dev/null");
    QString sessState = runCmd("systemctl is-active waydroid-session.service 2>/dev/null");

    contState = contState.trimmed();
    sessState = sessState.trimmed();

    auto colorForState = [](const QString &s) -> QString {
        if (s == "active")   return "#7CFC00";
        if (s == "inactive") return "#FF5555";
        if (s.isEmpty() || s == "unknown") return "#CCCCCC";
        return "#CCCCCC";
    };

    QString contColor = colorForState(contState);
    QString sessColor = colorForState(sessState.isEmpty() ? "unknown" : sessState);

    QString contText = contState.isEmpty() ? "unknown" : contState;
    QString sessText = sessState.isEmpty() ? "unknown" : sessState;

    // Android image version only if container is active
    QString androidVer;
    if (contState == "active") {
        androidVer = runCmd("waydroid shell getprop ro.build.version.release 2>/dev/null");
        if (androidVer.isEmpty())
            androidVer = "unknown (container running, but property not available)";
    } else {
        androidVer = "offline (container not running)";
    }

    QString binderfs = runCmd("[ -e /dev/binderfs ] && echo present || echo missing");

    QString html;
    html  = QString("Installed: <b>yes</b><br>");
    html += "Version: " + version.toHtmlEscaped() + "<br>";
    html += QString("Container: <span style='color:%1;'>%2</span><br>")
            .arg(contColor, contText.toHtmlEscaped());
    html += QString("Session: <span style='color:%1;'>%2</span><br>")
            .arg(sessColor, sessText.toHtmlEscaped());
    html += "Android image: " + androidVer.toHtmlEscaped() + "<br>";
    html += "Binderfs: " + binderfs.toHtmlEscaped();

    return html;
}

// Deeper Wine info (wineserver based)
QString EmulationPage::buildWineInfoHtml()
{
    QString hasWine = runCmd("command -v wine >/dev/null 2>&1 && echo yes || echo no");
    if (hasWine.trimmed() != "yes") {
        return "Wine not installed or not in PATH.";
    }

    QString version  = runCmd("wine --version 2>/dev/null");
    QString prefix   = runCmd("printf \"%s\" \"${WINEPREFIX:-$HOME/.wine}\" 2>/dev/null");
    QString backend  = runCmd("if [ -n \"$WAYLAND_DISPLAY\" ]; then echo Wayland; "
                              "elif [ -n \"$DISPLAY\" ]; then echo X11; "
                              "else echo Unknown; fi");
    QString running  = runCmd("pidof wineserver >/dev/null 2>&1 && echo running || echo stopped");

    QString runColor = (running.trimmed() == "running") ? "#7CFC00" : "#CCCCCC";

    QString html;
    html  = "Installed: <b>yes</b><br>";
    html += "Version: " + version.toHtmlEscaped() + "<br>";
    html += QString("Wineserver: <span style='color:%1;'>%2</span><br>")
            .arg(runColor, running.toHtmlEscaped());
    html += "Default prefix: " + prefix.toHtmlEscaped() + "<br>";
    html += "Graphics backend: " + backend.toHtmlEscaped();

    return html;
}

// -----------------------------------------------------
// Refresh functions
// -----------------------------------------------------
void EmulationPage::refreshWaydroid()
{
    if (m_wayInfo)
        m_wayInfo->setText(buildWaydroidInfoHtml());
}

void EmulationPage::refreshWine()
{
    if (m_wineInfo)
        m_wineInfo->setText(buildWineInfoHtml());
}

// -----------------------------------------------------
// Start/Stop actions
// -----------------------------------------------------
// Hybrid C: container + session
void EmulationPage::startWaydroid()
{
    QString out;
    bool okCont = runCmdOk("systemctl start waydroid-container.service", out);
    if (!okCont) {
        okCont = runCmdOk("sudo systemctl start waydroid-container.service", out);
    }

    QString out2;
    bool okSess = runCmdOk("waydroid session start", out2);

    if (!okCont || !okSess) {
        QMessageBox::warning(
            this,
            "Waydroid start failed",
            "Could not start Waydroid completely.\n\n"
            "Container output:\n" + out + "\n\n"
            "Session output:\n" + out2
        );
    }
}

void EmulationPage::stopWaydroid()
{
    QString outSess;
    bool okSess = runCmdOk("waydroid session stop", outSess);

    QString outCont;
    bool okCont = runCmdOk("systemctl stop waydroid-container.service", outCont);
    if (!okCont) {
        okCont = runCmdOk("sudo systemctl stop waydroid-container.service", outCont);
    }

    if (!okCont || !okSess) {
        QMessageBox::warning(
            this,
            "Waydroid stop failed",
            "Could not stop Waydroid completely.\n\n"
            "Session output:\n" + outSess + "\n\n"
            "Container output:\n" + outCont
        );
    }
}

// Wine option 3: wineserver -p / wineserver -k
void EmulationPage::startWine()
{
    QString out;
    bool ok = runCmdOk("wineserver -p", out);
    if (!ok) {
        QMessageBox::warning(
            this,
            "Wine start failed",
            "Could not start wineserver.\n\n" + out
        );
    }
}

void EmulationPage::stopWine()
{
    QString out;
    bool ok = runCmdOk("wineserver -k", out);
    if (!ok) {
        QMessageBox::warning(
            this,
            "Wine stop failed",
            "Could not stop wineserver.\n\n" + out
        );
    }
}

// -----------------------------------------------------
// Factory for osm-settings
// -----------------------------------------------------
extern "C" QWidget* make_page(QStackedWidget *stack)
{
    return new EmulationPage(stack);
}
