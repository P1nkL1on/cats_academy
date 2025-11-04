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
                    8, "Number of activations"
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

void herbalist::draw_info(ui &o) const
{
    o << "generates a D6";
}

state::state()
{
    shop_ = {
        shared<room>(new herbalist),
        shared<room>(new splitter),
        shared<room>(new seller),
        shared<room>(new mass_seller),
        shared<room>(new panacea),
    };

    insert_room(0, new herbalist);
    insert_room(1, new herbalist);
    insert_room(2, new herbalist);
    insert_room(3, new herbalist);
    insert_room(4, new seller);
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
    if (added < 0 && -added > gold_)
        return false;

    gold_ += added;
    return true;
}

dice_hash state::has_dice(dice::filter f, int count, dice::comparer c) const
{
    assert(count >= 1);
    assert(f);
    dice_hash best = dh_invalid;
    for (auto i = dice_count_.begin(); i != dice_count_.end(); ++i) {
        const int d_count = i.value();
        if (d_count < count)
            continue;
        const dice_hash dh = i.key();
        if (!f(dh))
            continue;
        if (!c)
            return dh;
        if (best == dh_invalid || c(dh, best))
            best = dh;
    }
    return best;
}

void state::insert_room(int pos, room *r)
{
    assert(r);
    rooms_.insert(pos, shared<room>(r));
}

