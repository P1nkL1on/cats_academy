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

    auto *r = new room_talking(&s);
    s.add_room(r);

    for (int i = 0; i < 10; ++i) {
        auto *c = new cat_basic();
        s.add_cat(c);
        c->set_name(i == 0 ? "Biggo" : ("Smallo" + std::to_string(i)));
        c->add_exp(i == 0 ? 5000 : 0);
        r->add_cat(c, PUPIL);
    }
    auto *e = new exam_top_level();
    e->set_period(WEEKLY, 2);
    e->set_rooms(r, PUPIL, r, TEACHER);
    s.add_exam(e);

    for (int i = 0; i < 100; ++i)
        s.skip_day();
    s.print_events(std::cout);

    return 0;
}

int room::top_level(const who w) const
{
    int lvl = 0;
    const int n = n_cats(w);
    for (int i = 0; i < n; ++i)
        lvl = std::max(lvl, cat_at(i, w)->level());
    return lvl;
}

void event::print_with_date(std::ostream &os) const
{
    os << "(";
    school::print_time(os, _day);
    os << "): ";
    print(os);
}

void school::print_time(std::ostream &os, const int day)
{
    if (day >= 336)
        os << "year " << day / 336 << ", ";
    os << "month " << (day % 336) / 28 + 1;
    os << ", week " << (day % 28) / 7 + 1;
    os << ", day " << day % 7 + 1;
}

bool school::is_time(const when w, const int offset, const int multiplier) const
{
    const int d = day() * multiplier + offset;
    switch (w) {
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

void school_impl::add_room(room *r)
{
    _rooms.emplace_back(r);
}

void school_impl::add_cat(cat *c)
{
    _cats.emplace_back(c);
}

void school_impl::add_exam(exam *e)
{
    _exams.emplace_back(e);
}

void school_impl::add_event(event *e)
{
    _events.emplace_back(e);
}

void school_impl::take_cat(cat *c)
{
    for (const auto &room : _rooms)
        if (room->remove_cat(c)) return;
}

void school_impl::skip_day()
{
    // take those who are on exams from rooms
    std::map<exam*, std::vector<cat*>> _cats_on_exam;
    for (const auto &exam : _exams) {
        if (const auto [w, m] = exam->period(); !is_time(w, 0, m))
            continue;
        const auto [from, who, _1, _2] = exam->rooms();
        for (int i = 0; i < exam->capacity(); ++i) {
            const int n_cats = from->n_cats(who);
            auto *cat = n_cats ? from->cat_at(school::rng(n_cats), who) : nullptr;
            if (!cat) break;
            from->remove_cat(cat);
            _cats_on_exam[exam.get()].push_back(cat);
        }
    }

    for (const auto &room : _rooms)
        room->skip_day();

    // run exams and return cats
    for (const auto &[e, cs] : _cats_on_exam) {
        for (cat *c : cs) {
            const auto [from, whofrom, to, whoto] = e->rooms();
            const bool passed = e->passed(*c);
            if (!passed) {
                from->add_cat(c, whofrom);
            } else {
                if (cat *newc = e->change_cat(c); newc != c) {
                    replace_cat(c, newc);
                    c = newc;
                }
                to->add_cat(c, whoto);
            }
            add_event(new event_cat_attempted_an_exam(day(), c, e, passed));
        }
    }

    for (const auto &cat : _cats)
        cat->skip_day();

    _day += 1;
}

void school_impl::print_events(std::ostream &os) const
{
    for (const auto &e : _events) {
        e->print_with_date(os);
        os << "\n";
    }
}

void school_impl::clear_events()
{
    _events.clear();
}

void school_impl::replace_cat(const cat *from, cat *to)
{
    for (auto &cat : _cats) {
        if (cat.get() != from) continue;
        cat.reset(to);
        return;
    }
    assert(false && "unreachable");
}

void cat_basic::set_name(const std::string &name)
{
    _name = name;
}

std::string cat_basic::name() const
{
    return _name;
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

room_impl::room_impl(school *s) : _school(s)
{
}

void room_impl::add_cat(cat *c, const who w)
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

bool room_impl::remove_cat(cat *c)
{
    const auto remove_from = [](std::vector<cat*> &vec, const cat *target) -> bool {
        if (const auto it = std::find(vec.begin(), vec.end(), target); it != vec.end()) {
            vec.erase(it);
            return true;
        }
        return false;
    };
    return remove_from(_pupils, c) || remove_from(_teachers, c);
}

int room_impl::n_cats(const who w) const
{
    switch (w) {
    case PUPIL:
        return static_cast<int>(_pupils.size());
    case TEACHER:
        return static_cast<int>(_teachers.size());
    default:
        assert(false && "unreachable");
    }
}

cat *room_impl::cat_at(const int idx, const who w) const
{
    switch (w) {
    case PUPIL:
        return _pupils.empty() ? nullptr : _pupils.at(idx);
    case TEACHER:
        return _teachers.empty() ? nullptr : _teachers.at(idx);
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

void exam_impl::set_rooms(room *from, const who whofrom, room *to, const who whoto)
{
    assert(from && "no source room");
    assert(to && "no destination room");
    _from = from;
    _whofrom = whofrom;
    _to = to;
    _whoto = whoto;
}

void room_talking::daily_lesson(const std::vector<cat*> &pupils, const std::vector<cat*> &teachers)
{
    if (pupils.empty() || teachers.empty())
        return;

    auto pupils_shuffled = pupils;
    std::shuffle(pupils_shuffled.begin(), pupils_shuffled.end(), _school->rng());

    auto teachers_shuffled = teachers;
    std::shuffle(teachers_shuffled.begin(), teachers_shuffled.end(), _school->rng());

    for (size_t i = 0; i < std::min(pupils.size(), teachers.size()); ++i) {
        cat &pupil = *pupils_shuffled[i];
        const cat &teacher = *teachers_shuffled[i];
        if (pupil.level() < teacher.level()) {
            const int xp = (teacher.level() - pupil.level()) * 10;
            pupil.add_exp(xp);
            _school->add_event(new event_cat_learned(_school->day(), &pupil, &teacher, xp));
        }
    }
}

bool exam_top_level::passed(const cat &c) const
{
    const auto [from, whofrom, to, whoto] = rooms();
    if (const bool someone_passed = to->n_cats(whoto) > 0)
        return c.level() > from->top_level(whofrom);

    return c.level() >= from->top_level(whofrom);
}

std::string exam_top_level::name() const
{
    return "Top Level";
}

event_cat_learned::event_cat_learned(const int day, const cat *pupil, const cat *teacher, const int xp) :
    event(day), _pupil(pupil), _teacher(teacher), _xp(xp)
{
    assert(_pupil && _teacher && xp > 0);
}

void event_cat_learned::print(std::ostream &os) const
{
    os << _pupil->name() << " gained " << _xp << "xp by learning from " << _teacher->name();
}

event_cat_attempted_an_exam::event_cat_attempted_an_exam(
    const int day, const cat *c, const exam *e, const bool passed) :
    event(day), _cat(c), _exam(e), _passed(passed)
{
}

void event_cat_attempted_an_exam::print(std::ostream &os) const
{
    os << _cat->name() << " ";
    os << (_passed ? "passed" : "failed");
    os << " an exam " << _exam->name();
}
