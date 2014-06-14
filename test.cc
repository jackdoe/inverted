#include "list.h"

int main(void) {
    // create 5 documents
    StoredList documents("_documents.idx");
    documents.insert(1,0.100f);
    documents.insert(2,0.150f);
    documents.insert(3,0.130f);
    documents.insert(4,0.140f);
    documents.insert(5,0.160f);
    documents.remove(4);
    documents.insert(6,0.160f);
    documents.dump();
    // add some terms
    StoredList t_new("_new.idx");
    t_new.insert(1);
    t_new.insert(2);
    t_new.insert(3);
    t_new.insert(4);
    t_new.insert(5);
    t_new.remove(4);
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
