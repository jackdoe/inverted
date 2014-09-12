#ifdef __cplusplus
#include "../list.h"
extern "C" {
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#ifdef __cplusplus
}
#endif



MODULE = Inverted::StoredList		PACKAGE = Inverted::StoredList

StoredList *
StoredList::new( char *name, char *root, int default_payload_size,int buckets )

char *
StoredList::get_path()

int
StoredList::stored_payload_size()

int
StoredList::count()

void
StoredList::insert(int id)

void
StoredList::remove(int id)

void
StoredList::DESTROY()


MODULE = Inverted::StoredList		PACKAGE = Inverted::StoredList::TermQuery

TermQuery *
TermQuery::new(StoredList *_list)

void
TermQuery::DESTROY()


MODULE = Inverted::StoredList		PACKAGE = Inverted::StoredList::BoolMustQuery

BoolMustQuery *
BoolMustQuery::new()

void
BoolMustQuery::DESTROY()

void
BoolMustQuery::add(Advancable *query)


MODULE = Inverted::StoredList		PACKAGE = Inverted::StoredList::Search
AV *topN(Advancable *query, int n)
    CODE:
        RETVAL = newAV();
        sv_2mortal( (SV*)RETVAL );
        for (auto s : __topN(query,n)) {
            HV *rh;
            rh = (HV *)sv_2mortal((SV *)newHV());
            hv_store(rh,"__score",7,newSVnv(s.score),0);
            hv_store(rh,"__id",4,newSVnv(s.id),0);
            av_push(RETVAL, newRV((SV *)rh));
        }
    OUTPUT:
        RETVAL