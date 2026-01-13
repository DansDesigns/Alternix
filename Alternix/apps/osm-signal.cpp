#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QFrame>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMap>
#include <QTimer>
#include <QTextEdit>
#include <QShowEvent>
#include <QHideEvent>
#include <QTextCursor>
#include <QLineEdit>
#include <QSizePolicy>

// -----------------------------------------------------
// Alternix compact button style (same as SecurityPage)
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

// -----------------------------------------------------
// Simple command helpers
// -----------------------------------------------------
static QString runCmd(const QString &cmd)
{
    QProcess p;
    p.start("/bin/sh", {"-c", cmd});
    p.waitForFinished();
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

static bool runCmdOk(const QString &cmd, QString &output)
{
    QProcess p;
    p.start("/bin/sh", {"-c", cmd});
    if (!p.waitForFinished())
        return false;

    output = QString::fromLocal8Bit(p.readAllStandardOutput()) +
             QString::fromLocal8Bit(p.readAllStandardError());

    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}

// -----------------------------------------------------
// CONFIG LOAD / SAVE
// -----------------------------------------------------
static QString cfgFile()
{
    // standalone config, separate from osm-settings
    return QDir::homePath() + "/.config/Alternix/signal-ui.conf";
}

static QMap<QString,QString> loadCfg()
{
    QMap<QString,QString> map;
    QFile f(cfgFile());
    if (!f.exists()) return map;

    if (f.open(QFile::ReadOnly))
    {
        QTextStream s(&f);
        while (!s.atEnd())
        {
            QString line = s.readLine().trimmed();
            if (line.startsWith("#") || !line.contains("="))
                continue;
            QStringList parts = line.split("=");
            if (parts.size() == 2)
                map[parts[0].trimmed()] = parts[1].trimmed();
        }
    }
    return map;
}

static void saveCfg(const QMap<QString,QString> &map)
{
    QFile f(cfgFile());
    f.open(QFile::WriteOnly | QFile::Truncate);
    QTextStream s(&f);
    for (auto it = map.begin(); it != map.end(); ++it)
        s << it.key() << "=" << it.value() << "\n";
}

// -----------------------------------------------------
// SignalMainWidget (no Q_OBJECT, no moc)
// -----------------------------------------------------
class SignalMainWidget : public QWidget
{
public:
    explicit SignalMainWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    QLineEdit *m_phoneEdit   = nullptr;
    QLineEdit *m_codeEdit    = nullptr;
    QLineEdit *m_pinEdit     = nullptr;
    QLineEdit *m_recipient   = nullptr;
    QTextEdit *m_messageEdit = nullptr;
    QLineEdit *m_attachEdit  = nullptr;

    QTextEdit *m_logEdit     = nullptr;

    QTimer *m_recvTimer      = nullptr;

    QMap<QString,QString> cfg;

    // config helpers
    void writeCfgKey(const QString &k, const QString &v);
    QString readCfg(const QString &k, const QString &def = QString()) const;

    // ui builders
    QWidget *makeAccountCard();
    QWidget *makeVerifyCard();
    QWidget *makeSendCard();
    QWidget *makeReceiveCard();

    // backend helpers
    void appendLog(const QString &line);
    QString currentAccount() const;

    void doRegister();
    void doVerify();
    void doSend();
    void doReceiveOnce();
};

// -----------------------------------------------------
// Constructor
// -----------------------------------------------------
SignalMainWidget::SignalMainWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Alternix Signal");
    resize(1100, 800);
    setStyleSheet("background:#282828; color:white; font-family:Sans;");
    cfg = loadCfg();

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);
    root->setSpacing(10);

    QLabel *title = new QLabel("Signal");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:42px; font-weight:bold;");
    root->addWidget(title);

    // Scroll area (same pattern as security page)
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);

    QWidget *wrap = new QWidget(scroll);
    QVBoxLayout *wrapLay = new QVBoxLayout(wrap);
    wrapLay->setSpacing(10);
    wrapLay->setContentsMargins(0, 0, 0, 0);

    QFrame *outer = new QFrame(wrap);
    outer->setStyleSheet("QFrame { background:#3a3a3a; border-radius:40px; }");
    QVBoxLayout *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(50, 30, 50, 30);
    outerLay->setSpacing(30);

    outerLay->addWidget(makeAccountCard());
    outerLay->addWidget(makeVerifyCard());
    outerLay->addWidget(makeSendCard());
    outerLay->addWidget(makeReceiveCard());

    wrapLay->addWidget(outer);
    wrapLay->addStretch();

    scroll->setWidget(wrap);
    root->addWidget(scroll);

    // Receive timer (disabled by default – you can enable auto-poll later)
    m_recvTimer = new QTimer(this);
    m_recvTimer->setInterval(8000); // 8 s
    QObject::connect(m_recvTimer, &QTimer::timeout, [this]() {
        doReceiveOnce();
    });

    // Load config values into fields
    QString phone = readCfg("signal_phone", "");
    if (!phone.isEmpty() && m_phoneEdit)
        m_phoneEdit->setText(phone);

    QString lastRecipient = readCfg("signal_last_recipient", "");
    if (!lastRecipient.isEmpty() && m_recipient)
        m_recipient->setText(lastRecipient);
}

