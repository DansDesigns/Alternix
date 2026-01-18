#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QFrame>
#include <QProcess>
#include <QScrollArea>
#include <QScroller>
#include <QStringList>

// ---------------------------------------------------------
// Helpers (Ethernet)
// ---------------------------------------------------------

static QString runCommandEth(const QString &cmd)
{
    QProcess p;
    p.start("bash", {"-c", cmd});
    p.waitForFinished();
    QString out = p.readAllStandardOutput();
    out += p.readAllStandardError();
    return out.trimmed();
}

// Bluetooth-style uniform button
static QPushButton* smallBtnEth(const QString &txt)
{
    QPushButton *b = new QPushButton(txt);
    b->setFixedSize(180, 60);
    b->setStyleSheet(
        "QPushButton {"
        " background:#444444;"
        " color:white;"
        " border:1px solid #222222;"
        " border-radius:16px;"
        " font-size:26px;"
        " font-weight:bold;"
        " padding:10px 24px;"
        "}"
        "QPushButton:hover { background:#555555; }"
        "QPushButton:pressed { background:#333333; }"
    );
    return b;
}

static bool isEthernetPowered()
{
    QString s = runCommandEth("cat /sys/class/net/eth0/operstate 2>/dev/null");
    return s.contains("up", Qt::CaseInsensitive);
}

static void setEthernetPowered(bool on)
{
    if (on) runCommandEth("sudo ip link set eth0 up");
    else    runCommandEth("sudo ip link set eth0 down");
}

// ---------------------------------------------------------
// EthernetPage
// ---------------------------------------------------------

class EthernetPage : public QWidget
{
public:
    explicit EthernetPage(QStackedWidget *stack, QWidget *parent = nullptr)
        : QWidget(parent), stackedWidget(stack)
    {
        //
        // ★ GLOBAL FONT FIX — NEVER REMOVE
        //
        setStyleSheet("background:#282828; color:white; font-family:Sans;");

        QVBoxLayout *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(40,40,40,40);
        rootLayout->setSpacing(20);
        rootLayout->setAlignment(Qt::AlignTop);

        // TITLE (Bluetooth size)
        QLabel *title = new QLabel("Ethernet", this);
        title->setStyleSheet("font-size:42px; font-weight:bold;");
        title->setAlignment(Qt::AlignCenter);
        rootLayout->addWidget(title);

        // -------------------------------------------------
        // CARD 1 — NETWORK MAP (Bluetooth card style)
        // -------------------------------------------------
        QFrame *mapCard = new QFrame(this);
        mapCard->setStyleSheet(
            "QFrame { background:#3a3a3a; border-radius:40px; }"
        );
        mapCard->setFixedHeight(300);

        QVBoxLayout *mapLay = new QVBoxLayout(mapCard);
        mapLay->setContentsMargins(25,25,25,25);

        QFrame *mapInner = new QFrame(this);
        mapInner->setStyleSheet(
            "QFrame { background:#444444; border-radius:22px; }"
        );

        QVBoxLayout *mapInnerLay = new QVBoxLayout(mapInner);
        mapInnerLay->setContentsMargins(20,20,20,20);

        QLabel *mapLbl = new QLabel(
            "Network map\n"
            "- Show network locations (click location to open\n"
            "  osm-files at that address)"
        );
        mapLbl->setStyleSheet("font-size:26px;");
        mapLbl->setAlignment(Qt::AlignCenter);
        mapLbl->setWordWrap(true);

        mapInnerLay->addWidget(mapLbl);
        mapLay->addWidget(mapInner);

        rootLayout->addWidget(mapCard);

        // -------------------------------------------------
        // CARD 2 — IP INFO
        // -------------------------------------------------
        QFrame *ipCard = new QFrame(this);
        ipCard->setStyleSheet(
            "QFrame { background:#3a3a3a; border-radius:30px; }"
        );
        ipCard->setFixedHeight(240);

        QVBoxLayout *ipLay = new QVBoxLayout(ipCard);
        ipLay->setContentsMargins(20,20,20,20);

        ipInfoLabel = new QLabel("Loading…");
        ipInfoLabel->setStyleSheet("font-size:26px;");
        ipInfoLabel->setAlignment(Qt::AlignCenter);
        ipInfoLabel->setWordWrap(true);

        ipLay->addWidget(ipInfoLabel);

        rootLayout->addWidget(ipCard);

        refreshIpInfo();

        // -------------------------------------------------
        // BUTTON ROW (Bluetooth identical)
        // -------------------------------------------------
        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->setSpacing(40);
        btnRow->setAlignment(Qt::AlignHCenter);

        powerButton = smallBtnEth("On");
        refreshButton = smallBtnEth("Refresh");

        btnRow->addWidget(powerButton);
        btnRow->addWidget(refreshButton);

        rootLayout->addLayout(btnRow);

        // -------------------------------------------------
        // ★ PIN BACK BUTTON TO BOTTOM
        // -------------------------------------------------
        rootLayout->addStretch();  // pushes below to bottom

        QPushButton *backButton = new QPushButton(QStringLiteral("❮"));
        backButton->setFixedSize(140,60);
        backButton->setStyleSheet(
            "QPushButton{ background:#444444; color:white; border-radius:16px; "
            "border:1px solid #222; font-size:34px; }"
            "QPushButton:hover{ background:#555; }"
            "QPushButton:pressed{ background:#333; }"
        );

        QHBoxLayout *backLay = new QHBoxLayout();
        backLay->addWidget(backButton, 0, Qt::AlignHCenter);
        rootLayout->addLayout(backLay);

        // -------------------------------------------------
        // CONNECTIONS
        // -------------------------------------------------
        connect(powerButton, &QPushButton::clicked, this, &EthernetPage::togglePower);
        connect(refreshButton, &QPushButton::clicked, this, &EthernetPage::refreshIpInfo);
        connect(backButton, &QPushButton::clicked, this, [this]{
            if (stackedWidget) stackedWidget->setCurrentIndex(0);
        });

        // INITIAL STATE
        ethernetPowered = isEthernetPowered();
        updatePowerButton();
    }

private:
    QStackedWidget *stackedWidget = nullptr;
    QLabel *ipInfoLabel = nullptr;
    QPushButton *powerButton = nullptr;
    QPushButton *refreshButton = nullptr;
    bool ethernetPowered = false;

