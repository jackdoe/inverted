#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <sys/stat.h>
#include <limits.h>

#ifndef __GNUC__
#define __builtin_expect(a,b) a
#endif
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#ifndef sayx
#define FORMAT(fmt,arg...) fmt " [%s()]\n",##arg,__func__
#define D(fmt,arg...) printf(FORMAT(fmt,##arg))
#define sayx(fmt,arg...)                        \
    do {                                        \
        D(FORMAT(fmt,##arg));                   \
        exit(EXIT_FAILURE);                     \
    } while(0)
#endif

#define saypx(fmt,arg...) sayx(fmt " { %s(%d) }",##arg,errno ? strerror(errno) : "undefined error",errno);

#define NO_MORE INT_MAX
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int32_t  s32;
typedef int64_t  s64;

struct entry {
    s32 id;
    u8 freq;
    u8 payload[0];
} __attribute__((packed));

struct stats {
    u32 last_id;
    u32 n;
    u32 max_payload_size;
} __attribute__((packed));

struct stored {
    struct stats stats;
    u8 begin[0];
} __attribute__((packed));

struct scored {
    s32 id;
    double score;
};

bool has_suffix(const char *s, const char *suffix) {
    size_t suffix_len = strlen(suffix);
    size_t s_len = strlen(s);
    int i;
    if (s_len < suffix_len)
        return false;
    for (i = 0; i < suffix_len; i++) {
        if (s[s_len - suffix_len + i] != suffix[i])
            return false;
    }
    return true;
}

u32 jenkins_one_at_a_time_hash(const char *key, size_t len) {
    u32 hash, i;
    for(hash = i = 0; i < len; ++i) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

class StoredList {
public:
    u32 size;
    int fd;
    struct stored *stored;
    char path[PATH_MAX];
    u32 stored_payload_size;
    StoredList(const char *fn, const char *root = "/tmp", int initial_payload_size = 4, int buckets = 32) {
        assert(fn != NULL);
        assert(has_suffix(fn, ".idx") == true);
        u32 bucket = jenkins_one_at_a_time_hash(fn,strlen(fn)) % buckets;
        u32 off = snprintf(path,PATH_MAX,"%s/%02d",root,bucket);
        if (off >= PATH_MAX)
            saypx("filename > %d [ truncated: %s ]",PATH_MAX,path);
        mkdir(path,0755);
        if (snprintf(path + off,PATH_MAX,"/%s",fn) >= PATH_MAX)
            saypx("filename > %d [ truncated: %s ]",PATH_MAX,path);

        if ((fd = open(path, O_RDWR|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR)) == -1)
            saypx("open");

        struct stat st;
        if (fstat(fd, &st) == -1)
            saypx("stat");

        struct stored s;
        if (st.st_size < sizeof(s)) {
            bzero(&s,sizeof(s));
            s.stats.max_payload_size = initial_payload_size;
            if (write(fd,&s,sizeof(s)) != sizeof(s))
                saypx("failed to create empty list");
            st.st_size = sizeof(s);
        }
        _mmap(st.st_size);
        // D("found data with %d payload size, entry size: %lu",stored_payload_size(),sizeof_entry());
        assert(initial_payload_size == stored->stats.max_payload_size);
        stored_payload_size = stored->stats.max_payload_size;
    }

    ~StoredList() {
        _munmap();
        close(fd);
    }

    void sync(void) {
        if (unlikely(msync(stored,size,MS_SYNC)) == -1)
            saypx("msync");
    }

    char *get_path(void) {
        return path;
    }

    inline u32 count(void) const {
        return stored->stats.n;
    }

    u32 closest(s32 id, int *found, u32 start = 0) {
        u32 end = stored->stats.n;
        u32 mid = end;

        while (start < end) {
            mid = start + (end - start) / 2;
            struct entry *e = entry_at(mid, 0);
            if (e->id < id) {
                start = mid + 1;
            } else if (e->id > id) {
                end = mid;
            } else {
                *found = 1;
                return mid;
            }
        }
        *found = 0;
        return start;
    }

    void insert(s32 id, u32 payload = 0) {
        insert(id,(u8 *)&payload, sizeof(payload));
    }

    void insert(s32 id, float payload) {
        insert(id,(u8 *)&payload, sizeof(payload));
    }

    void insert(s32 id, u8 *buf, u8 len) {
        assert(len <= stored->stats.max_payload_size);
        int found;
        u32 where = closest(id, &found);
        struct entry *e;
        if (!found) {
            int n = stored->stats.n;
            _remap(sizeof_entry(1));
            stored->stats.n++;
            size_t bytes = sizeof_entry(n - where);
            if (bytes > 0)
                memmove((u8 *) entry_at(where + 1), (u8 *)entry_at(where), bytes);
            e = entry_at(where);
            e->id = id;
            e->freq = 0;
            if (id > stored->stats.last_id)
                stored->stats.last_id = id;
        } else {
            e = entry_at(where);
        }
        bcopy(buf, &e->payload[0], len);
        if (e->freq < 255)
            e->freq++;
    }

    int remove(s32 id) {
        assert(id >= 0);
        int found;
        u32 where = closest(id, &found);
        if (likely(found)) {
            int n = stored->stats.n;
            size_t bytes = sizeof_entry(n - where - 1);
            if (bytes > 0)
                memmove((u8 *) entry_at(where), (u8 *)entry_at(where + 1), bytes);
            stored->stats.n--;
        }
        return found;
    }

    void dump(void) {
        struct entry *e;
        int i,j = 0;
        for (i = 0; i < count(); i++){
            e = entry_at(i, 0);
            printf("pos: %d; id: %d; freq: %d; payload:\n\t", i, e->id, e->freq);
            printf("[");
            for (j = 0; j < stored->stats.max_payload_size; j++) {
                printf(" 0x%x  ", e->payload[j]);
            }
            printf("]");
            printf("\n");
        }
    }

    void _mmap(size_t n) {
        stored = (struct stored *) mmap(0, n, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (unlikely(stored == MAP_FAILED)) {
            close(fd);
            saypx("mmap");
        }
        size = n;
    }
    void _munmap(void) {
        if (stored != NULL) {
            if (munmap(stored, size) == -1)
                saypx("munmap");
        }
        stored = NULL;
    }
    void _remap(int n) {
        assert(stored != NULL);
        u32 asize = sizeof(struct stored) + sizeof_entry(stored->stats.n);
        if (unlikely(asize + n >= size)) {
            n += asize + 10000 * sizeof_entry();
            _munmap();
            if (ftruncate(fd, n) == -1)
                saypx("ftruncate");
            _mmap(n);
        }
    }

    inline size_t sizeof_entry(int n = 1) {
        return (sizeof(struct entry) + stored_payload_size) * n;
    }

    inline s32 get_stored_payload_size(void) {
        return stored_payload_size;
    }

    struct entry *entry_at(u32 pos, s32 expect = 0) {
        if (unlikely(pos >= count()))
            return NULL;
        struct entry *e = (struct entry *) (stored->begin + ((sizeof(struct entry) + stored_payload_size) * pos));
        if (unlikely(expect > 0 && e->id != expect))
            return NULL;
        return e;
    }

    inline s32 id_at_unsafe(u32 pos) {
        return *(int *)(stored->begin + ((sizeof(struct entry) + stored_payload_size) * pos));
    }

};


class Advancable {
public:
    virtual s32 current() = 0;
    virtual s32 skip_to(s32 id) = 0;
    virtual u32 count(void) const = 0;
    virtual void score(struct scored *scored) = 0;
    virtual s32 next(void) = 0;
    virtual void reset(void) = 0;
    virtual const char *to_string(void) = 0;
};


bool smallest (const Advancable *a, const Advancable *b) {
    return a->count() < b->count();
}

bool largest (const Advancable *a, const Advancable *b) {
    return a->count() > b->count();
}

bool scored_cmp (const scored a, const scored b) {
    if (a.score > b.score) return true;
    if (a.score < b.score) return false;
    return a.id < b.id;
}
class TermQuery : public Advancable {
public:
    s32 index;
    s32 doc_id;
    s32 _end;
    StoredList *list;
    char sbuf[PATH_MAX];
    TermQuery(StoredList *list_) : Advancable() {
        list = list_;
        reset();
    }
    const char *to_string(void) {
        snprintf(sbuf,PATH_MAX,"<[%d @ %d] %s>",doc_id,index,this->list->path);
        return sbuf;
    }

    inline s32 update(void) {
        if (unlikely(index >= _end)) {
            doc_id = NO_MORE;
        } else {
            doc_id = list->id_at_unsafe(index);
        }
        return doc_id;
    }

    s32 current() {
        return doc_id;
    }

    s32 skip_to(s32 target) {
        if (unlikely(doc_id == target))
            return target;
        else if (unlikely(target == NO_MORE)) {
            doc_id = target;
            return target;
        }

        u32 end = _end;
        u32 start = likely(index >= 0) ? index : 0;
        while (likely(start < end)) {
            u32 mid = start + ((end - start) / 2);
            s32 id = list->id_at_unsafe(mid);
            if (unlikely(id == target)) {
                index = mid;
                doc_id = target;
                return doc_id;
            }
            if (likely(id < target))
                start = mid + 1;
            else
                end = mid;
        }
        index = start;
        return update();
    }

    s32 next(void) {
        if (likely(doc_id != -1))
            index++;
        return update();
    }

    u32 count(void) const {
        return list->count();
    }

    void score(struct scored *s) {
        struct entry *e = list->entry_at(index);
        assert(e != NULL);
        s->id = e->id;
        s->score += 1;
    }

    void reset(void) {
        index = 0;
        doc_id = -1;
        _end = count();
    }
};
class BoolQuery : public Advancable {
    virtual void add(Advancable *q) = 0;
};

class BoolMustQuery : public BoolQuery {
public:
    s32 doc_id;
    std::vector<Advancable *> queries;
    BoolMustQuery() : BoolQuery() { doc_id = -1; };
    char sbuf[PATH_MAX];
    const char *to_string(void) {
        int off = 0;
        off = snprintf(sbuf,PATH_MAX,"+([%d]",doc_id);
        for (auto query : queries) {
            off += snprintf(sbuf + off,PATH_MAX - off," %s ",query->to_string());
        }
        snprintf(sbuf + off,PATH_MAX - off,")");
        return sbuf;
    }

    void add(Advancable *q) {
        assert(q && q != this);
        queries.push_back(q);
        std::sort(queries.begin(), queries.end(), smallest);
        reset();
    }

    inline s32 _skip_to(s32 id) {
    again:
        for (s32 i = 1; i < queries.size(); i++) {
            s32 n = queries[i]->skip_to(id);
            if (likely(n > id)) {
                id = queries[0]->skip_to(n);
                goto again;
            }
        }

        doc_id = queries[0]->current();
        return doc_id;
    };

    s32 current(void) {
        return doc_id;
    }

    s32 next(void) {
        if (unlikely(queries.size() == 0))
            return NO_MORE;
 
        return _skip_to(queries[0]->next());
    }

    s32 skip_to(s32 id) {
        if (unlikely(queries.size() == 0))
            return NO_MORE;

        return _skip_to(queries[0]->skip_to(id));
    }

    void score(struct scored *s) {
        for (auto query : queries) {
            assert(query->current() == doc_id);
            query->score(s);
        }
    }

    u32 count(void) const {
        if (unlikely(queries.size() == 0))
            return 0;
        return queries[0]->count();
    }

    void reset(void) {
        for (auto query : queries)
            query->reset();
        doc_id = -1;
    }
};


class BoolShouldQuery : public BoolQuery {
public:
    s32 doc_id;
    std::vector<Advancable *> queries;
    BoolShouldQuery() : BoolQuery() { doc_id = -1; };
    char sbuf[PATH_MAX];
    const char *to_string(void) {
        int off = 0;
        off = snprintf(sbuf,PATH_MAX,"~([%d]",doc_id);
        for (auto query : queries) {
            off += snprintf(sbuf + off,PATH_MAX - off," %s ",query->to_string());
        }
        snprintf(sbuf + off,PATH_MAX - off,")");
        return sbuf;
    }

    void add(Advancable *q) {
        assert(q && q != this);
        queries.push_back(q);
        reset();
    }

    s32 current(void) {
        return doc_id;
    }

    s32 next(void) {
        s32 new_doc = NO_MORE;
        for (auto query : queries) {
            s32 cur_doc = query->current();
            if (cur_doc == current()) cur_doc = query->next();
            if (cur_doc < new_doc) new_doc = cur_doc;
        }
        doc_id = new_doc;
        return doc_id;
    }

    s32 skip_to(s32 target) {
        s32 new_doc = NO_MORE;
        for (auto query : queries) {
            s32 cur_doc = query->current();
            if (cur_doc < target) cur_doc = query->skip_to(target);
            if (cur_doc < new_doc) new_doc = cur_doc;
        }
        doc_id = new_doc;
        return doc_id;
    }

    void score(struct scored *s) {
        for (auto query : queries) {
            if (likely(query->current() == doc_id))
                query->score(s);
        }
    }

    u32 count(void) const {
        if (queries.size() == 0)
            return 0;
        u32 c = 0;
        for (auto query : queries)
            c += query->count();
        return c;
    }

    void reset(void) {
        for (auto query : queries)
            query->reset();
        doc_id = -1;
    }
};


std::vector<scored> __topN(Advancable *query,int n_items,int *total) {
    assert(query != NULL);
    assert(n_items > 0);

    query->reset();
    std::vector<scored> items;
    struct scored scored,min_item;
    min_item.id = 0;
    min_item.score = 0;
    bool is_heap = false;

    while (query->next() != NO_MORE) {
        query->score(&scored);
        if (unlikely(scored.score > min_item.score)) {
            if (likely(items.size() >= n_items)) {
                items.push_back(scored);
                if (likely(!is_heap)) {
                    is_heap = true;
                    make_heap(items.begin(), items.end(), scored_cmp);
                } else {
                    push_heap(items.begin(), items.end(), scored_cmp);
                }
                pop_heap(items.begin(), items.end(), scored_cmp);
                items.pop_back();
                min_item = items.front();
            } else {
                is_heap = false;
                items.push_back(scored);
            }
        }
        if (likely(total))
            (*total)++;
        scored.score = 0;
    }
    std::sort(items.begin(), items.end(), scored_cmp);
    return items;
}
