#include <QMap>
#include <QTextBrowser>

#include <functional>
#include <random>
#include <memory>
#include <string>
#include <iostream>
#include <sstream>

namespace ca {

template<typename k, typename v> using map = QMap<k, v>;
template<typename t> using vector = QVector<t>;
template<typename t> using list = QList<t>;
template<typename t> using shared = std::shared_ptr<t>;
using str = std::string;
using out = std::ostream;
using strout = std::stringstream;

enum dice_type
{
    dt_invalid = 0,
    dt_d6 = 1 << 4,
};

enum dice_hash
{
    dh_invalid = 0,
    dh_d6 = dt_d6,
    dh_d6_1,
    dh_d6_2,
    dh_d6_3,
    dh_d6_4,
    dh_d6_5,
    dh_d6_6,
    dh_d6_first = dh_d6_1,
    dh_d6_last = dh_d6_6,
};

struct dice
{
    dice(dice_hash);
    dice_type type() const;
    int value() const;

    using filter = std::function<bool(dice)>;
    using comparer = std::function<bool(dice, dice)>;
    static bool filter_any(dice) { return true; }
private:
    dice_hash dh_ = dh_invalid;
};

struct room;

struct ui
{
    enum symbol
    {
        undefined,
        gold,
        d6_first,
        d6_last = d6_first + 5,
    };
    enum signal
    {
        next_roll = 10000,
        next_roll_10,
        next_roll_100,

        room_upgrade_first = 20000,
        max_rooms = 100,
        max_room_actions = 900,
        max_room_upgrades = 100,
        max_room_signals = max_room_actions + max_room_upgrades,
        room_upgrade_last  = room_upgrade_first + max_rooms * max_room_signals - 1,
        room_action_sell = 1,
        room_action_move_up,
        room_action_move_down,

        room_buy_first = 120000,
        room_buy_last = 121000,
    };
    static signal mk_signal(int r, int u);
    static signal mk_room_action(int r, int u);
    static signal mk_room_upgrade(int r , int u);
    static signal mk_room_buy(int s);
    static bool rd_signal(signal, int &r, int &u);
    static bool rd_room_action(signal, int &r, int &u);
    static bool rd_room_upgrade(signal, int &r, int &u);
    static bool rd_room_buy(signal, int &s);

    virtual ~ui() = default;
    virtual ui &operator<<(int) = 0;
    virtual ui &operator<<(double) = 0;
    virtual ui &operator<<(str) = 0;
    virtual ui &operator<<(symbol) = 0;
    virtual void nl() {}
    virtual void flush() {}

    virtual void begin_room() {}
    virtual void end_room() {}
    virtual void begin_upgrade() {}
    virtual void end_upgrade() {}
    virtual void begin_button(signal) {}
    virtual void end_button() {}
    virtual void begin_paragraph() {}
    virtual void end_paragraph() {}
    virtual void begin_list() {}
    virtual void end_list() {}
};

struct state
{
    state();
    ui *ui_ = nullptr;
    int rolls = 0;

    dice_hash roll_d6();
    bool inc_dice(dice_hash, int added = 1);
    bool inc_gold(int added);
    dice_hash has_dice(dice::filter, int count = 1, dice::comparer = nullptr) const;
    void insert_room(int r, room *);

    void next_roll();
    void reset();