// -----------------------------------------------------
// show/hide events
// -----------------------------------------------------
void SignalMainWidget::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    // If you want auto receive, uncomment:
    // if (m_recvTimer && !m_recvTimer->isActive())
    //     m_recvTimer->start();
}

void SignalMainWidget::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    if (m_recvTimer && m_recvTimer->isActive())
        m_recvTimer->stop();
}

// -----------------------------------------------------
// Config helpers
// -----------------------------------------------------
QString SignalMainWidget::readCfg(const QString &k, const QString &def) const
{
    return cfg.contains(k) ? cfg.value(k) : def;
}

void SignalMainWidget::writeCfgKey(const QString &k, const QString &v)
{
    cfg[k] = v;
    saveCfg(cfg);
}

// -----------------------------------------------------
// UI builders
// -----------------------------------------------------
QWidget *SignalMainWidget::makeAccountCard()
{
    QFrame *card = new QFrame;
    card->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(30, 20, 30, 20);
    lay->setSpacing(10);

    QLabel *lbl = new QLabel("Account");
    lbl->setStyleSheet("font-size:30px; font-weight:bold;");
    lay->addWidget(lbl);

    // phone row
    QHBoxLayout *row = new QHBoxLayout;
    row->setSpacing(10);

    QLabel *plbl = new QLabel("Phone (+CC...)");
    plbl->setStyleSheet("font-size:22px;");
    row->addWidget(plbl);

    m_phoneEdit = new QLineEdit(card);
    m_phoneEdit->setPlaceholderText("+441234567890");
    m_phoneEdit->setStyleSheet(
        "QLineEdit {"
        " background:#3a3a3a;"
        " border-radius:20px;"
        " padding:8px 14px;"
        " font-size:22px;"
        " color:white;"
        "}"
    );
    row->addWidget(m_phoneEdit, 1);

    lay->addLayout(row);

    // buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(20);

    QPushButton *regBtn  = makeBtn("Register");
    QPushButton *saveBtn = makeBtn("Save", "#aaddff");

    btnRow->addWidget(regBtn);
    btnRow->addWidget(saveBtn);
    lay->addLayout(btnRow);

    QObject::connect(saveBtn, &QPushButton::clicked, [this]() {
        QString phone = m_phoneEdit ? m_phoneEdit->text().trimmed() : QString();
        writeCfgKey("signal_phone", phone);
        appendLog("Saved account: " + phone);
    });

    QObject::connect(regBtn, &QPushButton::clicked, [this]() {
        doRegister();
    });

    return card;
}

QWidget *SignalMainWidget::makeVerifyCard()
{
    QFrame *card = new QFrame;
    card->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(30, 20, 30, 20);
    lay->setSpacing(10);

    QLabel *lbl = new QLabel("Verification");
    lbl->setStyleSheet("font-size:30px; font-weight:bold;");
    lay->addWidget(lbl);

    // Code row
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(10);

        QLabel *clbl = new QLabel("Code");
        clbl->setStyleSheet("font-size:22px;");
        row->addWidget(clbl);

        m_codeEdit = new QLineEdit(card);
        m_codeEdit->setPlaceholderText("123-456");
        m_codeEdit->setStyleSheet(
            "QLineEdit {"
            " background:#3a3a3a;"
            " border-radius:20px;"
            " padding:8px 14px;"
            " font-size:22px;"
            " color:white;"
            "}"
        );
        row->addWidget(m_codeEdit, 1);

        lay->addLayout(row);
    }

    // PIN row (optional)
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(10);

        QLabel *plbl = new QLabel("PIN (optional)");
        plbl->setStyleSheet("font-size:22px;");
        row->addWidget(plbl);

        m_pinEdit = new QLineEdit(card);
        m_pinEdit->setEchoMode(QLineEdit::Password);
        m_pinEdit->setPlaceholderText("Signal PIN");
        m_pinEdit->setStyleSheet(
            "QLineEdit {"
            " background:#3a3a3a;"
            " border-radius:20px;"
            " padding:8px 14px;"
            " font-size:22px;"
            " color:white;"
            "}"
        );
        row->addWidget(m_pinEdit, 1);

        lay->addLayout(row);
    }

    QPushButton *verifyBtn = makeBtn("Verify", "#aaffaa");
    lay->addWidget(verifyBtn, 0, Qt::AlignLeft);

    QObject::connect(verifyBtn, &QPushButton::clicked, [this]() {
        doVerify();
    });

    return card;
}

