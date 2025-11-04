#include <main.h>

#include <QThread>
#include <QApplication>
#include <QDebug>

#include <cassert>
#include <algorithm>
#include <iostream>

using namespace std;

int main(int argc, char **argv)
{
    using namespace ca;
    state s;
    s.reset();

    s.set_room(0, 0, new herbalist);
    s.set_room(0, 1, new herbalist);
    s.set_room(0, 2, new herbalist);
    s.set_room(0, 3, new herbalist);

    s.set_room(10, 0, new seller);

    QApplication app(argc, argv);
    auto *te = new QTextBrowser;
    te->setReadOnly(true);
    te->showMaximized();

    ui_QTextEdit u(te);
    s.ui_ = &u;
    s.draw(u);

    QObject::connect(te, &QTextBrowser::anchorClicked, [&s, &u](const QUrl &url){
        const str btn = url.toString().toStdString();
        const bool ok = s.btn(btn);
        cout << "> button pressed '" << btn << "': " << (ok ? "OK" : "ignored") << '\n';
        cout.flush();
        if (!ok)
            s.draw(u); // since invalid link makes a QTextBrowser empty
    });
    return app.exec();
}

namespace ca {

dice::dice(dice_hash dh) : dh_(dh)
{
    assert(dh != dh_invalid);
}

dice_type dice::type() const
{
    if (dh_d6_first <= dh_ && dh_ <= dh_d6_last)
        return dt_d6;
    return dt_invalid;
}

int dice::value() const
{
    switch (type()) {
    case dt_d6:
        return dh_ - dh_d6_first + 1;
    case dt_invalid:
        break;
    }
    return dh_invalid;
}

herbalist::herbalist()
{
    add_upgrade(activates, upgrade(
                    1, new linear_growing_number(1),
                    200, new multiply_growing_number(1.5),
                    -1, "Number of activations"
                ));
}

bool herbalist::activate_(state &s)
{
    s.inc_dice(s.roll_d6(), 1);
    return true;
}

int herbalist::activates_max_() const
{
    return upgrade_value_floor(activates);
}

void herbalist::draw_info_(ui &o) const
{
    o << "generates a D6";
}

dice_hash state::roll_d6()
{
    return dice_hash(rng_.bounded(dh_d6_first, dh_d6_last));
}

bool state::inc_dice(dice_hash dh, int added)
{
    assert(added);
    int &stored = dice_count_[dh];
    if (added < 0 && -added > stored)
        return false;

    stored += added;
    return true;
}

bool state::inc_gold(int added)
{
    assert(added);
    if (added < 0 && -added > gold_)
        return false;

    gold_ += added;
    return true;
}

dice_hash state::has_dice(dice::filter f, int count) const
{
    assert(count >= 1);
    assert(f);
    for (auto i = dice_count_.begin(); i != dice_count_.end(); ++i) {
        const int d_count = i.value();
        if (d_count < count)
            continue;
        const dice_hash dh = i.key();
        if (!f(dh))
            continue;
        return dh;
    }
    return dh_invalid;
}

void state::set_room(int x, int y, room *r)
{
    assert(r);
    rooms_[x][y].reset(r);
}

seller::seller()
{
    add_upgrade(money_mult, upgrade(
                    1, new linear_growing_number(0.01),
                    10, new multiply_growing_number(1.5),
                    -1, "Gold output multiplier"
                ));
}

bool seller::activate_(state &s)
{
    const dice_hash dh = s.has_dice(dice::filter_any);
    if (!dh)
        return false;

    s.inc_dice(dh, -1);
    s.inc_gold(upgrade_value_multiplier(money_mult, dice(dh).value()));
    return true;
}

int seller::activates_max_() const
{
    return -1;
}

void seller::draw_info_(ui &o) const
{
    o << "sells any dice for it's value in gold";
}

bool room::activate(state &s)
{
    if (activates_max_() != -1 && activates_ >= activates_max_())
        return false;
    if (!activate_(s))
        return false;
    activates_++;
    return true;
}

void room::draw(ui &o) const
{
    o.begin_room();
    {
        o.begin_paragraph();
        {
            o << name_() << ": ";
            draw_info_(o);

            if (activates_max_() != -1) {
                const int left = activates_max_() - activates_;
                o << " (";
                switch(left) {
                case 0:
                    o << "no activations";
                    break;
                case 1:
                    o << "1 activation";
                    break;
                default:
                    o << left << " activations";
                    break;
                }
                o << " left)";
            }
        }
        o.end_paragraph();
//        {
//            o.begin_button();
//            o << "Sell";
//            o.end_button();
//        }
//        {
//            o.begin_button();
//            o << "Move up";
//            o.end_button();
//            o.begin_button();
//            o << "Move down";
//            o.end_button();
//            o.begin_button();
//            o << "Move before";
//            o.end_button();
//            o.begin_button();
//            o << "Move after";
//            o.end_button();
//        }
        o.begin_list();
        for (const upgrade &u : upgrades_)
            u.draw(o);
        o.end_list();
    }
    o.end_room();
}

void room::add_upgrade(int i, upgrade u)
{
    upgrades_.insert(i, u);
}

void state::next_roll()
{
    const list<int> xs = rooms_.keys();

    for (int x : xs) {
        const list<shared<room>> rooms = rooms_[x].values();
        if (!rooms.size())
            continue;

        for (;;) {
            const bool used = any_of(
                rooms.begin(), rooms.end(),
                [this](const shared<room> &r){ return r->activate(*this); });

            if (!used)
                break;
            if (ui_)
                draw(*ui_);
        }
    }

    for (const auto &ys : qAsConst(rooms_))
        for (const shared<room> &r : ys)
            r->activates_ = 0;

    rolls++;
    if (ui_)
        draw(*ui_);
}

void state::reset()
{
    *this = state{};
}

void state::draw(ui &o) const
{
    o.begin_paragraph();
    o << "Gold: " << gold() << ui::gold;
    o << " Rolls: " << rolls;
    o << " ";
    o.begin_button("next_roll");
    o << "Next roll";
    o.end_button();
    o.end_paragraph();

    o.begin_paragraph();
    o << "Dice pool: ";
    for (auto i = dice_count_.begin(); i != dice_count_.end(); ++i) {
        const dice_hash dh = i.key();
        int count = i.value();
        while (count--) {
            switch (dice(dh).type()) {
            case dt_d6:
                o << ui::symbol(ui::d6_first + dice(dh).value());
                break;
            default:
                o << ui::undefined;
                break;
            }
        }
    }
    o.end_paragraph();

    o << "Rooms: ";
    o.begin_list();
    for (const auto &ys : qAsConst(rooms_))
        for (const shared<room> &r : ys)
            r->draw(o);
    o.end_list();
    o.flush();
}

bool state::btn(str s)
{
    if (s == "next_roll") {
        next_roll();
        return true;
    }

    return false;
}

upgrade::upgrade(
        double v, growing_number *vadd,
        double p, growing_number *padd,
        int lvl_max, str description) :
    description(move(description)),
    value_(v), value_grow(vadd),
    price_(p), price_grow(padd),
    level_max_(lvl_max), level_(1)
{
    assert(vadd);
    assert(padd);
    assert(p >= 0.0);
    assert(lvl_max == -1 || lvl_max > 0);
}

bool upgrade::level_up(state &s)
{
    if (level_max_ != -1 && level_ >= level_max_)
        return false;
    if (!s.inc_gold(-price()))
        return false;
    level_++;
    value_ = value_next();
    price_ = price_grow ? price_grow->next(price_) : price_;
    return true;
}

void upgrade::draw(ui &o) const
{
    o.begin_upgrade();
    o << description << " (lvl " << level_ ;
    if (level_max_ > 0)
        o << '/' << level_max_;
    o << "): " << value() << " -> " << value_next();

    if (level_max_ == -1 || level_ < level_max_) {
        o << " ";
        o.begin_button("upgrade_uid=helloworld");
        o << "upgrade for " << price() << ui::gold;
        o.end_button();
    }
    o.end_upgrade();
}

double upgrade::value_next() const
{
    return value_grow ? value_grow->next(value_) : value_;
}

int upgrade::price() const
{
    return ceil(price_);
}

ui &ui_cmd::operator <<(int i)
{
    o << i;
    return *this;
}

ui &ui_cmd::operator <<(double d)
{
    o << d;
    return *this;
}

ui &ui_cmd::operator <<(str s)
{
    o << s;
    return *this;
}

ui &ui_cmd::operator <<(symbol s)
{
    if (s == gold)
        o << "[$]";
    else if (d6_first <= s && s <= d6_last)
        o << "[" << (s - d6_first + 1) << "]";
    else
        o << "[?]";
    return *this;
}

void ui_cmd::begin_room()
{
    push_scope(scope_room);
    o << "<li>";
}

void ui_cmd::end_room()
{
    o << "</li>";
    pop_scope(scope_room);
}

void ui_cmd::begin_upgrade()
{
    push_scope(scope_upgrade);
    o << "<li>";
}

void ui_cmd::end_upgrade()
{
    o << "</li>";
    pop_scope(scope_upgrade);
}

void ui_cmd::begin_button(str s)
{
    push_scope(scope_btn);
    o << "<a href=\"" << s << "\">";
}

void ui_cmd::end_button()
{
    o << "</a>";
    pop_scope(scope_btn);
}

void ui_cmd::begin_paragraph()
{
    push_scope(scope_p);
}

void ui_cmd::end_paragraph()
{
    pop_scope(scope_p);
}

void ui_cmd::begin_list()
{
    push_scope(scope_ul);
}

void ui_cmd::end_list()
{
    pop_scope(scope_ul);
}

str ui_cmd::scope_str(scope s)
{
    switch (s) {
    case scope_room:
        return "room";
    case scope_upgrade:
        return "upgrade";
    case scope_btn:
        return "btn";
    case scope_p:
        return "p";
    case scope_ul:
        return "ul";
    default:
        break;
    }
    assert(false && "unreachable");
    return "???";
}

void ui_cmd::push_scope(scope s)
{
    o << "\n";
    scopes_stack_.push_back(s);
    for (int i = 0; i < scopes_stack_.size(); ++i)
        o << "  ";
    o << '<' << scope_str(s) << ">";
}

void ui_cmd::pop_scope(scope s)
{
    o << "\n";
    for (int i = 0; i < scopes_stack_.size(); ++i)
        o << "  ";
    o << "</" << scope_str(s) << ">";
    assert(scopes_stack_.size() && scopes_stack_.last() == s);
    scopes_stack_.pop_back();
}

void ui_QTextEdit::flush()
{
    ui_cmd::flush();
    const str flushed = s_.str();
    s_.str("");
    if (te_) {
        te_->setHtml(QString::fromStdString(flushed));
        QThread::msleep(100);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

}

