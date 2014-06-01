extremely naive inverted index implementation that uses only stored and mmaped array.

index format:

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

the payload size is decided once per index
creation, and it is the same for every entry
in the array.

for example:

 StoredList documents("_documents.idx",128);

 // creating documents 1 2 3 (with payload 0 - which will use sizeof(int) bytes)
 documents.insert(1)
 documents.insert(2)
 documents.insert(3)

the Forward and the Inverted indexes are both just StoredLists
 // adding the term 'new' to documents 1,2,3
 StoredList t_new("_new.idx");
 t_new.insert(1);
 t_new.insert(2);
 t_new.insert(3);

 StoredList t_new("_york.idx");
 t_york.insert(1);
 t_york.insert(3);

 StoredList t_state("_state.idx");
 t_york.insert(3);

fetching entry from the forward index:
 documents.entry_at(3)

simple query:
 TermQuery q_new(&t_new);
 TermQuery q_york(&t_york);
 TermQuery q_state(&t_state);

 BoolMustQuery q_new_york;
 BoolMustQuery q_new_york_state;
 q_new_york.add(&q_new);
 q_new_york.add(&q_york);
 q_new_york_state.add(&q_new_york);
 q_new_york_state.add(&q_state);
 Collector collector(q_new_york_state);
 for (auto s : collector.topN(3,documents)) {
     printf("doc: %d, score: %f\n",s.id,s.score);
 }

 
