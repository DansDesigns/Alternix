#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QFrame>
#include <QTimer>
#include <QStackedWidget>
#include <QResizeEvent>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QtMath>
#include <algorithm>   // for std::max_element

// -----------------------------------------------------
// Alternix helpers
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
// Line chart widget with 0 / 50 / 100 markers
// -----------------------------------------------------
class BatteryChartWidget : public QWidget
{
public:
    explicit BatteryChartWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setSamples(const QVector<double> &values, double minVal, double maxVal)
    {
        m_values = values;
        m_min = minVal;
        m_max = maxVal;
        if (m_max <= m_min)
            m_max = m_min + 1.0;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRect r = rect().adjusted(16, 12, -16, -16);

        // background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#2a2a2a"));
        p.drawRoundedRect(r, 18, 18);

        if (m_values.isEmpty())
            return;

        int leftAxisPad = 40;
        QRect chartRect = r.adjusted(leftAxisPad, 10, -10, -20);

        // axes
        p.setPen(QPen(QColor("#666666"), 1));
        p.drawLine(chartRect.bottomLeft(), chartRect.bottomRight());
        p.drawLine(chartRect.bottomLeft(), chartRect.topLeft());

        // Y-axis labels 0, 50, 100
        p.setPen(QColor("#bbbbbb"));
        QFont f = p.font();
        f.setPointSize(16);
        p.setFont(f);

        auto drawY = [&](int percent) {
            double norm = (percent - m_min) / (m_max - m_min);
            if (norm < 0.0) norm = 0.0;
            if (norm > 1.0) norm = 1.0;
            int y = chartRect.bottom() - norm * chartRect.height();
            p.drawText(r.left() + 6, y + 6, QString::number(percent));
            p.setPen(QPen(QColor("#444444"), 1, Qt::DashLine));
            p.drawLine(chartRect.left(), y, chartRect.right(), y);
            p.setPen(QPen(QColor("#bbbbbb"), 1));
        };

        drawY(0);
        drawY(50);
        drawY(100);

        // line
        if (m_values.size() >= 2) {
            QPainterPath path;
            int n = m_values.size();

            auto toY = [&](double v) {
                double norm = (v - m_min) / (m_max - m_min);
                if (norm < 0.0) norm = 0.0;
                if (norm > 1.0) norm = 1.0;
                return chartRect.bottom() - norm * chartRect.height();
            };

            double step = (n > 1) ? double(chartRect.width()) / double(n - 1) : 0.0;
            path.moveTo(chartRect.left(), toY(m_values[0]));
            for (int i = 1; i < n; ++i)
                path.lineTo(chartRect.left() + step * i, toY(m_values[i]));

            QPen pen(QColor("#4da3ff"));
            pen.setWidth(3);
            p.setPen(pen);
            p.drawPath(path);
        }
    }

private:
    QVector<double> m_values;
    double m_min = 0.0;
    double m_max = 100.0;
};

// -----------------------------------------------------
// Battery helpers
// -----------------------------------------------------
struct BatteryStats {
    bool   valid       = false;
    int    capacityPct = -1;
    double voltageV    = qQNaN();
    double currentA    = qQNaN();
    double powerW      = qQNaN();
    double tempC       = qQNaN();
    double healthPct   = qQNaN();
    QString status;
};

static QString findBatteryPath()
{
    QDir dir("/sys/class/power_supply");
    QStringList ents = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &e : ents) {
        QFile f(dir.filePath(e + "/type"));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QString t = QString::fromLatin1(f.readAll().trimmed());
        if (t.compare("Battery", Qt::CaseInsensitive) == 0)
            return dir.filePath(e);
    }
    return QString();
}

static bool readDoubleFile(const QString &path, double &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    bool ok = false;
    out = QString::fromLatin1(f.readAll().trimmed()).toDouble(&ok);
    return ok;
}

static bool readIntFile(const QString &path, int &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    bool ok = false;
    out = QString::fromLatin1(f.readAll().trimmed()).toInt(&ok);
    return ok;
}

static QString readStringFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(f.readAll().trimmed());
}

