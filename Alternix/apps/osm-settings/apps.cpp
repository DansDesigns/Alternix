#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScroller>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QStackedWidget>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfoList>
#include <functional>
#include <QSet>
#include <QScrollBar>
#include <QMouseEvent>
#include <QList>
#include <QMessageBox>

// =======================================================
// Global list of inner scroll areas (for hit testing)
// =======================================================
static QList<QScrollArea*> g_innerScrollAreas;

// Forward declare for refresh system
static std::function<void()> g_refreshAllLists;

// =======================================================
// Async command runner (non-blocking)
// =======================================================
static void runAsync(const QString &cmd, std::function<void(QString)> cb)
{
    QProcess *p = new QProcess;
    QObject::connect(
        p,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [p, cb](int, QProcess::ExitStatus) {
            QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
            cb(out);
            p->deleteLater();
        }
    );
    p->start("bash", {"-c", cmd});
}

// =======================================================
// Alternix Label Helper
// =======================================================
static QLabel* makeLabel(const QString &txt, int px, bool bold = false)
{
    QLabel *l = new QLabel(txt);
    QFont f("DejaVu Sans");
    f.setPointSize(px);
    f.setBold(bold);
    l->setFont(f);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet("color:white;");
    return l;
}

// =======================================================
// Row widget (app name + uninstall button)
// With ⏳ indicator during uninstall
// =======================================================
static QWidget* makeRow(const QString &name, std::function<void(QString, QPushButton*)> uninstallFn)
{
    QFrame *r = new QFrame;
    r->setStyleSheet("background:#3A3A3A; border-radius:24px;");
    QHBoxLayout *h = new QHBoxLayout(r);
    h->setContentsMargins(20, 14, 20, 14);

    QLabel *lbl = makeLabel(name, 22, false);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    h->addWidget(lbl);

    QPushButton *x = new QPushButton("❌");
    x->setStyleSheet(
        "QPushButton { background:transparent; color:#ff4444; font-size:32px; }"
        "QPushButton:hover { color:#ff1616; background:#ad1236; border-radius:18px; }"
        "QPushButton:pressed { color:#ffffff; background:#550000; border-radius:18px; }"
    );
    x->setFixedWidth(60);

    QObject::connect(x, &QPushButton::clicked, [=]() {
        x->setText("⏳");
        x->setEnabled(false);
        uninstallFn(name, x);
    });

    h->addWidget(x);
    return r;
}

// =======================================================
// Outer scroll area with manual drag scrolling
// (ignores drags that start on inner cards)
// =======================================================
class OuterScrollArea : public QScrollArea {
public:
    OuterScrollArea(QWidget *parent = nullptr)
        : QScrollArea(parent), dragging(false) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            lastPos = event->pos();
            dragging = !isOnInnerScroll(event->pos());
        }
        QScrollArea::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (dragging && (event->buttons() & Qt::LeftButton)) {
            int dy = event->pos().y() - lastPos.y();
            verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
            lastPos = event->pos();
            event->accept();
            return;
        }
        QScrollArea::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            dragging = false;
        QScrollArea::mouseReleaseEvent(event);
    }

private:
    bool dragging;
    QPoint lastPos;

    bool isOnInnerScroll(const QPoint &p)
    {
        QWidget *vp = viewport();
        for (QScrollArea *sa : g_innerScrollAreas) {
            if (!sa || !sa->viewport()) continue;

            QPoint topLeft = sa->viewport()->mapTo(vp, QPoint(0,0));
            QRect rect(topLeft, sa->viewport()->size());
            if (rect.contains(p))
                return true;
        }
        return false;
    }
};

// =======================================================
// Populate card list (6 rows visible max)
// =======================================================
static void populateList(QVBoxLayout *list, QScrollArea *area,
                         QStringList entries,
                         std::function<void(QString, QPushButton*)> uninstallFn)
{
    if (!list || !area) return;

    QLayoutItem *c;
    while ((c = list->takeAt(0))) {
        if (c->widget()) c->widget()->deleteLater();
        delete c;
    }

    if (entries.isEmpty()) {
        list->addWidget(makeLabel("No applications installed", 20));
    } else {
        for (const QString &e : entries)
            list->addWidget(makeRow(e, uninstallFn));
    }

    list->addStretch();

    int rowH = 72;
    int count = entries.size();
    int visible = qMin(count == 0 ? 1 : count, 6);

    area->setFixedHeight(rowH * visible);
}

