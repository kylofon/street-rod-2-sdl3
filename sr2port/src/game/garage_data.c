/* Garage: the car and part data model (garage.md "Records", 0001:426c - 4852, 70d2 - 71be, 7a44). The pools and
 * part_alloc / car_alloc / part_free / car_release are game_flow's (flow.h). Records are u16 DS offsets. */
#include "game/garage.h"

#include "game/flow.h"
#include "game/ds_shop.h"
#include "platform/platform.h"

static inline s16 init_wear(s16 cat) { return DSS((u16)(DS_INIT_WEAR + 2 * cat)); }

/* 0001:426c */
void spare_add(s16 type, s16 value)
{
    u16 p = part_alloc();
    DSW((u16)(p + PT_TYPE)) = (u16)type;
    DSW((u16)(p + PT_WEAR)) = (u16)init_wear(parts_category(type));
    DSW((u16)(p + PT_VALUE)) = (u16)value;
    DSW((u16)(p + PT_NEXT)) = DSW(DS_spare_parts);
    DSW(DS_spare_parts) = p;
}

/* One stock part of catalogue type t in the slot (the PART macro of the spec): a new node, or with `reuse` the
 * node the slot already holds. Returns the node. */
static u16 stock_part(u16 reuse, u16 slot, s16 t, s16 cat)
{
    u16 p = reuse == 0 ? part_alloc() : DSW(slot);
    DSW(slot) = p;
    DSW((u16)(p + PT_TYPE)) = (u16)t;
    DSW((u16)(p + PT_VALUE)) = (u16)parts_price(t);
    DSW((u16)(p + PT_WEAR)) = (u16)init_wear(cat);
    DSW((u16)(p + PT_NEXT)) = 0;
    return p;
}

/* 0001:42aa */
u16 car_new(s16 model, s16 value, u16 reuse)
{
    u16 c = reuse != 0 ? reuse : car_alloc();
    DSW((u16)(c + CR_MODEL)) = (u16)model;
    DSW((u16)(c + CR_VALUE)) = (u16)value;
    DSW((u16)(c + CR_GAS)) = 0x28;
    DSB((u16)(c + CR_CLASS)) = model_class(model);
    s16 col;
    do {
        col = rnd(6);
        if (col != 5) break;
    } while (rnd(2) != 1);
    DSB((u16)(c + CR_COLOUR)) = (u8)col;
    DSB((u16)(c + CR_ENGINE_LINK)) = 1;
    DSB((u16)(c + CR_IGNITION)) = (u8)(rnd(9) - 5);

    u16 specA = model_specA(model);
    u16 specB = model_specB(model);
    s16 make = (s16)((specA & 0x0E) >> 1);          /* one-hot mask 1 GM / 2 Ford / 4 Chrysler -> 0 / 1 / 2 */
    stock_part(reuse, (u16)(c + CR_TRANS), (s16)((((specA & 0xC0) >> 6) + 3) * 3 + make), 1);
    stock_part(reuse, (u16)(c + CR_ENGINE), (s16)(((specA >> 8) & 3) * 3 + make), 0);
    stock_part(reuse, (u16)(c + CR_TYRES), (s16)(((specA & 0x30) >> 4) + 0x32), 4);
    for (u16 k = 3; k < 6; k++) DSB((u16)(c + CR_BOLT + k)) = 0;      /* carburettor bolts */
    s16 g = (s16)((specA >> 10) & 0x0F);                               /* manifold grade 0..8 */
    s16 idx = (s16)((g + 8) * 3 + make);
    if (g == 8 && make == 4) idx--;                                    /* dead code in the exe: make is never 4 */
    stock_part(reuse, (u16)(c + CR_MANIFOLD), idx, 3);
    s16 ctype = (s16)((specA >> 14) + 0x15);                           /* carburettor 21..23 */
    s16 n = (s16)(DSS((u16)(DS_CARBS_PER_MANIFOLD + 2 * g)) - 1);      /* -1 for injection / supercharger */
    for (s16 k = 2; k > n; k--) DSW((u16)(c + CR_CARB + 2 * k)) = 0;
    for (s16 k = n; k >= 0; k--) {
        stock_part(reuse, (u16)(c + CR_CARB + 2 * k), ctype, 2);
        DSB((u16)(c + CR_BOLT + 3 + k)) = 3;                           /* one bolt per carburettor */
    }
    for (u16 k = 0; k < 3; k++) DSB((u16)(c + CR_BOLT + k)) = 3;       /* engine bolt + 2 manifold bolts */
    for (u16 k = 0; k < 2; k++) DSB((u16)(c + CR_TRANS_BOLT + k)) = 3;
    DSW((u16)(c + CR_FLAGS)) = 0;
    stock_part(reuse, (u16)(c + CR_EXHAUST), (s16)((specB & 0x1F) * 3 + make + 0x35), 5);
    stock_part(reuse, (u16)(c + CR_MUFFLER), (s16)(((specB & 0x3E0) >> 5) + 0x3B), 6);
    stock_part(reuse, (u16)(c + CR_DIFF), (s16)((specB >> 10) + 0x3D), 7);
    if (reuse == 0) {
        if (DSW(DS_cur_car) != 0) {
            DSW((u16)(c + CR_NEXT)) = DSW(DS_car_list);
            DSW(DS_car_list) = c;
        } else {
            DSW(DS_cur_car) = c;
        }
    }
    return c;
}

