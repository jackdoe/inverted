use List::Util qw(shuffle);
use Data::Dumper;
use Time::HiRes qw(time);
use Inverted::StoredList;
my $i = Inverted::StoredList->new();
my @terms = ("jack","doe","foo","bar");

my $t0 = time();
my $n = 0;
my $per_report = 100_000;
srand;
for my $id(1..100_000_000) {
    my @t = ();
=begin
    for my $term(@terms) {
        push @t,$term if (int(rand(2)) == 1);
    }
=cut
    $i->index(\@terms,$id);
    if ($n != 0 && ($n % $per_report) == 0) {

        my $diff = time() - $t0;
        $t0 = time();
        print sprintf "$n: took <%.2f> for %d or %.2f per second\n",$diff,$per_report,$per_report / $diff;
    }
    $n++;
}



my $result = $i->search({
    must => [
        { term => "jack" },
        { term => "doe" },
    ]
});
