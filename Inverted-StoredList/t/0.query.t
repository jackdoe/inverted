use strict;
use warnings;
use Test::More tests => 7;
BEGIN { use_ok('Inverted::StoredList') };

my $i = Inverted::StoredList->new();
$i->index(["jack","doe","moe","foo","bar"],3);
$i->index(["jack","doe","bar"],4);
$i->index(["doe","bar","moe"],5);
$i->index(["jack","doe"],6);
my $result = $i->search({
    must => [
        { term => "jack" },
        { term => "doe" },
        {
            should => [
                { term => "bar" },
                {
                    must => [
                        { term => "moe" },
                        { term => "jack" }
                    ]
                }
            ]
        }
    ]
});
is (scalar(@{ $result->{hits}}),2);
is ($result->{total},2);
is ($result->{hits}->[0]->{__id},3);
is ($result->{hits}->[0]->{__score},5);
is ($result->{hits}->[1]->{__id},4);
is ($result->{hits}->[1]->{__score},3);

