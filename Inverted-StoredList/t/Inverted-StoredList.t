# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Inverted-StoredList.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Data::Dumper;
use Test::More tests => 7;
BEGIN { use_ok('Inverted::StoredList') };

my $n = Inverted::StoredList->new("new_york.idx","/tmp/",4,32);
is ($n->stored_payload_size(),4);
is (-f $n->get_path(),1);

$n->insert(1);
$n->insert(2);
$n->insert(3);

my $tq = Inverted::StoredList::TermQuery->new($n);
like ($n->get_path(),qr/^\/tmp\/+\d+\/+new_york.idx$/);
my $top = Inverted::StoredList::Search::topN($tq,5);
is (scalar(@{ $top }), 3);

my $second = Inverted::StoredList->new("state.idx","/tmp/",4,32);
$second->insert(2);
my $secondq = Inverted::StoredList::TermQuery->new($second);

my $bq = Inverted::StoredList::BoolShouldQuery->new();
$bq->add($tq);
$bq->add($secondq);

my $tq2 = Inverted::StoredList::TermQuery->new($second);
$top = Inverted::StoredList::Search::topN($bq,5);
is (scalar(@{ $top }), 3);
is ($top->[0]->{__id}, 2);

unlink ($n->get_path());
unlink ($second->get_path());

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

