#include "snap_map.h"
#include <linux/blkdev.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>

static LIST_HEAD(map_list);
static DEFINE_SPINLOCK(write_lock);
static struct srcu_struct srcu;

int snap_map_init(void) {
    init_srcu_struct(&srcu);
    return 0;
}

void snap_map_cleanup(void) {
    LIST_HEAD(list);
    spin_lock(&write_lock);
    list_splice_init(&map_list, &list);
    spin_unlock(&write_lock);
    synchronize_srcu(&srcu);
    struct snap_map *pos, *tmp;
    list_for_each_entry_safe(pos, tmp, &list, list) {
        rbitmap32_destroy(&pos->bitmap);
        kfree(pos);
    }
    cleanup_srcu_struct(&srcu);
}

static void snap_map_destroy_srcu(struct callback_head *head) {
    struct snap_map *p = container_of(head, struct snap_map, head);
    rbitmap32_destroy(&p->bitmap);
    kfree(p);
}

void snap_map_destroy(dev_t dev, struct timespec64 *created_on) {
    unsigned long flags;
    spin_lock_irqsave(&write_lock, flags);
    bool found;
    struct snap_map *pos;
    list_for_each_entry(pos, &map_list, list) {
        found = pos->device == dev && timespec64_equal(&pos->session_created_on, created_on);
        if (found) {
            list_del_rcu(&pos->list);
            break;
        }
    }
    spin_unlock_irqrestore(&write_lock, flags);
    if (found) {
        call_srcu(&srcu, &pos->head, snap_map_destroy_srcu);
    }
}

static struct snap_map* snap_map_alloc(dev_t dev, struct timespec64 *created_on) {
    struct snap_map *map;
    map = kzalloc(sizeof(*map), GFP_KERNEL);
    if (!map) {
        return NULL;
    }
    map->device = dev;
    memcpy(&map->session_created_on, created_on, sizeof(struct timespec64));
    int err = rbitmap32_init(&map->bitmap);
    if (err) {
        kfree(map);
        return NULL;
    }
    return map;
}

static struct snap_map *snap_map_lookup_srcu(dev_t dev, struct timespec64 *created_on) {
    struct snap_map *pos;
    list_for_each_entry_srcu(pos, &map_list, list, srcu_read_lock_held(&srcu)) {
        if (pos->device == dev && timespec64_equal(&pos->session_created_on, created_on)) {
            return pos;
        }
    }
    return NULL;
}

static int try_snap_map_create(dev_t dev, struct timespec64 *created_on) {
    bool found = false;
    struct snap_map *map = snap_map_alloc(dev, created_on);
    spin_lock(&write_lock);
    struct snap_map *pos;
    list_for_each_entry(pos, &map_list, list) {
        found = pos->device == dev && timespec64_equal(&pos->session_created_on, created_on);
        if (found) {
            break;
        }
    }
    if (!found) {
        list_add_rcu(&map->list, &map_list);
    }
    spin_unlock(&write_lock);
    if (found) {
        kfree(map);
        return -EEXIST;
    }
    return 0;
}

/**
 * snap_map_create searches whether a bitmap associated with a device number and certain date (the date when a session
 * has been creted) exists, if a bitmap doesn't exist, then a new one is created. It returns 0 if a new bitmap has been created,
 * -EEXIST if a bitmap already exists, <0 otherwise. 
 */
int snap_map_create(dev_t dev, struct timespec64 *created_on) {
    int rdx = srcu_read_lock(&srcu);
    struct snap_map *map = snap_map_lookup_srcu(dev, created_on);
    if (map) {
        srcu_read_unlock(&srcu, rdx);
        return -EEXIST;
    }
    srcu_read_unlock(&srcu, rdx);
    return try_snap_map_create(dev, created_on);
}

int snap_map_add_range(dev_t dev, struct timespec64 *created_on, sector_t lo, sector_t hi_excl, unsigned long *added) {
    int err;
    int rdx = srcu_read_lock(&srcu);
    struct snap_map *map = snap_map_lookup_srcu(dev, created_on);
    if (map) {
        err = rbitmap32_add_range(&map->bitmap, lo, hi_excl, added);
    }
    srcu_read_unlock(&srcu, rdx);
    return err;
}

/**
 * Aggiunge una regione da 512B nella bitmap, una bitmap è associata a un numero di dispositivo e la data di creazione della sessione.
 * Se nessuna bitmap è associata a un numero di dispositivo, allora si deve creare.
 */
int snap_map_add_sector(dev_t dev, struct timespec64 *created_on, sector_t sector, bool *added) {
    int err;
    int rdx = srcu_read_lock(&srcu);
    struct snap_map *map = snap_map_lookup_srcu(dev, created_on);
    if (map) {
        err = rbitmap32_add(&map->bitmap, sector, added);
    }
    srcu_read_unlock(&srcu, rdx);
    return err;
}