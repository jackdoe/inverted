#include "list.h"
#include <time.h>
int main(void) {
    // create 5 documents
    StoredList t_everything("everywhere.idx");
    StoredList t_jack("jack.idx");
    StoredList t_doe("doe.idx");
    StoredList t_a("a.idx");
    TermQuery q_everything(&t_everything);
    TermQuery q_jack(&t_jack);
    TermQuery q_doe(&t_doe);
    TermQuery q_a(&t_a);

    BoolMustQuery b;
    b.add(&q_everything);
    b.add(&q_jack);
    b.add(&q_doe);
    b.add(&q_a);
    for (int i = 0;i < 2;i++) {
        clock_t clock_start=clock();
        int total = 0;
        for (auto s : __topN(&b,20,&total)) {
            printf("doc: %d, score: %f\n",s.id,s.score);
        }
        double took = ((double)(clock()- clock_start)/CLOCKS_PER_SEC) * 1000;
        printf("[%f] matches %d\n%s\n",took,total,b.to_string());
    }
}