// =======================================================
// Build card (inner scroll area)
// =======================================================
static QFrame* makeAppCard(const QString &title,
                           QVBoxLayout **outList,
                           QScrollArea **outScroll)
{
    QFrame *card = new QFrame;
    card->setStyleSheet("background:#444444; border-radius:30px;");

    QVBoxLayout *v = new QVBoxLayout(card);
    v->setContentsMargins(30, 30, 30, 30);
    v->setSpacing(20);

    QLabel *t = makeLabel(title, 26, true);
    v->addWidget(t);

    QScrollArea *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    if (title.startsWith("Snap")) sa->setObjectName("inner_scroll_snap");
    else if (title.startsWith("Flatpak")) sa->setObjectName("inner_scroll_flatpak");
    else if (title.startsWith("APT")) sa->setObjectName("inner_scroll_apt");
    else sa->setObjectName("inner_scroll_generic");

    QScroller::grabGesture(sa->viewport(), QScroller::LeftMouseButtonGesture);
    sa->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    QWidget *wrap = new QWidget;
    QVBoxLayout *list = new QVBoxLayout(wrap);
    list->setSpacing(14);
    list->setContentsMargins(0, 0, 0, 0);
    list->addWidget(makeLabel("Calculating..", 22));
    list->addStretch();

    sa->setWidget(wrap);
    v->addWidget(sa);

    g_innerScrollAreas.append(sa);

    *outList = list;
    *outScroll = sa;
    return card;
}

// =======================================================
// APT loader
// =======================================================
static void loadAPT(QVBoxLayout *list, QScrollArea *area)
{
    QStringList dirs;
    dirs << QDir::homePath() + "/.local/share/applications"
         << "/usr/share/applications";

    QStringList out;
    QSet<QString> seen;

    for (const QString &d : dirs) {
        QDir dir(d);
        if (!dir.exists()) continue;

        auto files = dir.entryInfoList({"*.desktop"}, QDir::Files);
        for (const QFileInfo &fi : files) {
            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly)) continue;

            QTextStream ts(&f);
            QString name, exec;

            while (!ts.atEnd()) {
                QString L = ts.readLine().trimmed();
                if (L.startsWith("Name=") && name.isEmpty()) name = L.mid(5);
                if (L.startsWith("Exec=") && exec.isEmpty()) exec = L.mid(5).split(" ").first();
                if (!name.isEmpty() && !exec.isEmpty()) break;
            }

            if (!exec.isEmpty() && !seen.contains(exec)) {
                seen.insert(exec);
                out << (name + " — " + exec);
            }
        }
    }

    out.sort(Qt::CaseInsensitive);

    auto uninstall = [=](QString label, QPushButton *btn) {
        QString exe = label;
        int i = label.lastIndexOf(" — ");
        if (i != -1) exe = label.mid(i + 3).trimmed();

        QString cmd =
            "exe=\"" + exe + "\";"
            "p=$(command -v \"$exe\" 2>/dev/null);"
            "[ -z \"$p\" ] && exit 0;"
            "pkg=$(dpkg -S \"$p\" 2>/dev/null | head -n1 | cut -d: -f1);"
            "[ -z \"$pkg\" ] && exit 0;"
            "sudo apt remove -y \"$pkg\"";

        runAsync(cmd, [=](QString) {
            QMessageBox::information(nullptr, "Uninstalled",
                                     label + " has been uninstalled");
            g_refreshAllLists();
        });
    };

    populateList(list, area, out, uninstall);
}