/* 0001:46f0 */
s16 part_release(u16 p)
{
    if (p == 0) return 0;
    s16 v = DSS((u16)(p + PT_VALUE));
    part_free(p);
    return v;
}

/* 0001:4716: the value of the parts that count (transmission, engine, manifold, carburettors) leaves the car's value */
s16 car_free(u16 car)
{
    DSW((u16)(car + CR_VALUE)) = (u16)(DSS((u16)(car + CR_VALUE)) - part_release(DSW((u16)(car + CR_TRANS))));
    for (u16 i = 0; i < 5; i++)
        DSW((u16)(car + CR_VALUE)) = (u16)(DSS((u16)(car + CR_VALUE)) -
                                           part_release(DSW((u16)(car + CR_ENGINE + 2 * i))));
    part_release(DSW((u16)(car + CR_TYRES)));
    part_release(DSW((u16)(car + CR_EXHAUST)));
    part_release(DSW((u16)(car + CR_MUFFLER)));
    part_release(DSW((u16)(car + CR_DIFF)));
    s16 v = DSS((u16)(car + CR_VALUE));
    if (v < 0) v = 0;
    car_release(car);
    return v;
}

/* 0001:4804 (the manifold must be present) */
u16 car_flags(u16 car)
{
    u16 f = 0;
    u8 hi = DSB((u16)(car + CR_FLAGS + 1));
    if (hi & 0x20) f = 4;
    if (hi & 0x40) f |= 2;
    if (hi & 0x80) f |= 1;
    if (DSW((u16)(car + CR_MUFFLER)) == 0) f |= 0x20;
    if (parts_grade(DSS((u16)(DSW((u16)(car + CR_MANIFOLD)) + PT_TYPE))) == 8) f |= 8;   /* supercharger */
    return f;
}

/* 0001:70d2: spares of category cat into nodes[n0..], wear[n0..], DS_list_entries[n0..]; *count = last index + 1 */
void spares_collect(s16 cat, u16 *nodes, s8 *wear, s16 n0, s16 *count)
{
    s16 n = n0;
    for (u16 p = DSW(DS_spare_parts); p != 0; p = DSW((u16)(p + PT_NEXT))) {
        s16 t = DSS((u16)(p + PT_TYPE));
        if (parts_category(t) == cat) {
            nodes[n] = p;
            DSW((u16)(DS_list_entries + 2 * n)) = parts_text(t);
            s16 w = DSS((u16)(p + PT_WEAR));
            wear[n] = w == -0x80 ? (s8)0x80 : (s8)(w / 100);
            n++;
        }
    }
    *count = n;
}

/* 0001:715a: the slot's old part becomes a spare; cur_car's value follows the catalogue values */
void part_install(u16 part, u16 slot)
{
    u16 cur = DSW(DS_cur_car);
    u16 old = DSW(slot);
    if (old != 0) {
        DSW((u16)(old + PT_NEXT)) = DSW(DS_spare_parts);
        DSW(DS_spare_parts) = old;
        DSW((u16)(cur + CR_VALUE)) = (u16)(DSS((u16)(cur + CR_VALUE)) - DSS((u16)(old + PT_VALUE)));
    }
    DSW(slot) = part;
    DSW((u16)(part + PT_NEXT)) = 0;
    DSW((u16)(cur + CR_VALUE)) = (u16)(DSS((u16)(cur + CR_VALUE)) + DSS((u16)(part + PT_VALUE)));
}

/* 0001:7196 */
void part_uninstall(u16 slot)
{
    u16 cur = DSW(DS_cur_car);
    u16 p = DSW(slot);
    DSW((u16)(p + PT_NEXT)) = DSW(DS_spare_parts);
    DSW(DS_spare_parts) = p;
    DSW((u16)(cur + CR_VALUE)) = (u16)(DSS((u16)(cur + CR_VALUE)) - DSS((u16)(p + PT_VALUE)));
    DSW(slot) = 0;
}

/* 0001:71be (the part must be on the list; else it returns 0 after running off the end) */
u16 spare_unlink(u16 part)
{
    u16 prev = 0, q;
    for (q = DSW(DS_spare_parts); q != 0 && q != part; q = DSW((u16)(q + PT_NEXT))) prev = q;
    if (prev == 0) DSW(DS_spare_parts) = DSW((u16)(q + PT_NEXT));
    else DSW((u16)(prev + PT_NEXT)) = DSW((u16)(q + PT_NEXT));
    return q;
}

/* 0001:7a44 */
s16 car_runnable(u16 car, s16 full)
{
    if (DSW((u16)(car + CR_ENGINE)) == 0 || DSC((u16)(car + CR_ENGINE_LINK)) <= 0 ||
        DSW((u16)(car + CR_MANIFOLD)) == 0)
        return 0;
    s16 n = DSS((u16)(DS_CARBS_PER_MANIFOLD + 2 * part_grade(DSW((u16)(car + CR_MANIFOLD)))));
    for (s16 k = 0; k < n; k++)
        if (DSW((u16)(car + CR_CARB + 2 * k)) == 0) return 0;
    for (s16 i = (s16)(n + 2); i >= 0; i--)
        if (DSC((u16)(car + CR_BOLT + i)) != 3) return 0;
    if (full != 0) {
        if (DSW((u16)(car + CR_TRANS)) == 0 || DSW((u16)(car + CR_TYRES)) == 0) return 0;
        if (DSC((u16)(car + CR_TRANS_BOLT + 1)) != 3 || DSC((u16)(car + CR_TRANS_BOLT)) != 3) return 0;
    }
    return 1;
}
