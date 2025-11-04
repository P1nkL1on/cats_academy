#include <QMap>
#include <QRandomGenerator>
#include <functional>

namespace ca {

template<typename k, typename v> using map = QMap<k, v>;
template<typename t> using vector = QVector<t>;
template<typename t> using list = QList<t>;
using random = QRandomGenerator;

enum dice_type
{
    dt_invalid = 0,
    dt_d6 = 1 << 4,
};

enum dice_hash
{
    dh_invalid = 0,
    dh_d6 = dt_d6,
    dh_d6_first = dt_d6 + 1,
    dh_d6_last = dt_d6 + 6,
};

struct dice
{
    dice(dice_hash);
    dice_type type() const;
    int value() const;

    using filter = std::function<bool(dice)>;
    static bool filter_any(dice) { return true; }
private:
    dice_hash dh_ = dh_invalid;
};

struct room;

struct state
{
    map<int, map<int, room *>> rooms_;
    int gold = 0;
    int rolls = 0;

    dice_hash roll_d6();
    bool inc_dice(dice_hash, int added = 1);
    dice_hash has_dice(dice::filter, int count = 1) const;
    void set_room(int x, int y, room *);

    void next_roll();
    void reset();

private:
    map<dice_hash, int> dice_count_;
    random rng_;
};

struct room
{
    virtual ~room() = default;
    bool activate(state &);
    int activates_ = 0;
protected:
    virtual bool activate_(state &) { return false; }
    virtual int activates_max_() const { return 1; }
};

struct herbalist : room
{
    bool activate_(state &s) override;
};

struct seller : room
{
    bool activate_(state &s) override;
};

}
