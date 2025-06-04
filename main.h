#ifndef MAIN_H
#define MAIN_H
#include <memory>
#include <random>
#include <vector>

class cat
{
public:
    virtual ~cat() = default;
    virtual void add_exp(int exp) = 0;
    virtual int level() const = 0;
    virtual void skip_day() = 0;
};


enum when
{
    DAILY,
    WEEKLY,
    MONTHLY,
    YEARLY,
};

enum who
{
    PUPIL,
    TEACHER,
};


class room
{
public:
    virtual ~room() = default;
    virtual void add_cat(cat *, who) = 0;
    virtual cat *random_cat(who) const = 0;
    virtual bool remove_cat(cat *) = 0;
    virtual void skip_day() = 0;
};


class exam
{
public:
    virtual ~exam() = default;
    virtual void set_period(when, int multiplier) = 0;
    virtual std::tuple<when, int> period() const = 0;

    virtual void set_rooms(room *from, who, room *to, who) = 0;
    virtual std::tuple<room *, who, room *, who> rooms() const = 0;
    virtual int capacity() const = 0;

    virtual bool passed(const cat &) const = 0;
    virtual cat *change_cat(cat *c) const { return c; }
};


class school
{
public:
    virtual ~school() = default;
    virtual void add_room(room *) = 0;
    virtual void add_cat(cat *) = 0;
    virtual void add_exam(exam *) = 0;
    virtual void take_cat(cat *) = 0;
    virtual void skip_day() = 0;
    virtual int day() const = 0;
    bool is_time(when, int offset = 0, int multiplier = 1) const;
    virtual std::mt19937 &rng() = 0;
    int rng(int max);
};


// implementations


class school_impl : public school
{
public:
    void add_room(room *) override;
    void add_cat(cat *) override;
    void add_exam(exam *) override;
    void take_cat(cat *) override;
    void skip_day() override;
    int day() const override { return _day; }
    std::mt19937 &rng() override { return _rng; }
private:
    void replace_cat(const cat *from, cat *to);
    std::vector<std::unique_ptr<room>> _rooms;
    std::vector<std::unique_ptr<cat>> _cats;
    std::vector<std::unique_ptr<exam>> _exams;
    std::mt19937 _rng;
    int _day = 0;
};


class cat_basic : public cat
{
public:
    void add_exp(int exp) override;
    void skip_day() override;
    int level() const override { return _level; }
    static int exp_threshold_for_level(int level);
private:
    int _exp = 0;
    int _level = 1;
};


class room_impl : public room
{
public:
    explicit room_impl(school*);
    void add_cat(cat*, who) override;
    void skip_day() override;
    bool remove_cat(cat*) override;
    cat *random_cat(who) const override;
protected:
    virtual void daily_lesson(
        const std::vector<cat*> &pupils, const std::vector<cat*> &teachers) = 0;
    school *_school = nullptr;
private:
    std::vector<cat*> _pupils;
    std::vector<cat*> _teachers;
};


class exam_impl : public exam
{
public:
    void set_capacity(int c);
    void set_period(when, int multiplier) override;
    void set_rooms(room* from, who, room* to, who) override;
    std::tuple<when, int> period() const override { return {_when, _multiplier}; }
    std::tuple<room*, who, room*, who> rooms() const override { return {_from, _whofrom, _to, _whoto}; }
    int capacity() const override { return _capacity; }
private:
    int _capacity = 0;
    room *_from = nullptr;
    who _whofrom = PUPIL;
    room *_to = nullptr;
    who _whoto = PUPIL;
    when _when = WEEKLY;
    int _multiplier = 1;
};


class room_talking : public room_impl
{
public:
    explicit room_talking(school* s) : room_impl(s) {}
private:
    void daily_lesson(const std::vector<cat*>& pupils, const std::vector<cat*>& teachers) override;
};



#endif //MAIN_H