    // -------------------------------------------------
    // POWER BUTTON (Bluetooth exact behaviour)
    // -------------------------------------------------
    void updatePowerButton()
    {
        if (ethernetPowered) {
            powerButton->setText("On");
            powerButton->setStyleSheet(
                "QPushButton { background:#444444; color:#7CFC00; border-radius:16px;"
                " border:1px solid #222; font-size:26px; font-weight:bold; padding:10px 24px; }"
                "QPushButton:hover{ background:#555; }"
                "QPushButton:pressed{ background:#333; }"
            );
        } else {
            powerButton->setText("Off");
            powerButton->setStyleSheet(
                "QPushButton { background:#444444; color:#CC6666; border-radius:16px;"
                " border:1px solid #222; font-size:26px; font-weight:bold; padding:10px 24px; }"
                "QPushButton:hover{ background:#555; }"
                "QPushButton:pressed{ background:#333; }"
            );
        }
    }

    void togglePower()
    {
        ethernetPowered = !ethernetPowered;
        setEthernetPowered(ethernetPowered);
        updatePowerButton();
        refreshIpInfo();
    }

    void refreshIpInfo()
    {
        QString ip      = runCommandEth("hostname -I | awk '{print $1}'");
        QString mask    = runCommandEth("ip -o -f inet addr show eth0 | awk '{print $4}' | cut -d/ -f2");
        QString dns     = runCommandEth("grep nameserver /etc/resolv.conf | awk '{print $2}' | head -n1");
        QString gateway = runCommandEth("ip route | grep default | awk '{print $3}'");

        if (ip.isEmpty()) ip="Unknown";
        if (mask.isEmpty()) mask="Unknown";
        if (dns.isEmpty()) dns="Unknown";
        if (gateway.isEmpty()) gateway="Unknown";

        ipInfoLabel->setText(
            "IP address:   " + ip + "\n"
            "Subnet mask:  " + mask + "\n"
            "DNS server:   " + dns + "\n"
            "Gateway:      " + gateway
        );
    }
};

// ---------------------------------------------------------
// Factory
// ---------------------------------------------------------

extern "C" QWidget* make_page(QStackedWidget *stack)
{
    return new EthernetPage(stack);
}