    int gold() const { return gold_; }
    void draw(ui &) const;
    bool btn(ui::signal);
    bool move_room(int r, int mod);
    bool sell_room(int r);
    bool buy_room(int u);

private:
    map<dice_hash, int> dice_count_;
    list<shared<room>> rooms_;
    list<shared<room>> shop_;
    int gold_ = 0;
    std::mt19937 rng_;
};

struct growing_number
{
    virtual ~growing_number() = default;
    virtual double next(double) const = 0;
};

struct  upgrade
{
    upgrade() = default;
    upgrade(double v, growing_number *vadd,
            double p, growing_number *padd,
            int lvl_max, str description);
    bool level_up(state &s);
    int value_ceil() const { return ceil(value_); }
    int value_floor() const { return floor(value_); }
    double value() const { return value_; }
    void draw(ui &, ui::signal) const;
    int level() const { return level_; }
    str description;
private:
    double value_next() const;
    int price() const;
    double value_ = 0;
    shared<growing_number> value_grow = nullptr;
    double price_ = 0;
    shared<growing_number> price_grow = nullptr;
    int level_max_ = -1;
    int level_ = 0;
};

struct room
{
    virtual ~room() = default;
    bool activate(state &);
    int activates_ = 0;
    int upgrade_count() const { return upgrades_.size(); }
    bool level_up_upgrade(int u, state &s);
    void draw(ui &, int r) const;
    int level() const;
    virtual void draw_info(ui &) const {}
    virtual str name() const { return "Room"; }
    virtual int price() const { return 100; }
    virtual room *duplicate() const = 0;
protected:
    virtual bool activate_(state &) { return false; }
    virtual int activates_max_() const { return 1; }
    void add_upgrade(int, upgrade);
    int upgrade_value_ceil(int u) const { return upgrades_[u].value_ceil(); }
    int upgrade_value_floor(int u) const { return upgrades_[u].value_floor(); }
    int upgrade_value_multiplier(int u, int x = 1) const { return floor(upgrades_[u].value() * x); }
private:
    map<int, upgrade> upgrades_;
};

// content impl

struct linear_growing_number : growing_number
{
    linear_growing_number(double inc) : inc_(inc) {}
    double next(double v) const override { return v + inc_; }
private:
    double inc_ = 0;
};

struct multiply_growing_number : growing_number
{
    multiply_growing_number(double mult) : mult_(mult) {}
    double next(double v) const override { return v * mult_; }
private:
    double mult_ = 0;
};

template <typename room_impl>
struct room_duplicate : room
{
    room *duplicate() const override { return new room_impl(); }
};

struct herbalist : room_duplicate<herbalist>
{
    str name() const override { return "Herbalist"; }
    enum { activates };
    herbalist();
    bool activate_(state &s) override;
    int activates_max_() const override;
    void draw_info(ui &o) const override;
};

struct seller : room_duplicate<seller>
{
    str name() const override { return "Leftovers Salesman"; }
    enum { money_mult };
    seller();
    bool activate_(state &s) override;
    int activates_max_() const override;
    void draw_info(ui &o) const override;
};

struct mass_seller : room_duplicate<mass_seller>
{
    str name() const override { return "Mass Salesman"; }
    enum { activates, base_price };
    mass_seller();
    bool activate_(state &s) override;
    int activates_max_() const override;
    void draw_info(ui &o) const override;
};

struct splitter : room_duplicate<splitter>
{
    enum { max_split_count };
    str name() const override { return "Blender"; }
    splitter();
    bool activate_(state &s) override;
    void draw_info(ui &o) const override;
};

struct debt_collector : room_duplicate<debt_collector>
{
    enum { total_take_percent, bribed };
    debt_collector(int waits_gold = 0);
    str name() const override { return "Debt Collector"; }
    bool activate_(state &s) override;
    void draw_info(ui &o) const override;
    int price() const override;
    int waits_gold_ = 0;
private:
    int waits_gold_total_ = 0;
};

struct ui_cmd : ui
{
    ui_cmd(out &o) : o(o) {}
    ui &operator<<(int) override;
    ui &operator<<(double) override;
    ui &operator<<(str) override;
    ui &operator<<(symbol) override;
    void nl() override { o << "<br>"; }
    void flush() override { o.flush(); }
    void begin_room() override;
    void end_room() override;
    void begin_upgrade() override;
    void end_upgrade() override;
    void begin_button(signal) override;
    void end_button() override;
    void begin_paragraph() override;
    void end_paragraph() override;
    void begin_list() override;
    void end_list() override;
private:
    out &o;
    enum scope { scope_room, scope_upgrade, scope_btn, scope_p, scope_ul };
    static str scope_str(scope s);
    list<scope> scopes_stack_;
    void push_scope(scope);
    void pop_scope(scope);
};

struct ui_QTextEdit : ui_cmd
{
    ui_QTextEdit(QTextBrowser *te) : ui_cmd(s_), te_(te) { assert(te); }
    void flush() override;
    void set_flush_delay(int ms);
private:
    strout s_;
    QTextBrowser *te_ = nullptr;
    int delay_ms_ = 100;
};

}