QWidget *SignalMainWidget::makeSendCard()
{
    QFrame *card = new QFrame;
    card->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(30, 20, 30, 20);
    lay->setSpacing(10);

    QLabel *lbl = new QLabel("Send Message");
    lbl->setStyleSheet("font-size:30px; font-weight:bold;");
    lay->addWidget(lbl);

    // Recipient row
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(10);

        QLabel *rlbl = new QLabel("To");
        rlbl->setStyleSheet("font-size:22px;");
        row->addWidget(rlbl);

        m_recipient = new QLineEdit(card);
        m_recipient->setPlaceholderText("+441234567891");
        m_recipient->setStyleSheet(
            "QLineEdit {"
            " background:#3a3a3a;"
            " border-radius:20px;"
            " padding:8px 14px;"
            " font-size:22px;"
            " color:white;"
            "}"
        );
        row->addWidget(m_recipient, 1);

        lay->addLayout(row);
    }

    // Message text
    m_messageEdit = new QTextEdit(card);
    m_messageEdit->setPlaceholderText("Type message…");
    m_messageEdit->setStyleSheet(
        "QTextEdit {"
        " background:#3a3a3a;"
        " border-radius:20px;"
        " color:white;"
        " font-size:20px;"
        "}"
    );
    m_messageEdit->setFixedHeight(120);
    lay->addWidget(m_messageEdit);

    // Attachment row
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(10);

        QLabel *albl = new QLabel("Attachment");
        albl->setStyleSheet("font-size:22px;");
        row->addWidget(albl);

        m_attachEdit = new QLineEdit(card);
        m_attachEdit->setPlaceholderText("/path/to/file (optional - image/audio/etc)");
        m_attachEdit->setStyleSheet(
            "QLineEdit {"
            " background:#3a3a3a;"
            " border-radius:20px;"
            " padding:8px 14px;"
            " font-size:22px;"
            " color:white;"
            "}"
        );
        row->addWidget(m_attachEdit, 1);

        lay->addLayout(row);
    }

    // Send button
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(20);

    QPushButton *sendBtn = makeBtn("Send", "#aaffaa");
    btnRow->addWidget(sendBtn);

    lay->addLayout(btnRow);

    QObject::connect(sendBtn, &QPushButton::clicked, [this]() {
        doSend();
    });

    return card;
}

