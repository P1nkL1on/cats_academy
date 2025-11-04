#ifndef MAIN_H
#define MAIN_H

#include <vector>
#include <unordered_map>


/// basic game loop
///
///     recieve fresh dumb cats
///     teach them to be specialists
///     send specialists to the dungeons
///     recieve money and hire more newbies, tech more, go more

constexpr int DAY = 1;
constexpr int WEEK = DAY * 7;
constexpr int MONTH = WEEK * 4;
constexpr int YEAR = MONTH * 12;

struct land {
    virtual ~land() = default;
    virtual int travel_cost(int cats, int days) const = 0;
    virtual int exploring_cost(int cats, int days) const = 0;
    virtual int living_cost(int cats, int days) const = 0;
};

struct dungeon {
    virtual ~dungeon() = default;
    virtual int level() const = 0;
    virtual int days_to_travel() const = 0;
    virtual land *location() const = 0;
    virtual int random_treasure() = 0;
    virtual int random_loot() = 0;
};

enum spec {
    TRAVELER,
    DUNGEON_FIGHTER,
    DUNGEON_EXPORER,
    CITY_EXPLORER,
    RECRUITER,
};

struct cat {
};

struct cats_party : cat {
    virtual int count() const = 0;
};

enum room_status {
    FREE, BUSY,
    RS_SEARCHING, RS_HIRING,
    DSB_TRAVEL, DSB_GOING_DEEPER, DSB_RETURNING,
    DEB_TRAVEL, DEB_DUNGEONEERING, DEB_FIGHTING, DEB_RETURNING,
};

struct room_ctx {
    virtual cat *cat_in_room(int who = 0) const = 0;
    virtual void cat_exits_room(cat *c) = 0;
    virtual cats_party *cats_in_room() const = 0;
    virtual bool cat_task(int days, cat *, spec, room_status s = BUSY) = 0;
    virtual void wait_days(int) = 0;
    virtual void cooldown_days(int) = 0;
    virtual dungeon *random_unfound_dungeon() = 0;
    virtual dungeon *selected_dungeon() = 0;
    virtual void dungeon_found(cat *, dungeon *) = 0;
    virtual void lost_on_a_road(cat *, land *) = 0;
    virtual bool spend_money(cat *, int) = 0;
    virtual void add_money(cat *, int) = 0;
    virtual void fail_dungeon_found(cat *, dungeon *) = 0;
    virtual void fail_spend_money(cat *) = 0;
    virtual void fail_recruit(cat *) = 0;
    virtual cat *generate_recruit() = 0;
    virtual land *location() const = 0;
    virtual int random_number(int min, int max) const = 0;
    virtual int random_number(int limit) const { return random_number(0, limit - 1); }
};

struct room {
    virtual ~room() = default;
    virtual void run(room_ctx &ctx) = 0;
};

struct world {
    virtual ~world() = default;
    virtual void skip_days() const = 0;
};

// content

struct room_recruit_service : room {
    void run(room_ctx &ctx) override;
};

struct room_dungeon_search_bureau : room {
    void run(room_ctx &ctx) override;
};

struct room_dungeon_explorers_base : room {
    void run(room_ctx &ctx) override;
};

// impl

struct land_generic : land {
    int travel_cost(int cats, int days) const override;
    int exploring_cost(int cats, int days) const override;
    int living_cost(int cats, int days) const override;
};

struct dungeon_generic : dungeon {
    dungeon_generic(land *l) : _l(l) {}
    int level() const override;
    int days_to_travel() const override;
    land *location() const override;
    int random_treasure() override;
    int random_loot() override;
private:
    land *_l = nullptr;
    int _money = 2000;
};

struct world_generic : world, room_ctx {
    world_generic();
    ~world_generic() override;
    world_generic(world_generic const &) = delete;
    world_generic(world_generic &&) = delete;
    world_generic &operator=(world_generic const &) = delete;
    world_generic &operator=(world_generic &&) = delete;

    void skip_days() const override;
    cat *cat_in_room(int who) const override;
    void cat_exits_room(cat *c) override;
    cats_party *cats_in_room() const override;
    bool cat_task(int days, cat *, spec, room_status s) override;
    void wait_days(int) override;
    void cooldown_days(int) override;
    dungeon *random_unfound_dungeon() override;
    dungeon *selected_dungeon() override;
    void dungeon_found(cat *, dungeon *) override;
    void lost_on_a_road(cat *, land *) override;
    bool spend_money(cat *, int) override;
    void add_money(cat *, int) override;
    void fail_dungeon_found(cat *, dungeon *) override;
    void fail_spend_money(cat *) override;
    void fail_recruit(cat *) override;
    cat *generate_recruit() override;
    land *location() const override;
    int random_number(int min, int max) const override;
private:
    int _date = 0;
    int _money = 0;
    struct cat_info {
        room *in = nullptr;
    };
    std::vector<cat *> _cats;
    std::unordered_map<const cat *, cat_info> _cat_info;
    void add_cat(cat *);
    void set_cat_room(cat *, room *);
    room *cats_room(cat *) const;

    struct room_info {
        std::vector<room *> exits;
        room_status status = FREE;
    };
    std::vector<room *> _rooms;
    std::unordered_map<const room *, room_info> _room_infos;
    void add_room(room *);
    void add_room_exit(room *exits_from, room *exits_to);
    void set_room_status(room *, room_status);

    std::vector<land *> _lands;
    void add_land(land *);

    std::vector<dungeon *> _dungeons;
    void add_dungeon(dungeon *);
    struct {
        room *self = nullptr;
        int _days_passed = 0;
    } current_room;
};


#endif //MAIN_H
