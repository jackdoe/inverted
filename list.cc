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
    u16 freq;
    u16 plen;
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
        if ((this->fd = open(fn, O_RDWR|O_CREAT|O_CLOEXEC,S_IRUSR|S_IWUSR)) == -1)
            saypx("open");

        struct stat st;
        if (fstat(this->fd, &st) == -1)
            saypx("stat");

        struct stored s;
        if (st.st_size < sizeof(s)) {
            // zero init
            bzero(&s,sizeof(s));
            s.stats.max_payload_size = initial_payload_size;
            if (write(this->fd,&s,sizeof(s)) != sizeof(s))
                saypx("failed to create empty list");
            st.st_size = sizeof(s);
        }
        this->_mmap(st.st_size);
        this->term = std::string(fn);
        // D("found data with %d payload size, entry size: %lu",this->stored_payload_size(),sizeof_entry());
        assert(initial_payload_size == this->stored_payload_size());
        this->cursor = 0;
    }
    ~StoredList() {
        this->_munmap();
        close(this->fd);
    }
    std::string toString() {
        return std::string("term: ") + this->term;
    }
    void sync(void) {
        if (msync(this->stored,this->size,MS_SYNC) == -1)
            saypx("msync");
    }
    u32 count(void) const {
        return this->stored->stats.n;
    }
    s32 skip_to(s32 id) {
        // try the current position
        // then the next position
        // and then the requested positon
        struct entry *e = entry_at(this->cursor,id);
        if (e == NULL) {
            e = this->entry_at(this->cursor + 1,id);
            if (e != NULL) {
                this->cursor += 1;
            } else {
                int found;
                u32 where = this->closest(id,&found);
                if (found)
                    this->cursor = where;
                else
                    return NO_MORE;
            }
        }
        return this->cursor;
    }
    u32 closest(s32 id, int *found) {
        u32 start = 0;
        u32 end = this->stored->stats.n;
        u32 mid = end;
        while (start < end) {
            mid = start + (end - start) / 2;
            struct entry *e = this->entry_at(mid,0);
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
        this->insert(id,(u8 *)&payload,sizeof(payload));
    }
    void insert(s32 id, float payload) {
        this->insert(id,(u8 *)&payload,sizeof(payload));
    }
    void insert(s32 id, u8 *buf, u8 len) {
        assert(id >= 0);
        assert(len <= this->stored_payload_size());
        int found;
        u32 where = this->closest(id,&found);
        struct entry *e;
        if (!found) {
            int n = this->stored->stats.n;
            this->_remap(this->sizeof_entry(1));
            this->stored->stats.n++;
            size_t bytes = this->sizeof_entry(n - where);
            if (bytes > 0)
                memmove((u8 *) this->entry_at(where + 1), (u8 *)this->entry_at(where),bytes);
            e = this->entry_at(where);
            e->id = id;
            e->freq = 0;
            if (id > this->stored->stats.last_id)
                this->stored->stats.last_id = id;
        } else {
            e = this->entry_at(where);
        }
        e->plen = len;
        bcopy(buf,&e->payload[0] ,len);
        e->freq++;
    }
    void dump(void) {
        struct entry *e;
        int i,j = 0;
        for (i = 0; i < this->count(); i++){
            e = this->entry_at(i,0);
            printf("pos: %d; id: %d; freq: %d; plen: %d; payload:\n\t",i,e->id,e->freq,e->plen);
            printf("[");
            for (j = 0; j < e->plen; j++) {
                printf(" 0x%x  ",e->payload[j]);
            }
            printf("]");
            printf("\n");
        }
    }

    void _mmap(size_t n) {
        this->stored = (struct stored *) mmap(0, n, PROT_READ|PROT_WRITE, MAP_SHARED, this->fd, 0);
        if (this->stored == MAP_FAILED) {
            close(this->fd);
            saypx("mmap");
        }
        this->size = n;
    }
    void _munmap(void) {
        if (this->stored != NULL) {
            if (munmap(this->stored, this->size) == -1)
                saypx("munmap");
        }
        this->stored = NULL;
    }
    void _remap(int n) {
        assert(this->stored != NULL);
        u32 asize = sizeof(struct stored) + this->sizeof_entry(this->stored->stats.n);
        if (asize + n >= this->size) {
            n += asize + 1000 * this->sizeof_entry();
            this->_munmap();
            if (ftruncate(this->fd,n) == -1)
                saypx("ftruncate");
            this->_mmap(n);
        }
    }
    u32 stored_payload_size(void) {
        return this->stored->stats.max_payload_size;
    }
    size_t sizeof_entry(int n = 1) {
        return (sizeof(struct entry) + this->stored_payload_size()) * n;
    }
    struct entry *entry_at(void) {
        return entry_at(cursor,0);
    }
    struct entry *entry_at(u32 pos,s32 expect = 0) {
        if (pos >= this->count())
            return NULL;
        struct entry *e = (struct entry *) (this->stored->begin + (this->sizeof_entry(pos)));
        assert (e->plen <= stored_payload_size());
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
};

class Advancable {
public:
    virtual s32 skip_to(s32 id) = 0;
    virtual u32 count(void) const = 0;
    virtual bool score(struct scored *scored,StoredList &documents) = 0;
    virtual s32 next(void) = 0;
    virtual s32 current(void) = 0;
    virtual std::string toString() = 0;
};

bool smallest (const Advancable *a, const Advancable *b) {
    return a->count() < b->count();
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
    std::string toString() {
        return list->toString();
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
        cursor = queries[0]->current();
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

    std::string toString() {
        std::string r = "bool: (";
        for (auto query : queries)
            r += std::string(" ") + query->toString() + std::string(" ");
        r += ")";
        return r;
    }
};

class Collector {
public:
    Advancable &query;
    Collector(Advancable &query_) : query(query_) {}
    std::vector<scored> topN(int n_items,StoredList &documents) {
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

int main(void) {
    // create 5 documents
    StoredList documents("_documents.idx");
    documents.insert(1,0.100f);
    documents.insert(2,0.150f);
    documents.insert(3,0.130f);
    documents.insert(4,0.140f);
    documents.insert(5,0.160f);

    // add some terms
    StoredList t_new("_new.idx");
    t_new.insert(1);
    t_new.insert(2);
    t_new.insert(3);
    t_new.insert(4);
    t_new.insert(5);
    StoredList t_york("_york.idx");
    t_york.insert(1);
    t_york.insert(2);
    t_york.insert(3);
    StoredList t_state("_state.idx");
    t_state.insert(2);
    t_state.insert(3);

    TermQuery q_new(&t_new);
    TermQuery q_york(&t_york);
    TermQuery q_state(&t_state);

    BoolMustQuery q_new_york;
    BoolMustQuery q_new_york_state;
    q_new_york.add(&q_new);
    q_new_york.add(&q_york);
    q_new_york_state.add(&q_new_york);
    q_new_york_state.add(&q_state);
    Collector c(q_new_york_state);
    for (auto s : c.topN(3,documents)) {
        printf("doc: %d, score: %f\n",s.id,s.score);
    }
}