static BatteryStats readBattery(const QString &basePath)
{
    BatteryStats s;
    if (basePath.isEmpty())
        return s;

    readIntFile(basePath + "/capacity", s.capacityPct);

    s.status = readStringFile(basePath + "/status");

    double v = 0.0;
    if (readDoubleFile(basePath + "/voltage_now", v))
        s.voltageV = v / 1000000.0;

    double c = 0.0;
    bool haveCurrent = readDoubleFile(basePath + "/current_now", c);
    if (haveCurrent)
        s.currentA = c / 1000000.0;

    double p = 0.0;
    if (readDoubleFile(basePath + "/power_now", p)) {
        s.powerW = p / 1000000.0;
        if (!haveCurrent && !qIsNaN(s.voltageV) && s.voltageV > 0.1)
            s.currentA = s.powerW / s.voltageV;
    } else {
        if (haveCurrent && !qIsNaN(s.voltageV) && s.voltageV > 0.1)
            s.powerW = s.voltageV * s.currentA;
    }

    double tempRaw = 0.0;
    if (readDoubleFile(basePath + "/temp", tempRaw)) {
        if (tempRaw > 2000.0)
            s.tempC = tempRaw / 1000.0;
        else if (tempRaw > 200.0)
            s.tempC = tempRaw / 10.0;
        else
            s.tempC = tempRaw;
    }

    double full = 0.0, design = 0.0;
    if (readDoubleFile(basePath + "/energy_full", full) &&
        readDoubleFile(basePath + "/energy_full_design", design) &&
        design > 0.0) {
        s.healthPct = (full / design) * 100.0;
    } else if (readDoubleFile(basePath + "/charge_full", full) &&
               readDoubleFile(basePath + "/charge_full_design", design) &&
               design > 0.0) {
        s.healthPct = (full / design) * 100.0;
    }

    s.valid = (s.capacityPct >= 0);
    return s;
}

static QString formatDouble(double v, int decimals)
{
    if (qIsNaN(v))
        return "Unknown";
    return QString::number(v, 'f', decimals);
}

// -----------------------------------------------------
// BatteryPage
// -----------------------------------------------------
class BatteryPage : public QWidget
{
public:
    explicit BatteryPage(QStackedWidget *stack)
        : QWidget(stack), m_stack(stack)
    {
        // Global font / colour override
        setStyleSheet("background:#282828; color:white; font-family:Sans;");
        QFont f = font();
        f.setFamily("Sans");
        setFont(f);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(40, 40, 40, 40);
        root->setSpacing(10);

        QLabel *title = new QLabel("Battery");
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size:42px; font-weight:bold;");
        root->addWidget(title);

        // Scroll area
        QScrollArea *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);

        m_wrap = new QWidget(scroll);
        QVBoxLayout *wrapLay = new QVBoxLayout(m_wrap);
        wrapLay->setSpacing(10);
        wrapLay->setContentsMargins(0,0,0,0);

        // Outer big card
        m_outer = new QFrame(m_wrap);
        m_outer->setStyleSheet("QFrame { background:#3a3a3a; border-radius:40px; }");
        QVBoxLayout *outerLay = new QVBoxLayout(m_outer);
        outerLay->setContentsMargins(50, 30, 50, 30);
        outerLay->setSpacing(30);

        // Cards based on your layout
        outerLay->addWidget(createCard("Battery health",  &m_healthChart,    &m_healthLabel));
        outerLay->addWidget(createCard("Discharge rate",  &m_dischargeChart, &m_dischargeLabel));
        outerLay->addWidget(createCard("Charge rate",     &m_chargeChart,    &m_chargeLabel));

        // Stats card
        QFrame *statsCard = new QFrame(m_outer);
        statsCard->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");
        QVBoxLayout *statsLay = new QVBoxLayout(statsCard);
        statsLay->setContentsMargins(30, 30, 30, 30);
        statsLay->setSpacing(16);

        QLabel *statsTitle = new QLabel("Stats");
        statsTitle->setAlignment(Qt::AlignCenter);
        statsTitle->setStyleSheet("font-size:30px; font-weight:bold;");
        statsLay->addWidget(statsTitle);

        m_statsLabel = new QLabel("Battery information not available");
        m_statsLabel->setAlignment(Qt::AlignCenter);
        m_statsLabel->setStyleSheet(
            "QLabel { background:#383838; border-radius:18px;"
            " font-size:24px; padding:12px 24px; }"
        );
        m_statsLabel->setWordWrap(true);
        statsLay->addWidget(m_statsLabel);

        outerLay->addWidget(statsCard);

        wrapLay->addWidget(m_outer, 0, Qt::AlignHCenter);
        wrapLay->addStretch();

        scroll->setWidget(m_wrap);
        root->addWidget(scroll);

        // Back button pinned to bottom
        QPushButton *back = makeBtn("❮");
        back->setFixedSize(140, 60);
        connect(back, &QPushButton::clicked, this, [this]() {
            if (m_stack)
                m_stack->setCurrentIndex(0);
        });
        root->addWidget(back, 0, Qt::AlignCenter);

