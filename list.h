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
#define FORMAT(fmt,arg...) fmt " [%s()]\n",##arg,__func__
#define D(fmt,arg...) printf(FORMAT(fmt,##arg))
#define sayx(fmt,arg...)                            \
do {                                                \
    D(FORMAT(fmt,##arg));                           \
    exit(EXIT_FAILURE);                             \
} while(0)
#define saypx(fmt,arg...) sayx(fmt " { %s(%d) }",##arg,errno ? strerror(errno) : "undefined error",errno);
#define NO_MORE -1
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

class StoredList {
public:
    u32 size;
    int fd;
    s32 cursor;
    std::string term;
    struct stored *stored;
    StoredList(const char *fn, int initial_payload_size = 4) {
        if ((fd = open(fn, O_RDWR|O_CREAT|O_CLOEXEC,S_IRUSR|S_IWUSR)) == -1)
            saypx("open");

        struct stat st;
        if (fstat(fd, &st) == -1)
            saypx("stat");

        struct stored s;
        if (st.st_size < sizeof(s)) {
            // zero init
            bzero(&s,sizeof(s));
            s.stats.max_payload_size = initial_payload_size;
            if (write(fd,&s,sizeof(s)) != sizeof(s))
                saypx("failed to create empty list");
            st.st_size = sizeof(s);
        }
        _mmap(st.st_size);
        term = std::string(fn);
        // D("found data with %d payload size, entry size: %lu",stored_payload_size(),sizeof_entry());
        assert(initial_payload_size == stored_payload_size());
        reset();
    }
    ~StoredList() {
        _munmap();
        close(fd);
    }
    void sync(void) {
        if (msync(stored,size,MS_SYNC) == -1)
            saypx("msync");
    }
    u32 count(void) const {
        return stored->stats.n;
    }
    s32 skip_to(s32 id) {
        // try the current position
        // then the next position
        // and then the requested positon
        struct entry *e = entry_at(cursor,id);
        if (e == NULL) {
            e = entry_at(cursor + 1,id);
            if (e != NULL) {
                cursor += 1;
            } else {
                int found;
                u32 where = closest(id,&found,cursor + 1);
                if (found)
                    cursor = where;
                else
                    return NO_MORE;
            }
        }
        return cursor;
    }
    u32 closest(s32 id, int *found,u32 start = 0) {
        u32 end = stored->stats.n;
        u32 mid = end;
        while (start < end) {
            mid = start + (end - start) / 2;
            struct entry *e = entry_at(mid,0);
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
        insert(id,(u8 *)&payload,sizeof(payload));
    }
    void insert(s32 id, float payload) {
        insert(id,(u8 *)&payload,sizeof(payload));
    }
    void insert(s32 id, u8 *buf, u8 len) {
        assert(id >= 0);
        assert(len <= stored_payload_size());
        int found;
        u32 where = closest(id,&found);
        struct entry *e;
        if (!found) {
            int n = stored->stats.n;
            _remap(sizeof_entry(1));
            stored->stats.n++;
            size_t bytes = sizeof_entry(n - where);
            if (bytes > 0)
                memmove((u8 *) entry_at(where + 1), (u8 *)entry_at(where),bytes);
            e = entry_at(where);
            e->id = id;
            e->freq = 0;
            if (id > stored->stats.last_id)
                stored->stats.last_id = id;
        } else {
            e = entry_at(where);
        }
        bcopy(buf,&e->payload[0] ,len);
        if (e->freq < 255)
            e->freq++;
    }
    void dump(void) {
        struct entry *e;
        int i,j = 0;
        for (i = 0; i < count(); i++){
            e = entry_at(i,0);
            printf("pos: %d; id: %d; freq: %d; payload:\n\t",i,e->id,e->freq);
            printf("[");
            for (j = 0; j < stored_payload_size(); j++) {
                printf(" 0x%x  ",e->payload[j]);
            }
            printf("]");
            printf("\n");
        }
    }

    void _mmap(size_t n) {
        stored = (struct stored *) mmap(0, n, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (stored == MAP_FAILED) {
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
        if (asize + n >= size) {
            n += asize + 1000 * sizeof_entry();
            _munmap();
            if (ftruncate(fd,n) == -1)
                saypx("ftruncate");
            _mmap(n);
        }
    }
    u32 stored_payload_size(void) {
        return stored->stats.max_payload_size;
    }
    size_t sizeof_entry(int n = 1) {
        return (sizeof(struct entry) + stored_payload_size()) * n;
    }
    struct entry *entry_at(void) {
        return entry_at(cursor,0);
    }
    struct entry *entry_at(u32 pos,s32 expect = 0) {
        if (pos >= count())
            return NULL;
        struct entry *e = (struct entry *) (stored->begin + (sizeof_entry(pos)));
        if (expect > 0 && e->id != expect)
            return NULL;
        return e;
    }
    s32 current(void) {
        struct entry *e = entry_at(cursor);
        if (e != NULL)
            return e->id;
        return NO_MORE;
    }
    s32 next(void) {
        // hack: cursor holds the current index, not the current doc
        cursor++;
        return current();
    }
    void reset(void) {
        cursor = 0;
    }
};

class Advancable {
public:
    virtual s32 skip_to(s32 id) = 0;
    virtual u32 count(void) const = 0;
    virtual bool score(struct scored *scored,StoredList &documents) = 0;
    virtual s32 next(void) = 0;
    virtual s32 current(void) = 0;
    virtual void reset(void) = 0;
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
    StoredList *list;
    TermQuery(StoredList *list_) : Advancable() {
        list = list_;
        reset();
    }
    s32 skip_to(s32 id) {
        return list->skip_to(id);
    }
    u32 count(void) const {
        return list->count();
    }
    bool score(struct scored *s, StoredList &documents) {
        struct entry *e = list->entry_at();
        if (e == NULL)
            return false;
        s->id = e->id;
        s->score += 1;
        struct entry *doc = documents.entry_at(e->id);
        if (doc != NULL) {
            s->score += *((float *)doc->payload);
        }
        return true;
    }
    s32 current(void) {
        return list->current();
    }
    s32 next(void) {
        return list->next();
    }
    void reset(void) {
        list->reset();
    }
};

class BoolMustQuery : public Advancable {
public:
    s32 cursor;
    std::vector<Advancable *> queries;
    BoolMustQuery() : Advancable() {
        cursor = NO_MORE;
    }
    void add(Advancable *q) {
        queries.push_back(q);
        std::sort(queries.begin(), queries.end(),smallest);
        reset();
    }
    s32 skip_to(s32 id) {
        if (cursor == NO_MORE)
            return NO_MORE;
        for (auto query : queries) {
            if (query->skip_to(id) == NO_MORE) {
                return NO_MORE;
            }
        }
        cursor = id;
        return cursor;
    }
    bool score(struct scored *s, StoredList &documents) {
        // position everything to the id or fail
        if (skip_to(cursor) == NO_MORE)
            return false;
        for (auto query : queries)
            query->score(s,documents);
        return true;
    }
    s32 current(void) {
        return cursor;
    }
    s32 next(void) {
        if (cursor == NO_MORE)
            return NO_MORE;
        cursor = queries[0]->next();
        while (cursor != NO_MORE) {
            for (auto query : queries) {
                if (query->skip_to(cursor) == NO_MORE)
                    goto next;
            }
            break;
        next:
            cursor = queries[0]->next();
        }
        return cursor;
    }

    u32 count(void) const {
        if (queries.size() == 0)
            return 0;
        return queries[0]->count();
    }
    void reset(void) {
        for (auto query : queries)
            query->reset();
        if (queries.size() > 0)
            cursor = queries[0]->current();
        else
            cursor = NO_MORE;
    }
};

class Collector {
public:
    Advancable &query;
    Collector(Advancable &query_) : query(query_) {}
    std::vector<scored> topN(int n_items,StoredList &documents) {
        query.reset();
        std::vector<scored> items;
        struct scored scored,min_item;
        min_item.id = 0;
        min_item.score = 0;
        bool is_heap = false;
        while (query.score(&scored,documents)) {
            if (scored.score > min_item.score) {
                if (items.size() >= n_items) {
                    items.push_back(scored);
                    if (!is_heap) {
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
            scored.score = 0;
            query.next();
        }
        std::sort(items.begin(), items.end(), scored_cmp);
        return items;
    }
};