seller::seller()
{
    add_upgrade(money_mult, upgrade(
                    1, new linear_growing_number(0.1),
                    100, new multiply_growing_number(1.5),
                    10, "Gold output multiplier"
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

void seller::draw_info(ui &o) const
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

void room::draw(ui &o, int r) const
{
    o.begin_room();
    {
        o.begin_paragraph();
        {
            o << name() << ", " << level();
            o << ": ";
            draw_info(o);

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
        o.nl();
        if (price() >= 0) {
            o.begin_button(ui::mk_room_action(r, ui::room_action_sell));
            o << "Sell for " << (price() * level()) << ui::gold;
            o.end_button();
        }
        {
            // TODO: add duplicate (for full price)

            o.begin_button(ui::mk_room_action(r, ui::room_action_move_up));
            o << "Move up";
            o.end_button();
            o.begin_button(ui::mk_room_action(r, ui::room_action_move_down));
            o << "Move down";
            o.end_button();
        }
        o.end_paragraph();
        o.begin_list();
        for (auto i = upgrades_.begin(); i != upgrades_.end(); ++i)
            i.value().draw(o, ui::mk_signal(r, i.key()));
        o.end_list();
    }
    o.end_room();
}

int room::level() const
{
    int lvl = 1;
    for (const upgrade &u : upgrades_)
        lvl += u.level();
    return lvl;
}

void room::add_upgrade(int i, upgrade u)
{
    upgrades_.insert(i, u);
}

void state::next_roll()
{
    if (state_ != gaming)
        return;

    for (;;) {
        const bool used = any_of(
            rooms_.begin(), rooms_.end(),
            [this](const shared<room> &r){ return r->activate(*this); });

        if (!used)
            break;
        if (ui_)
            draw(*ui_);
    }
    for (const shared<room> &r : qAsConst(rooms_))
        r->activates_ = 0;

    rolls++;
    switch (rolls) {
    case 100:
        insert_room(rooms_.size(), new debt_collector(2000));
        break;
    case 200:
        insert_room(rooms_.size(), new debt_collector(5000));
        break;
    case 300:
        insert_room(rooms_.size(), new debt_collector(10000));
        break;
    default:
        break;
    }
}

void state::reset()
{
    auto rng = rng_;
    auto *ui = ui_;
    *this = state{};
    swap(rng, rng_);
    swap(ui, ui_);
}

void state::draw(ui &o) const
{
    if (state_ != gaming) {
        o.begin_paragraph();
        switch (state_) {
        case lost_by_debt:
            o << "Game Over: lost by multiple debts at once";
            break;
        case won_by_panacea:
            o << "Game Over: won by Panacea";
            break;
        default:
            break;
        }
        o.begin_button(ui::restart);
        o << "Restart";
        o.end_button();
        o.end_paragraph();
    }
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
        o.begin_button(ui::restart);
        o << "Restart";
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
    for (int i = 0; i < rooms_.size(); ++i)
        rooms_[i]->draw(o, i);

    o.end_list();

    o << "Buy new: ";
    o.begin_list();
    for (int i = 0; i < shop_.size(); ++i) {
        o.begin_room();
        o << shop_[i]->name() + ", " << shop_[i]->level() << ": ";
        shop_[i]->draw_info(o);
        o << " ";
        o.begin_button(ui::mk_room_buy(i));
        o << "Buy for " << shop_[i]->price() << ui::gold;
        o.end_button();
        o.end_room();
    }
    o.end_list();
    o.flush();
}

bool state::btn(ui::signal s)
{
    struct update_on_exit
    {
        ~update_on_exit() { if (ui_) s_->draw(*ui_); }
        state *s_ = nullptr;
        ui *ui_ = nullptr;
    } exit { this, ui_ };

    if (s == ui::restart) {
        reset();
        return true;
    }
    if (s == ui::next_roll) {
        next_roll();
        return true;
    }
    if (s == ui::next_roll_10) {
        next_roll();
        int count = 9;
        while (count-- && rolls % 100)
            next_roll();
        return true;
    }
    if (s == ui::next_roll_100) {
        next_roll();
        int count = 99;
        while (count-- && rolls % 100)
            next_roll();
        return true;
    }
    int r, u;
    if (ui::rd_room_upgrade(s, r, u)) {
        const shared<room> &room = rooms_[r];
        return room && room->level_up_upgrade(u, *this);
    }
    if (ui::rd_room_action(s, r, u)) {
        bool ok = false;
        switch (u) {
        case ui::room_action_sell:
            ok = sell_room(r);
            break;
        case ui::room_action_move_up:
            ok = move_room(r, -1);
            break;
        case ui::room_action_move_down:
            ok = move_room(r, +1);
            break;
        default:
            assert(false && "unreachable");
            break;
        }
        return ok;
    }
    if (ui::rd_room_buy(s, u))
        return buy_room(u);

    return false;
}

bool state::move_room(int r, int mod)
{
    while (mod > 1) {
        if (!move_room(r++, 1))
            return false;
        mod--;
    }
    while (mod < -1) {
        if (!move_room(r--, -1))
            return false;
        mod++;
    }
    assert(-1 <= mod && mod <= 1);
    if (!mod)
        return true;

    if (r + mod > rooms_.size() || r + mod < 0)
        return false;
    swap(rooms_[r], rooms_[r + mod]);
    return true;
}

bool state::sell_room(int r)
{
    if (r < 0 || r >= rooms_.size())
        return false;
    inc_gold(rooms_[r]->price() * rooms_[r]->level());
    rooms_.removeAt(r);
    return true;
}

bool state::buy_room(int u)
{
    if (u < 0 || u >= shop_.size())
        return false;

    const shared<room> &buying = shop_.at(u);
    if (!inc_gold(-buying->price()))
        return false;

    // place the room next to rooms with the same name (or in the end if not possible)
    int i = rooms_.size();
    while (i > 0) {
        if (rooms_[i - 1]->name() == buying->name())
            break;
        --i;
    }
    if (i == 0)
        i = rooms_.size();
    insert_room(i, buying->duplicate());
    return true;
}

upgrade::upgrade(
        double v, growing_number *vadd,
        double p, growing_number *padd,
        int lvl_max, str description) :
    description(move(description)),
    value_(v), value_grow(vadd),
    price_(p), price_grow(padd),
    level_max_(lvl_max)
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
    o << description << " (Lvl " << level_;
    if (level_max_ > 0)
        o << "/" << level_max_;
    o << "): ";
    if (level_max_ != 0 && level_ < level_max_)
        o << value() << " -> " << value_next();
    else
        o << value() << " (MAX)";

    if (level_max_ == -1 || level_ < level_max_) {
        o << " ";
        o.begin_button(s);
        o << "Upgrade for " << price() << ui::gold;
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

splitter::splitter()
{
    add_upgrade(max_split_count, upgrade(
                    2, new linear_growing_number(1),
                    100, new multiply_growing_number(2),
                    4, "Max split dice count"
                ));
}

bool splitter::activate_(state &s)
{
    auto greater_than_2 = [](dice d){ return d.value() > 2; };
    auto split_count = upgrade_value_floor(max_split_count);
    auto best_to_split = [split_count](dice my, dice best) {
        return my.value() <= split_count && my.value() > best.value();
    };

    const dice_hash dh = s.has_dice(greater_than_2, 1, best_to_split);
    if (!dh)
        return false;

    s.inc_dice(dh, -1);
    s.inc_dice(dh_d6_first, min(split_count, dice(dh).value()));
    return true;
}

void splitter::draw_info(ui &o) const
{
    o << "splits a D6 with 2+ into up to "
      << upgrade_value_floor(max_split_count)
      << " D6's with value 1";
}

mass_seller::mass_seller()
{
    add_upgrade(activates, upgrade(
                    4, new linear_growing_number(1),
                    10, new multiply_growing_number(1.5),
                    5, "Number of activations"
                ));
    add_upgrade(base_price, upgrade(
                    1, new linear_growing_number(1),
                    100, new multiply_growing_number(1.5),
                    5, "Base price"
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

void mass_seller::draw_info(ui &o) const
{
    o << "sells a D6 for " << upgrade_value_multiplier(base_price);
    if (activates_)
        o << " + *" << activates_ << "*";
    o << " gold (each activation during roll increases cost by 1)";
}

ui::signal ui::mk_signal(int r, int u)
{
    assert(0 <= r && r < max_rooms);
    assert(0 <= u && u < max_room_signals);
    return signal((int)room_upgrade_first + max_room_signals * r + u);
}

ui::signal ui::mk_room_action(int r, int u)
{
    assert(0 <= u && u < max_room_actions);
    return mk_signal(r, u + max_room_upgrades);
}

ui::signal ui::mk_room_upgrade(int r, int u)
{
    assert(0 <= u && u < max_room_upgrades);
    return mk_signal(r, u + max_room_upgrades);
}

ui::signal ui::mk_room_buy(int s)
{
    signal r = signal(room_buy_first + s);
    assert(r <= room_buy_last);
    return r;
}

bool ui::rd_signal(signal s, int &r, int &u)
{
    if (s < room_upgrade_first || s > room_upgrade_last)
        return false;
    const int ss = s - room_upgrade_first;
    r = ss / max_room_signals;
    u = ss % max_room_signals;
    return true;
}

bool ui::rd_room_action(signal s, int &r, int &u)
{
    if (!rd_signal(s, r, u))
        return false;
    u -= max_room_upgrades;
    if (0 <= u && u < max_room_actions)
        return true;
    return false;
}

bool ui::rd_room_upgrade(signal s, int &r, int &u)
{
    if (!rd_signal(s, r, u))
        return false;
    if (0 <= u && u < max_room_upgrades)
        return true;
    return false;
}

bool ui::rd_room_buy(signal s, int &b)
{
    if (s < room_buy_first || s > room_buy_last)
        return false;
    b = (s - room_buy_first);
    return true;
}

debt_collector::debt_collector(int waits_gold) :
    waits_gold_(waits_gold), waits_gold_total_(waits_gold)
{
    assert(waits_gold >= 0);
    add_upgrade(total_take_percent, upgrade(
                    0.05, new linear_growing_number(-0.01),
                    50, new multiply_growing_number(1.2),
                    4, "% of total debt taken per activation"
                ));
    add_upgrade(bribed, upgrade(
                    1, new linear_growing_number(-0.01),
                    100, new multiply_growing_number(2),
                    10, "% of debt awaiting"
                ));
}

bool debt_collector::activate_(state &s)
{
    if (!waits_gold_)
        return false;

    bool have_multiple_debts_at_once =
            1 < count_if(s.rooms_.begin(), s.rooms_.end(), [](shared<room> r) {
                auto *debt = dynamic_cast<debt_collector *>(r.get());
                return debt && debt->waits_gold_ > 0;
            });
    if (have_multiple_debts_at_once) {
        s.state_ = state::lost_by_debt;
        return true;
    }

    int can_take_per_time = upgrade_value_multiplier(total_take_percent, waits_gold_total_);
    int can_take = min(s.gold(), min(waits_gold_, can_take_per_time));
    if (!s.inc_gold(-upgrade_value_multiplier(bribed, can_take)))
        return false;

    waits_gold_ -= can_take;
    return true;
}

void debt_collector::draw_info(ui &o) const
{
    if (waits_gold_) {
        o << "came to collect debt " << waits_gold_ << ui::gold <<
             " (of the total " << waits_gold_total_ << ui::gold << ")";
        return;
    }
    o << "collected all the " << waits_gold_total_ << ui::gold << " debt, now chills";
}

int debt_collector::price() const
{
    return waits_gold_ ? -1 : 0;
}

bool panacea::activate_(state &s)
{
    s.state_ = state::won_by_panacea;
    return true;
}

void panacea::draw_info(ui &o) const
{
    o << "wins you the game!";
}

}
