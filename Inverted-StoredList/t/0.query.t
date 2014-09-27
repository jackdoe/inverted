use strict;
use warnings;
use Test::More tests => 6;
BEGIN { use_ok('Inverted::StoredList') };

my $i = Inverted::StoredList->new();
$i->index(["jack","doe","moe","foo","bar"],3);
$i->index(["jack","doe","bar"],4);
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
    
is (scalar(@$result),2);
is ($result->[0]->{__id},3);
is ($result->[0]->{__score},5);
is ($result->[1]->{__id},4);
is ($result->[1]->{__score},3);