QWidget *SignalMainWidget::makeReceiveCard()
{
    QFrame *card = new QFrame;
    card->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(30, 20, 30, 20);
    lay->setSpacing(10);

    QLabel *lbl = new QLabel("Messages / Log");
    lbl->setStyleSheet("font-size:26px; font-weight:bold;");
    lay->addWidget(lbl);

    m_logEdit = new QTextEdit(card);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(
        "QTextEdit {"
        " background:#3a3a3a;"
        " border-radius:20px;"
        " color:white;"
        " font-family:monospace;"
        " font-size:16px;"
        "}"
    );
    m_logEdit->setFixedHeight(280);
    lay->addWidget(m_logEdit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(20);

    QPushButton *recvBtn  = makeBtn("Receive Now");
    QPushButton *clearBtn = makeBtn("Clear Log", "#ffaaaa");

    btnRow->addWidget(recvBtn);
    btnRow->addWidget(clearBtn);

    lay->addLayout(btnRow);

    QObject::connect(recvBtn, &QPushButton::clicked, [this]() {
        doReceiveOnce();
    });

    QObject::connect(clearBtn, &QPushButton::clicked, [this]() {
        if (m_logEdit)
            m_logEdit->clear();
    });

    return card;
}

// -----------------------------------------------------
// Backend helpers
// -----------------------------------------------------
void SignalMainWidget::appendLog(const QString &line)
{
    if (!m_logEdit) return;

    if (!m_logEdit->toPlainText().isEmpty())
        m_logEdit->append("\n");

    m_logEdit->append(line);
    m_logEdit->moveCursor(QTextCursor::End);
}

QString SignalMainWidget::currentAccount() const
{
    if (!m_phoneEdit)
        return QString();
    return m_phoneEdit->text().trimmed();
}

void SignalMainWidget::doRegister()
{
    QString account = currentAccount();
    if (account.isEmpty()) {
        appendLog("ERROR: phone/account is empty.");
        return;
    }

    writeCfgKey("signal_phone", account);

    QString cmd = QString("signal-cli -a '%1' register").arg(account);
    appendLog("$ " + cmd);

    QString out;
    bool ok = runCmdOk(cmd, out);
    appendLog(out.trimmed().isEmpty() ? "(no output)" : out.trimmed());

    if (!ok) {
        appendLog("Registration command failed.");
    } else {
        appendLog("Registration command finished.");
    }
}

void SignalMainWidget::doVerify()
{
    QString account = currentAccount();
    if (account.isEmpty()) {
        appendLog("ERROR: phone/account is empty.");
        return;
    }

    QString code = m_codeEdit ? m_codeEdit->text().trimmed() : QString();
    if (code.isEmpty()) {
        appendLog("ERROR: verification code is empty.");
        return;
    }

    QString cmd = QString("signal-cli -a '%1' verify '%2'")
                  .arg(account, code);

    QString pin = m_pinEdit ? m_pinEdit->text().trimmed() : QString();
    if (!pin.isEmpty()) {
        cmd += QString(" --pin '%1'").arg(pin);
    }

    appendLog("$ " + cmd);

    QString out;
    bool ok = runCmdOk(cmd, out);
    appendLog(out.trimmed().isEmpty() ? "(no output)" : out.trimmed());

    if (!ok) {
        appendLog("Verification command failed.");
    } else {
        appendLog("Verification command finished.");
    }
}

void SignalMainWidget::doSend()
{
    QString account = currentAccount();
    if (account.isEmpty()) {
        appendLog("ERROR: phone/account is empty.");
        return;
    }

    if (!m_recipient) {
        appendLog("ERROR: recipient field missing.");
        return;
    }

    QString to = m_recipient->text().trimmed();
    if (to.isEmpty()) {
        appendLog("ERROR: recipient is empty.");
        return;
    }

    writeCfgKey("signal_last_recipient", to);

    QString msg = m_messageEdit ? m_messageEdit->toPlainText().trimmed() : QString();
    QString att = m_attachEdit ? m_attachEdit->text().trimmed() : QString();

    QString cmd = QString("signal-cli -a '%1' send").arg(account);

    if (!msg.isEmpty()) {
        QString esc = msg;
        esc.replace("\"", "\\\"");
        cmd += QString(" -m \"%1\"").arg(esc);
    }

    if (!att.isEmpty()) {
        QString escA = att;
        escA.replace("\"", "\\\"");
        cmd += QString(" -a \"%1\"").arg(escA);
    }

    cmd += QString(" '%1'").arg(to);

    appendLog("$ " + cmd);

    QString out;
    bool ok = runCmdOk(cmd, out);
    appendLog(out.trimmed().isEmpty() ? "(no output)" : out.trimmed());

    if (!ok) {
        appendLog("Send command failed.");
    } else {
        appendLog("Send command finished.");
    }
}

void SignalMainWidget::doReceiveOnce()
{
    QString account = currentAccount();
    if (account.isEmpty()) {
        appendLog("ERROR: phone/account is empty.");
        return;
    }

    QString cmd = QString("signal-cli -a '%1' --output=json receive").arg(account);
    appendLog("$ " + cmd);

    QString out;
    bool ok = runCmdOk(cmd, out);
    if (!ok) {
        appendLog("Receive command failed.");
        appendLog(out.trimmed().isEmpty() ? "(no output)" : out.trimmed());
        return;
    }

    if (out.trimmed().isEmpty()) {
        appendLog("(no new messages)");
    } else {
        appendLog(out.trimmed());
    }
}

// -----------------------------------------------------
// main()
// -----------------------------------------------------
int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    SignalMainWidget w;
    w.show();

    return app.exec();
}
