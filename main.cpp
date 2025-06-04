#include "main.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <ratio>


int main()
{
    school_impl s;

    auto* c = new cat_basic();
    s.add_cat(c);

    auto* c2 = new cat_basic();
    s.add_cat(c2);
    c2->add_exp(5000);

    auto* r = new room_talking(&s);
    s.add_room(r);
    r->add_cat(c, PUPIL);
    r->add_cat(c2, TEACHER);

    s.skip_day();

    return 0;
}

bool school::is_time(const when w, const int offset, const int multiplier) const
{
    switch (const int d = day() * multiplier + offset) {
    case DAILY:
        return true;
    case WEEKLY:
        return d % 7 == 6;
    case MONTHLY:
        return d % 28 == 27;
    case YEARLY:
        return d % 336 == 335;
    default:
        assert(false && "unreachable");
    }
}

int school::rng(const int max)
{
    std::uniform_int_distribution distribution(0, max - 1);
    return distribution(rng());
}

void school_impl::add_room(room* r)
{
    _rooms.emplace_back(r);
}

void school_impl::add_cat(cat* c)
{
    _cats.emplace_back(c);
}

void school_impl::add_exam(exam* e)
{
    _exams.emplace_back(e);
}

void school_impl::take_cat(cat* c)
{
    for (const auto& room : _rooms)
        if (room->remove_cat(c)) return;
}

void school_impl::skip_day()
{
    // take those who are on exams from rooms
    std::map<exam*, std::vector<cat*>> _cats_on_exam;
    for (const auto& exam : _exams) {
        if (const auto [w, m] = exam->period(); !is_time(w, 0, m))
            continue;
        const auto [from, who, _1, _2] = exam->rooms();
        for (int i = 0; i < exam->capacity(); ++i) {
            auto* cat = from->random_cat(who);
            if (!cat) break;
            from->remove_cat(cat);
            _cats_on_exam[exam.get()].push_back(cat);
        }
    }

    for (const auto& room : _rooms)
        room->skip_day();

    // run exams and return cats
    for (const auto& [e, cs] : _cats_on_exam) {
        for (cat* c : cs) {
            const auto [from, whofrom, to, whoto] = e->rooms();
            if (!e->passed(*c)) {
                from->add_cat(c, whofrom);
            } else {
                if (cat *newc = e->change_cat(c); newc != c) {
                    replace_cat(c, newc);
                    c = newc;
                }
                to->add_cat(c, whoto);
            }
        }
    }

    for (const auto& cat : _cats)
        cat->skip_day();

    _day += 1;
}

void school_impl::replace_cat(const cat* from, cat* to)
{
    for (auto &cat : _cats) {
        if (cat.get() != from) continue;
        cat.reset(to);
        return;
    }
    assert(false && "unreachable");
}

void cat_basic::add_exp(const int exp)
{
    _exp += exp;
    while (_exp >= exp_threshold_for_level(_level + 1))
        _level += 1;
}

void cat_basic::skip_day()
{
    if (_exp > exp_threshold_for_level(_level))
        _exp -= 1;
}

int cat_basic::exp_threshold_for_level(const int level)
{
    assert(level > 0 && "can't have zero or neg lvl");
    if (level == 1)
        return 0;
    int exp = 2500;
    for (int i = 2; i < std::min(8, level); ++i)
        exp *= 2;
    return (level < 8) ? exp : ((level - 7) * 150000);
}

room_impl::room_impl(school* s) : _school(s)
{
}

void room_impl::add_cat(cat* c, const who w)
{
    _school->take_cat(c);
    switch (w) {
    case PUPIL:
        _pupils.push_back(c);
        return;
    case TEACHER:
        _teachers.push_back(c);
        return;
    default:
        assert(false && "unreachable");
    }
}

void room_impl::skip_day()
{
    daily_lesson(_pupils, _teachers);
}

bool room_impl::remove_cat(cat* c)
{
    const auto remove_from = [](std::vector<cat*>& vec, const cat* target) -> bool {
        if (const auto it = std::find(vec.begin(), vec.end(), target); it != vec.end()) {
            vec.erase(it);
            return true;
        }
        return false;
    };
    return remove_from(_pupils, c) || remove_from(_teachers, c);
}

cat* room_impl::random_cat(const who w) const
{
    switch (w) {
    case PUPIL:
        return _pupils.empty() ? nullptr : _pupils.at(_school->rng(_pupils.size()));
    case TEACHER:
        return _teachers.empty() ? nullptr : _teachers.at(_school->rng(_teachers.size()));
    default:
        break;
    }
    assert(false && "unknown who");
}

void exam_impl::set_capacity(const int c)
{
    assert(c > 0 && "can't have zero capacity");
    _capacity = c;
}

void exam_impl::set_period(const when w, const int multiplier)
{
    assert(multiplier > 0 && "can't have zero or neg mult");
    _when = w;
    _multiplier = multiplier;
}

void exam_impl::set_rooms(room* from, const who whofrom, room* to, const who whoto)
{
    assert(from && "no source room");
    assert(to && "no destination room");
    _from = from;
    _whofrom = whofrom;
    _to = to;
    _whoto = whoto;
}

void room_talking::daily_lesson(const std::vector<cat*>& pupils, const std::vector<cat*>& teachers)
{
    if (pupils.empty() || teachers.empty())
        return;

    auto pupils_shuffled = pupils;
    std::shuffle(pupils_shuffled.begin(), pupils_shuffled.end(), _school->rng());

    auto teachers_shuffled = teachers;
    std::shuffle(teachers_shuffled.begin(), teachers_shuffled.end(), _school->rng());

    for (size_t i = 0; i < std::min(pupils.size(), teachers.size()); ++i) {
        cat& pupil = *pupils_shuffled[i];
        const cat& teacher = *teachers_shuffled[i];
        if (pupil.level() < teacher.level())
            pupil.add_exp((teacher.level() - pupil.level()) * 10);
    }
}
