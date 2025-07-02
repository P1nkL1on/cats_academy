#include "main.h"

#include <cassert>
#include <iostream>


int main()
{
    std::cout << "Hi, im Biggo, the cat's magician!\n";

    world_generic w;

    return 0;
}

world_generic::world_generic()
{
    add_money(nullptr, 1000);

    add_cat(new cat);
    add_cat(new cat);
    add_cat(new cat);
    add_cat(new cat);

    add_room(new room_recruit_service);
    add_room(new room_dungeon_search_bureau);
    add_room(new room_dungeon_explorers_base);

    set_cat_room(_cats[0], _rooms[0]);
    set_cat_room(_cats[1], _rooms[0]);
    set_cat_room(_cats[2], _rooms[1]);
    set_cat_room(_cats[3], _rooms[1]);

    add_room_exit(_rooms[0], _rooms[0]);
    add_room_exit(_rooms[0], _rooms[1]);
    add_room_exit(_rooms[0], _rooms[2]);

    add_land(new land_generic);
    add_land(new land_generic);

    add_dungeon(new dungeon_generic(_lands[0]));
    add_dungeon(new dungeon_generic(_lands[1]));
    add_dungeon(new dungeon_generic(_lands[1]));
}

world_generic::~world_generic()
{
    for (cat *i : _cats)
        delete i;
    for (room *i : _rooms)
        delete i;
    for (dungeon *i : _dungeons)
        delete i;
    for (land *i : _lands)
        delete i;
}

void world_generic::skip_days() const
{

}

cat *world_generic::cat_in_room(int who) const
{

}

void world_generic::cat_exits_room(cat *c)
{

}

cats_party *world_generic::cats_in_room() const
{

}

bool world_generic::cat_task(int days, cat *c, spec, room_status s)
{
    assert(c && "real cat, pls");
    wait_days(days);
    set_cat_room();
}

void world_generic::wait_days(int x)
{
    assert(x >= 0 && "non-negative only");
    current_room._days_passed += x;
}

void world_generic::cooldown_days(int)
{

}

dungeon *world_generic::random_unfound_dungeon()
{

}

dungeon *world_generic::selected_dungeon()
{

}

void world_generic::dungeon_found(cat *, dungeon *)
{

}

void world_generic::lost_on_a_road(cat *, land *)
{

}

bool world_generic::spend_money(cat *, int)
{

}

void world_generic::add_money(cat *, int)
{

}

void world_generic::fail_dungeon_found(cat *, dungeon *)
{

}

void world_generic::fail_spend_money(cat *)
{

}

void world_generic::fail_recruit(cat *)
{

}

cat *world_generic::generate_recruit()
{

}

land *world_generic::location() const
{

}

int world_generic::random_number(int min, int max) const
{

}

void world_generic::add_cat(cat *c)
{
    _cats.push_back(c);
    _cat_info[c] = {};
}

void world_generic::set_cat_room(cat *c, room *r)
{
    assert(c && "real cat, pls");
    _cat_info[c].in = r;
}

room *world_generic::cats_room(cat *c) const
{
    assert(c && "real cat, pls");
}

void world_generic::add_room(room *r)
{
    _rooms.push_back(r);
    _room_infos[r] = {};
}

void world_generic::add_room_exit(room *exits_from, room *exits_to)
{
    assert(exits_from && exits_to && "no null");
    _room_infos[exits_from].exits.push_back(exits_to);
}

void world_generic::set_room_status(room *r, room_status s)
{
    _room_infos[r].status = s;
}

void world_generic::add_land(land *l)
{
    _lands.push_back(l);
}

void world_generic::add_dungeon(dungeon *d)
{
    _dungeons.push_back(d);
}

void room_dungeon_explorers_base::run(room_ctx &ctx)
{
    dungeon *d = ctx.selected_dungeon();
    cats_party *c = ctx.cats_in_room();

    int expected_time = d->level() * DAY;
    int travel_cost = d->location()->travel_cost(c->count(), d->days_to_travel());
    int living_cost = d->location()->living_cost(c->count(), expected_time);
    if (!ctx.spend_money(c, travel_cost * 2 + living_cost))
        return ctx.fail_spend_money(c);

    if (!ctx.cat_task(d->days_to_travel(), c, TRAVELER, DEB_TRAVEL))
        return ctx.lost_on_a_road(c, d->location());
    int money = 0;
    for (int i = 0; i < d->level(); ++i) {
        if (ctx.cat_task(DAY, c, DUNGEON_EXPORER, DEB_DUNGEONEERING)) {
            money += d->random_treasure();
            continue;
        }
        if (ctx.cat_task(0, c, DUNGEON_FIGHTER, DEB_FIGHTING)) {
            money += d->random_loot();
            continue;
        }
        // TODO: add cat harmed by monsters
    }
    if (!ctx.cat_task(d->days_to_travel(), c, TRAVELER, DEB_RETURNING))
        return ctx.lost_on_a_road(c, ctx.location());
    ctx.add_money(c, money);
}

void room_dungeon_search_bureau::run(room_ctx &ctx)
{
    dungeon *d = ctx.random_unfound_dungeon();
    cats_party *c = ctx.cats_in_room();

    int travel_cost = d->location()->travel_cost(c->count(), d->days_to_travel());
    int explore_cost = d->location()->exploring_cost(c->count(), d->level() * MONTH);
    if (!ctx.spend_money(c, travel_cost * 2 + explore_cost))
        return ctx.fail_spend_money(c);

    if (!ctx.cat_task(d->days_to_travel(), c, TRAVELER, DSB_TRAVEL))
        return ctx.lost_on_a_road(c, d->location());
    for (int i = 0; i < d->level(); ++i) {
        if (!ctx.cat_task(MONTH, c, DUNGEON_EXPORER, DSB_GOING_DEEPER))
            return ctx.fail_dungeon_found(c, d);
    }
    if (!ctx.cat_task(d->days_to_travel(), c, TRAVELER, DSB_RETURNING))
        return ctx.lost_on_a_road(c, ctx.location());
    ctx.dungeon_found(c, d);
}

void room_recruit_service::run(room_ctx &ctx)
{
    cats_party *c = ctx.cats_in_room();
    int cost = ctx.location()->exploring_cost(c->count(), WEEK);
    if (!ctx.spend_money(c, cost))
        return ctx.fail_spend_money(c);

    if (!ctx.cat_task(WEEK, c, CITY_EXPLORER, RS_SEARCHING))
        return ctx.fail_recruit(c);

    int found = ctx.random_number(1, 6);
    int hired = 0;
    for (int i = 0; i < found; ++i) {
        if (ctx.cat_task(DAY, c, RECRUITER, RS_HIRING)) {
            ctx.cat_exits_room(ctx.generate_recruit());
            ++hired;
        }
    }
    if (!hired)
        return ctx.fail_recruit(c);
}

int land_generic::travel_cost(int cats, int days) const
{
    return cats * days * 2;
}

int land_generic::exploring_cost(int cats, int days) const
{
    return cats * days * 5;
}

int land_generic::living_cost(int cats, int days) const
{
    return cats * days;
}

int dungeon_generic::level() const
{
    return 5;
}

int dungeon_generic::days_to_travel() const
{
    return WEEK;
}

land *dungeon_generic::location() const
{
    return _l;
}

int dungeon_generic::random_treasure()
{
    int money = std::min(_money, 500);
    _money -= money;
    return money;
}

int dungeon_generic::random_loot()
{
    return 10;
}
