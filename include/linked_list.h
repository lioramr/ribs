#ifndef _LINKED_LIST___H
#define _LINKED_LIST__H_

#include "vmbuf.h"
#include <stdint.h>

struct linked_list
{
    typedef struct entry
    {
        entry(): next(ULLONG_MAX), val(ULLONG_MAX), val_len(0) {}
        entry(size_t n, size_t v, uint32_t l): next(n), val(v), val_len(l) {}
	size_t    next;
	size_t    val;
	uint32_t  val_len;
    } entry_t;

    linked_list();
    void init();
    void insert_head(vmbuf &b, size_t val, uint32_t val_len);
    void insert_tail(vmbuf &b, size_t val, uint32_t val_len);
    int get_head(vmbuf &b, linked_list::entry_t &h);
    int get_tail(vmbuf &b, linked_list::entry_t &t);
    int get_next(vmbuf &b, entry_t cur, entry_t &next);
    
    size_t    head;
    size_t    tail;
    uint32_t  cnt;
};


inline linked_list::linked_list()
{
    init();
}

inline void linked_list::init()
{
    head = tail = ULLONG_MAX;
    cnt = 0;
}

inline void linked_list::insert_head(vmbuf &b, size_t val, uint32_t val_len)
{
    entry_t e(head, val, val_len);
    head = b.wlocpos();
    b.memcpy(&e, sizeof(e));
    if (ULLONG_MAX == tail)
	tail = head;
    cnt++;
}

inline void linked_list::insert_tail(vmbuf &b, size_t val, uint32_t val_len)
{
    entry_t e(ULLONG_MAX, val, val_len);
    size_t eofs = b.wlocpos();
    b.memcpy(&e, sizeof(e));
    if (ULLONG_MAX != tail) {
	entry_t *etail = (entry_t *)(b.data()+tail);
	etail->next = eofs;
    }
    tail = eofs;
    cnt++;
    if (ULLONG_MAX == head)
	head = tail;
}

inline int linked_list::get_head(vmbuf &b, linked_list::entry_t &h)
{
    if (b.wlocpos() < head)
	return -1;
    h = *((entry_t*)(b.data()+head));
    return 0;
}

inline int linked_list::get_tail(vmbuf &b, linked_list::entry_t &t)
{
    if (b.wlocpos() < tail)
	return -1;
    t = *((entry_t*)(b.data()+tail));
    return 0;
}

inline int linked_list::get_next(vmbuf &b, entry_t cur, linked_list::entry_t &next)
{
    if (b.wlocpos() <= cur.next || b.wlocpos() < (cur.next + sizeof(entry_t)))
	return -1;
    next = *((entry_t *)(b.data()+cur.next));
    return 0;
}

#endif // _LINKED_LIST__H_