// =======================================================
// Flatpak loader
// =======================================================
static void loadFlatpak(QWidget *ctx,
                        QVBoxLayout *list, QScrollArea *area,
                        QVBoxLayout *aptL, QScrollArea *aptA)
{
    runAsync("flatpak list --app --columns=application 2>/dev/null",
    [=](QString out) {

        QStringList apps = out.split("\n", Qt::SkipEmptyParts);
        apps.sort(Qt::CaseInsensitive);

        auto uninstall = [=](QString p, QPushButton *btn) {
            runAsync("flatpak uninstall -y " + p, [=](QString) {
                QMessageBox::information(nullptr, "Uninstalled",
                                         p + " has been uninstalled");
                g_refreshAllLists();
            });
        };

        populateList(list, area, apps, uninstall);

        QTimer::singleShot(0, ctx, [=]() {
            loadAPT(aptL, aptA);
        });
    });
}

// =======================================================
// Snap loader
// =======================================================
static void loadSnap(QWidget *ctx,
                     QVBoxLayout *list, QScrollArea *area,
                     QVBoxLayout *flatL, QScrollArea *flatA,
                     QVBoxLayout *aptL,  QScrollArea *aptA)
{
    runAsync("snap list 2>/dev/null | tail -n +2 | awk '{print $1}'",
    [=](QString out) {

        QStringList apps = out.split("\n", Qt::SkipEmptyParts);
        apps.sort(Qt::CaseInsensitive);

        auto uninstall = [=](QString p, QPushButton *btn) {
            runAsync("sudo snap remove -y " + p, [=](QString) {
                QMessageBox::information(nullptr, "Uninstalled",
                                         p + " has been uninstalled");
                g_refreshAllLists();
            });
        };

        populateList(list, area, apps, uninstall);

        QTimer::singleShot(0, ctx, [=]() {
            loadFlatpak(ctx, flatL, flatA, aptL, aptA);
        });
    });
}

// =======================================================
// PAGE FACTORY
// =======================================================
extern "C" QWidget* make_page(QStackedWidget *stack)
{
    g_innerScrollAreas.clear();

    QWidget *root = new QWidget;
    root->setStyleSheet("background:#282828; color:white;");

    QVBoxLayout *rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(40, 40, 40, 40);
    rootLay->setSpacing(10);

    QLabel *title = new QLabel("Installed Apps");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:42px; font-weight:bold;");
    rootLay->addWidget(title);

    OuterScrollArea *scroll = new OuterScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *wrap = new QWidget;
    QVBoxLayout *wrapLay = new QVBoxLayout(wrap);
    wrapLay->setSpacing(20);
    wrapLay->setContentsMargins(0, 0, 0, 0);

    QFrame *outer = new QFrame;
    outer->setStyleSheet("background:#3a3a3a; border-radius:40px;");
    QVBoxLayout *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(50, 30, 50, 30);
    outerLay->setSpacing(30);

    QVBoxLayout *snapL;  QScrollArea *snapA;
    QVBoxLayout *flatL;  QScrollArea *flatA;
    QVBoxLayout *aptL;   QScrollArea *aptA;

    outerLay->addWidget(makeAppCard("Snap Applications",   &snapL, &snapA));
    outerLay->addWidget(makeAppCard("Flatpak Applications",&flatL, &flatA));
    outerLay->addWidget(makeAppCard("APT Installed Apps",  &aptL, &aptA));

    wrapLay->addWidget(outer);
    wrapLay->addStretch();

    scroll->setWidget(wrap);
    rootLay->addWidget(scroll);

    // REFRESH SYSTEM
    g_refreshAllLists = [=]() {
        g_innerScrollAreas.clear();
        loadSnap(root, snapL, snapA, flatL, flatA, aptL, aptA);
    };

    // BACK BUTTON
    QPushButton *back = new QPushButton("❮");
    back->setStyleSheet(
        "QPushButton { background:#444; border-radius:20px; "
        "font-size:32px; font-weight:bold; padding:10px 20px; }"
        "QPushButton:hover { background:#555; }"
        "QPushButton:pressed { background:#333; }"
    );
    back->setFixedSize(140, 60);
    QObject::connect(back, &QPushButton::clicked, [stack]() {
        stack->setCurrentIndex(0);
    });
    rootLay->addWidget(back, 0, Qt::AlignCenter);

    // INITIAL LOAD
    QTimer::singleShot(0, root, [=]() {
        g_refreshAllLists();
    });

    return root;
}
