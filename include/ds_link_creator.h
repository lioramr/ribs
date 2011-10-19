#ifndef _DS_LINK_CREATOR__H_
#define _DS_LINK_CREATOR__H_

#include "mmap_file.h"
#include "ds_field.h"
#include "hashtable.h"

template<typename L, typename R> // local/remote
class ds_link_creator
{
public:
    static int generate_one_to_one(const char *ldir, const char *lbase, const char *rfile);
    struct R_to_loc
    {
        R r;
        uint32_t loc;
    };

    static int entry_compar(const void *a, const void *b);
    
    static int generate_one_to_many(const char *ldir, const char *lbase, const char *rfile);
};

template<typename L, typename R>
/* static */
inline int ds_link_creator<L, R>::generate_one_to_one(const char *ldir, const char *lbase, const char *rfile)
{
    ds_field<L> dsf_local;
    
    char lfile[strlen(ldir) + strlen(lbase) + 2]; // include: '/', '\0'
    sprintf(lfile, "%s/%s", ldir, lbase);
    char lfile_link[strlen(lfile) + 5 + 1];
    sprintf(lfile_link, "%s.link", lfile);
    if (0 > dsf_local.init(lfile))
        return -1;

    mmap_file_write mfw_local;
    if (0 > mfw_local.init(lfile_link))
        return -1;

    ds_field<R> dsf_remote;
    if (0 > dsf_remote.init(rfile))
        return -1;

    // generate id to ofs on the remote side
    size_t n = dsf_remote.num_records();
    hashtable ht;
    ht.init(n);
    R *remote = dsf_remote.begin();
    R *remote_end = remote + n;
    uint32_t loc = 0;
    for (; remote != remote_end; ++remote, ++loc)
    {
        ht.insert32(remote, sizeof(R), loc);
    }

    // build ofs table on the local side
    L *local = dsf_local.begin();
    L *local_end = local + dsf_local.num_records();
    int64_t t = ds_type_mapper::UINT32;
    mfw_local.write(&t, sizeof(t));
    for (; local != local_end; ++local)
    {
        uint32_t *loc = ht.lookup32(local, sizeof(L));
        uint32_t ofs = (loc ? *loc : (uint32_t)-1);
        mfw_local.write(&ofs, sizeof(ofs));
    }
    mfw_local.close();
    return 0;
}

template<typename L, typename R>
/* static */
int ds_link_creator<L, R>::entry_compar(const void *a, const void *b)
{
    const R_to_loc *aa = (const R_to_loc *)a;
    const R_to_loc *bb = (const R_to_loc *)b;
    return (aa->r < bb->r ? -1 : (aa->r > bb->r ? 1 : 0));
}

template<typename L, typename R>
/* static */
int ds_link_creator<L, R>::generate_one_to_many(const char *ldir, const char *lbase, const char *rfile)
{
    ds_field<L> dsf_local;
    char lfile[strlen(ldir) + strlen(lbase) + 2]; // include: '/', '\0'
    sprintf(lfile, "%s/%s", ldir, lbase);
    char lfile_link[strlen(lfile) + 5 + 1];
    sprintf(lfile_link, "%s.link", lfile);
    char lfile_link_offs[strlen(lfile) + 10 + 1];
    sprintf(lfile_link_offs, "%s.link.offs", lfile);
    
    if (0 > dsf_local.init(lfile))
        return -1;

    mmap_file_write mfw_local, mfw_local_offs;
    if (0 > mfw_local.init(lfile_link) ||
        0 > mfw_local_offs.init(lfile_link_offs))
        return -1;

    ds_field<R> dsf_remote;
    if (0 > dsf_remote.init(rfile))
        return -1;

    // generate id to ofs on the remote side
    size_t n = dsf_remote.num_records();

    vmbuf vect;
    vect.init(sizeof(R_to_loc) * n);
        
    R *remote = dsf_remote.begin();
    R *remote_end = remote + n;
    struct R_to_loc *entries = (struct R_to_loc *)vect.data();
    struct R_to_loc *entries_end = entries + n;
    struct R_to_loc *e = entries;
    uint32_t loc = 0;
    for (; remote != remote_end; ++remote, ++loc, ++e)
    {
        e->r = *remote;
        e->loc = loc;
    }
        
    qsort(entries, n, sizeof(R_to_loc), entry_compar);

    struct data
    {
        uint32_t size;
        uint32_t loc;
    };

    // build hashtable data->{size, location}
    hashtable ht;
    ht.init(n);

    loc = 0;
    for (e = entries; e != entries_end; )
    {
        R r = e->r;
        uint32_t l = loc;
        for (; e != entries_end && r == e->r; ++e, ++loc);
        data d;
        d.size = loc - l;
        d.loc = l;
        ht.insert(&r, sizeof(r), &d, sizeof(d));
    }

    // build ofs vects on the local side
    L *local = dsf_local.begin();
    L *local_end = dsf_local.end();
    for (; local != local_end; ++local)
    {
        uint32_t s, loc;
        uint32_t l = ht.lookup(local, sizeof(L));
        if (ht.is_found(l))
        {
            data *d = (data *)ht.get_val(l);
            s = d->size;
            loc = d->loc;
        } else
        {
            s = 0; // empty vector
            loc = 0;
        }
        mfw_local.write(&s, sizeof(uint32_t));
        for (struct R_to_loc *e = entries + loc, *e_end = e + s; e != e_end; ++e)
            mfw_local_offs.write(&e->loc, sizeof(uint32_t));
    }
    mfw_local.close();
    mfw_local_offs.close();
    return 0;
}


#endif // _DS_LINK_CREATOR__H_
