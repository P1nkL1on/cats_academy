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

    s.set_room(1, 0, new splitter);
    s.set_room(2, 0, new mass_seller);

    s.set_room(3, 0, new seller);

    QApplication app(argc, argv);
    auto *te = new QTextBrowser;
    te->setReadOnly(true);
    te->showMaximized();

    ui_QTextEdit u(te);
    s.ui_ = &u;
    s.draw(u);

    QObject::connect(te, &QTextBrowser::anchorClicked, [&s, &u](const QUrl &url){
        const ui::signal btn = ui::signal(url.toString().toInt());
        switch (btn) {
        case ui::next_roll:
            u.set_flush_delay(200);
            break;
        case ui::next_roll_10:
            u.set_flush_delay(20);
            break;
        case ui::next_roll_100:
            u.set_flush_delay(2);
            break;
        default:
            break;
        }
        const bool ok = s.btn(btn);
        cout << "> button pressed '" << btn << "': " << (ok ? "OK" : "ignored") << "\n";
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
    std::uniform_int_distribution<int> roll(dh_d6_first, dh_d6_last);
    return dice_hash(roll(rng_));
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

bool room::level_up_upgrade(int u, state &s)
{
    return upgrades_[u].level_up(s);
}

void room::draw(ui &o, int x, int y) const
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
        {
            o.begin_button(ui::mk_room_action(x, y, ui::room_action_sell));
            o << "Sell";
            o.end_button();
            o.begin_button(ui::mk_room_action(x, y, ui::room_action_move_up));
            o << "Move up";
            o.end_button();
            o.begin_button(ui::mk_room_action(x, y, ui::room_action_move_down));
            o << "Move down";
            o.end_button();
            o.begin_button(ui::mk_room_action(x, y, ui::room_action_move_left));
            o << "Move before";
            o.end_button();
            o.begin_button(ui::mk_room_action(x, y, ui::room_action_move_right));
            o << "Move after";
            o.end_button();
        }
        o.end_paragraph();
        o.begin_list();
        for (auto i = upgrades_.begin(); i != upgrades_.end(); ++i)
            i.value().draw(o, ui::mk_signal(x, y, i.key()));
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
    auto rng = rng_;
    *this = state{};
    swap(rng, rng_);
}

void state::draw(ui &o) const
{
    o.begin_paragraph();
    o << "Gold: " << gold() << ui::gold;
    o << " Rolls: " << rolls;
    {
        o << " ";
        o.begin_button(ui::next_roll);
        o << "Next roll";
        o.end_button();
        o << " ";
        o.begin_button(ui::next_roll_10);
        o << "x10";
        o.end_button();
        o << " ";
        o.begin_button(ui::next_roll_100);
        o << "x100";
        o.end_button();
    }
    o.end_paragraph();

    o.begin_paragraph();
    o << "Dice pool: ";
    for (auto i = dice_count_.begin(); i != dice_count_.end(); ++i) {
        const dice_hash dh = i.key();
        int count = i.value();
        while (count--) {
            switch (dice(dh).type()) {
            case dt_d6:
                o << ui::symbol(ui::d6_first + dice(dh).value() - 1);
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
    for (auto x = rooms_.begin(); x != rooms_.end(); ++x) {
        o << "Priority: " << x.key();
        o.begin_list();
        for (auto y = x.value().begin(); y != x.value().end(); ++y)
            y.value()->draw(o, x.key(), y.key());
        o.end_list();
    }
    o.end_list();
    o.flush();
}

bool state::btn(ui::signal s)
{
    if (s == ui::next_roll) {
        next_roll();
        return true;
    }
    if (s == ui::next_roll_10) {
        int count = 10;
        while (count--)
            next_roll();
        return true;
    }
    if (s == ui::next_roll_100) {
        int count = 100;
        while (count--)
            next_roll();
        return true;
    }
    int x, y, u;
    if (ui::rd_room_upgrade(s, x, y, u)) {
        const shared<room> &r = qAsConst(rooms_)[x][y];
        const bool ok = r && r->level_up_upgrade(u, *this);
        if (ok && ui_)
            draw(*ui_);
        return ok;
    }
    if (ui::rd_room_action(s, x, y, u)) {
        bool ok = false;
        switch (u) {
        case ui::room_action_sell:
            ok = sell_room(x, y);
            break;
        case ui::room_action_move_left:
            ok = move_room(x, y, -1, 0);
            break;
        case ui::room_action_move_right:
            ok = move_room(x, y, +1, 0);
            break;
        case ui::room_action_move_up:
            ok = move_room(x, y, 0, -1);
            break;
        case ui::room_action_move_down:
            ok = move_room(x, y, 0, +1);
            break;
        default:
            assert(false && "unreachable");
            break;
        }
        if (ok && ui_)
            draw(*ui_);
        return ok;
    }
    return false;
}

bool state::move_room(int x, int y, int xmod, int ymod)
{
    while (xmod > 1) {
        if (!move_room(x++, y, 1, 0))
            return false;
        xmod--;
    }
    while (xmod < -1) {
        if (!move_room(x--, y, -1, 0))
            return false;
        xmod++;
    }
    while (ymod > 1) {
        if (!move_room(x, y++, 0, 1))
            return false;
        ymod--;
    }
    while (ymod < -1) {
        if (!move_room(x, y--, 0, -1))
            return false;
        ymod++;
    }
    assert(-1 <= xmod && xmod <= 1);
    assert(-1 <= ymod && ymod <= 1);
    if (!xmod && !ymod)
        return true;

    const auto &r = qAsConst(rooms_);
    if (ymod) {
        if (y + ymod > r[x].lastKey() || y + ymod < r[x].firstKey())
            return false;
        swap(rooms_[x][y], rooms_[x][y + ymod]);
        return true;
    }
    return false;
}

bool state::sell_room(int x, int y)
{
    if (!qAsConst(rooms_)[x][y])
        return false;

    rooms_[x].remove(y);
    for (int i = y + 1; i < 10; ++i) {
        if (rooms_[x].contains(i))
            rooms_[x].insert(i - 1, rooms_[x].take(i));
    }
    if (rooms_[x].isEmpty()) {
        rooms_.remove(x);
        for (int i = x + 1; i < 10; ++i) {
            if (rooms_.contains(i))
                rooms_.insert(i - 1, rooms_.take(i));
        }
    }
    // TODO: give player some money for it
    return true;
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

void upgrade::draw(ui &o, ui::signal s) const
{
    o.begin_upgrade();
    o << description << " (lvl " << level_;
    if (level_max_ > 0)
        o << "/" << level_max_;
    o << "): " << value() << " -> " << value_next();

    if (level_max_ == -1 || level_ < level_max_) {
        o << " ";
        o.begin_button(s);
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
        o << "$";
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

void ui_cmd::begin_button(signal s)
{
    push_scope(scope_btn);
    o << "<a href=\"" << s << "\">[";
}

void ui_cmd::end_button()
{
    o << "]</a>";
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
    o << "<" << scope_str(s) << ">";
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
        if (delay_ms_ > 0)
            QThread::msleep(delay_ms_);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void ui_QTextEdit::set_flush_delay(int ms)
{
    assert(ms >= 0);
    delay_ms_ = ms;
}

bool splitter::activate_(state &s)
{
    const dice_hash dh = s.has_dice(dice::mk_filter_value_grt(1));
    if (!dh)
        return false;

    s.inc_dice(dh, -1);
    s.inc_dice(dh_d6_first, dice(dh).value());
    return true;
}

void splitter::draw_info_(ui &o) const
{
    o << "splits a D6 with 2+ into multiple D6's with value 1";
}

mass_seller::mass_seller()
{
    add_upgrade(activates, upgrade(
                    4, new linear_growing_number(1),
                    10, new multiply_growing_number(1.5),
                    16, "Number of activations"
                ));
    add_upgrade(base_price, upgrade(
                    1, new linear_growing_number(1),
                    100, new multiply_growing_number(1.5),
                    -1, "Base price"
                ));
}

bool mass_seller::activate_(state &s)
{
    const dice_hash dh = s.has_dice(dice::filter_any);
    if (!dh)
        return false;

    s.inc_dice(dh, -1);
    s.inc_gold(upgrade_value_multiplier(base_price) + activates_);
    return true;
}

int mass_seller::activates_max_() const
{
    return upgrade_value_floor(activates);
}

void mass_seller::draw_info_(ui &o) const
{
    o << "sells a D6 for " << upgrade_value_multiplier(base_price) <<
         " + *" << activates_ << "* gold (each activation during roll increases cost by 1)";
}

ui::signal ui::mk_signal(int x, int y, int u)
{
    assert(0 <= x && x < max_room_x);
    assert(0 <= y && y < max_room_y);
    assert(0 <= u && u < max_room_signals);
    return signal((int)room_upgrade_first
                  + (max_room_y * max_room_signals) * x + max_room_signals * y + u);
}

ui::signal ui::mk_room_action(int x, int y, int u)
{
    assert(0 <= u && u < max_room_actions);
    return mk_signal(x, y, u + max_room_upgrades);
}

ui::signal ui::mk_room_upgrade(int x, int y, int u)
{
    assert(0 <= u && u < max_room_upgrades);
    return mk_signal(x, y, u + max_room_upgrades);
}

bool ui::rd_signal(signal s, int &x, int &y, int &u)
{
    if (s < room_upgrade_first || s > room_upgrade_last)
        return false;
    const int ss = s - room_upgrade_first;
    x = ss / (max_room_y * max_room_signals);
    y = (ss % (max_room_y * max_room_signals)) / max_room_signals;
    u = ss % max_room_signals;
    return true;
}

bool ui::rd_room_action(signal s, int &x, int &y, int &u)
{
    if (!rd_signal(s, x, y, u))
        return false;
    u -= max_room_upgrades;
    if (0 <= u && u < max_room_actions)
        return true;
    return false;
}

bool ui::rd_room_upgrade(signal s, int &x, int &y, int &u)
{
    if (!rd_signal(s, x, y, u))
        return false;
    if (0 <= u && u < max_room_upgrades)
        return true;
    return false;
}

}

