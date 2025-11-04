#include <main.h>
#include <cassert>
#include <algorithm>
#include <iostream>

using namespace std;

int main()
{
    using namespace ca;
    state s;
    s.reset();

    s.set_room(0, 0, new herbalist);
    s.set_room(0, 1, new herbalist);
    s.set_room(0, 2, new herbalist);

    s.set_room(10, 0, new seller);
    s.set_room(10, 1, new seller);
    s.set_room(10, 2, new seller);

    while(s.rolls < 10) {
        s.next_roll();
        cout << "turn [" << s.rolls << "], gold = " << s.gold << '\n';
    }
    return 0;
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

bool herbalist::activate_(state &s)
{
    s.inc_dice(s.roll_d6(), 1);
    return true;
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
    delete rooms_[x][y];
    rooms_[x][y] = r;
}

bool seller::activate_(state &s)
{
    const dice_hash dh = s.has_dice(dice::filter_any);
    if (!dh)
        return false;

    s.inc_dice(dh, -1);
    s.gold += dice(dh).value();
    return true;
}

bool room::activate(state &s)
{
    if (activates_ >= activates_max_())
        return false;
    if (!activate_(s))
        return false;
    activates_++;
    return true;
}

void state::next_roll()
{
    const list<int> xs = rooms_.keys();

    for (int x : xs) {
        const list<room *> rooms = rooms_[x].values();
        if (!rooms.size())
            continue;

        for (;;) {
            const bool used = any_of(
                rooms.begin(), rooms.end(),
                [this](room *r){ return r->activate(*this); });

            if (!used)
                break;
        }
    }

    for (const auto &ys : qAsConst(rooms_))
        for (room *r : ys)
            r->activates_ = 0;

    rolls++;
}

void state::reset()
{
    for (const auto &r : qAsConst(rooms_))
        qDeleteAll(r);

    *this = state{};
}

}