        // Timer: 2s refresh
        m_timer = new QTimer(this);
        m_timer->setInterval(2000);
        connect(m_timer, &QTimer::timeout, this, &BatteryPage::refreshBattery);

        if (m_stack) {
            connect(m_stack, &QStackedWidget::currentChanged,
                    this, [this](int idx) {
                if (m_stack->widget(idx) == this)
                    m_timer->start();
                else
                    m_timer->stop();
            });
            if (m_stack->currentWidget() == this)
                m_timer->start();
        } else {
            m_timer->start();
        }

        m_batteryPath = findBatteryPath();
        refreshBattery();
    }

protected:
    void resizeEvent(QResizeEvent *ev) override
    {
        QWidget::resizeEvent(ev);
        int w = ev->size().width();
        int sidePadding = 80;              // 40px each side, to match root margins

        // If screen width > 720, expand cards to maintain edge padding.
        // For smaller widths, shrink but keep at least 320px.
        int innerWidth;
        if (w > 720)
            innerWidth = qMax(320, w - sidePadding);
        else
            innerWidth = qMax(320, w - sidePadding);

        m_outer->setMinimumWidth(innerWidth);
        m_outer->setMaximumWidth(innerWidth);
    }

private:
    QStackedWidget *m_stack = nullptr;

    QWidget *m_wrap         = nullptr;
    QFrame  *m_outer        = nullptr;

    BatteryChartWidget *m_healthChart     = nullptr;
    BatteryChartWidget *m_dischargeChart  = nullptr;
    BatteryChartWidget *m_chargeChart     = nullptr;

    QLabel *m_healthLabel    = nullptr;
    QLabel *m_dischargeLabel = nullptr;
    QLabel *m_chargeLabel    = nullptr;
    QLabel *m_statsLabel     = nullptr;

    QString m_batteryPath;
    QTimer *m_timer = nullptr;

    QVector<double> m_healthHistory;
    QVector<double> m_dischargeHistory;
    QVector<double> m_chargeHistory;

    // UI helper: create a card with title + chart + value label
    QFrame* createCard(const QString &titleText,
                       BatteryChartWidget **chartOut,
                       QLabel **valueLabelOut)
    {
        QFrame *card = new QFrame();
        card->setStyleSheet("QFrame { background:#444444; border-radius:30px; }");
        QVBoxLayout *lay = new QVBoxLayout(card);
        lay->setContentsMargins(30, 30, 30, 30);
        lay->setSpacing(16);

        QLabel *title = new QLabel(titleText);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size:30px; font-weight:bold;");
        title->setWordWrap(true);
        lay->addWidget(title);

        BatteryChartWidget *chart = new BatteryChartWidget(card);
        lay->addWidget(chart);

        QLabel *valueLbl = new QLabel("No data");
        valueLbl->setAlignment(Qt::AlignCenter);
        valueLbl->setStyleSheet(
            "QLabel { background:#383838; border-radius:18px;"
            " font-size:24px; padding:10px 24px; }"
        );
        valueLbl->setWordWrap(true);
        lay->addWidget(valueLbl);

        if (chartOut)      *chartOut      = chart;
        if (valueLabelOut) *valueLabelOut = valueLbl;

        return card;
    }

    static void pushSample(QVector<double> &vec, double v, int maxPoints = 60)
    {
        if (qIsNaN(v))
            return;
        vec.append(v);
        if (vec.size() > maxPoints)
            vec.remove(0, vec.size() - maxPoints);
    }

    static void findMinMax(const QVector<double> &vec, double &minOut, double &maxOut)
    {
        if (vec.isEmpty()) {
            minOut = 0.0;
            maxOut = 1.0;
            return;
        }
        double mn = vec[0];
        double mx = vec[0];
        for (double v : vec) {
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        if (mn == mx) {
            mx = mn + 1.0;
        }
        minOut = mn;
        maxOut = mx;
    }

    void refreshBattery()
    {
        if (m_batteryPath.isEmpty())
            m_batteryPath = findBatteryPath();

        BatteryStats st = readBattery(m_batteryPath);

        if (!st.valid) {
            m_healthLabel->setText("No battery detected");
            m_dischargeLabel->setText("No battery detected");
            m_chargeLabel->setText("No battery detected");
            m_statsLabel->setText("Battery information not available");
            m_healthChart->setSamples(QVector<double>(), 0, 1);
            m_dischargeChart->setSamples(QVector<double>(), 0, 1);
            m_chargeChart->setSamples(QVector<double>(), 0, 1);
            return;
        }

        // -------- Battery health (RESTORED behaviour) --------
        double healthVal = qIsNaN(st.healthPct) ? double(st.capacityPct) : st.healthPct;
        pushSample(m_healthHistory, healthVal);
        double hMin, hMax;
        findMinMax(m_healthHistory, hMin, hMax);
        // Clamp around 0..110 so we can still show 0/50/100 nicely
        hMin = qMin(hMin, 0.0);
        hMax = qMax(hMax, 110.0);
        m_healthChart->setSamples(m_healthHistory, hMin, hMax);

        m_healthLabel->setText(
            QString("Health: %1 %\nCurrent capacity: %2 %")
                .arg(qIsNaN(st.healthPct)
                         ? QString("Unknown")
                         : QString::number(st.healthPct, 'f', 1))
                .arg(st.capacityPct)
        );

        // -------- Charge / discharge --------
        double dischargeRate = 0.0;
        double chargeRate = 0.0;

        if (!qIsNaN(st.powerW)) {
            if (st.status.compare("Discharging", Qt::CaseInsensitive) == 0)
                dischargeRate = st.powerW;
            else if (st.status.compare("Charging", Qt::CaseInsensitive) == 0)
                chargeRate = st.powerW;
        } else if (!qIsNaN(st.currentA)) {
            if (st.status.compare("Discharging", Qt::CaseInsensitive) == 0)
                dischargeRate = qAbs(st.currentA);
            else if (st.status.compare("Charging", Qt::CaseInsensitive) == 0)
                chargeRate = qAbs(st.currentA);
        }

        pushSample(m_dischargeHistory, dischargeRate);
        pushSample(m_chargeHistory,    chargeRate);

        double dMin, dMax, cMin, cMax;
        findMinMax(m_dischargeHistory, dMin, dMax);
        findMinMax(m_chargeHistory,    cMin, cMax);

        dMin = 0.0;
        cMin = 0.0;
        dMax = qMax(dMax, 1.0);
        cMax = qMax(cMax, 1.0);

        m_dischargeChart->setSamples(m_dischargeHistory, dMin, dMax);
        m_chargeChart->setSamples(m_chargeHistory,       cMin, cMax);

        m_dischargeLabel->setText(
            QString("Instant discharge rate: %1")
                .arg(dischargeRate <= 0.0
                         ? QString("0")
                         : (!qIsNaN(st.powerW)
                                ? QString("%1 W").arg(dischargeRate, 0, 'f', 2)
                                : QString("%1 A").arg(dischargeRate, 0, 'f', 3)))
        );

        m_chargeLabel->setText(
            QString("Instant charge rate: %1")
                .arg(chargeRate <= 0.0
                         ? QString("0")
                         : (!qIsNaN(st.powerW)
                                ? QString("%1 W").arg(chargeRate, 0, 'f', 2)
                                : QString("%1 A").arg(chargeRate, 0, 'f', 3)))
        );

        // -------- Stats card --------
        QString currentText;
        if (!qIsNaN(st.currentA)) {
            double a = qAbs(st.currentA);
            QString dir;
            if (st.status.compare("Discharging", Qt::CaseInsensitive) == 0)
                dir = " (discharging)";
            else if (st.status.compare("Charging", Qt::CaseInsensitive) == 0)
                dir = " (charging)";
            currentText = QString("%1 A%2").arg(a, 0, 'f', 3).arg(dir);
        } else {
            currentText = "Unknown";
        }

        QString powerText = qIsNaN(st.powerW)
                                ? QString("Unknown")
                                : QString("%1 W").arg(st.powerW, 0, 'f', 2);

        QString tempText = qIsNaN(st.tempC)
                               ? QString("Unknown")
                               : QString("%1 °C").arg(st.tempC, 0, 'f', 1);

        QString healthTxt = qIsNaN(st.healthPct)
                                ? QString("Unknown")
                                : QString("%1 %").arg(st.healthPct, 0, 'f', 1);

        QString stats =
            QString("Status: %1\n"
                    "Charge level: %2 %\n"
                    "Voltage: %3 V\n"
                    "Current: %4\n"
                    "Power: %5\n"
                    "Temperature: %6\n"
                    "Pack health: %7")
                .arg(st.status.isEmpty() ? "Unknown" : st.status)
                .arg(st.capacityPct >= 0 ? QString::number(st.capacityPct) : "Unknown")
                .arg(formatDouble(st.voltageV, 3))
                .arg(currentText)
                .arg(powerText)
                .arg(tempText)
                .arg(healthTxt);

        m_statsLabel->setText(stats);
    }
};

// -----------------------------------------------------
// Factory for osm-settings
// -----------------------------------------------------
extern "C" QWidget* make_page(QStackedWidget *stack)
{
    return new BatteryPage(stack);
}
