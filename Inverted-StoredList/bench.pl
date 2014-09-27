use List::Util qw(shuffle);
use Data::Dumper;
use Time::HiRes qw(time);
use Inverted::StoredList;
my $i = Inverted::StoredList->new();
my @terms = ("jack","doe","foo","bar");

my $t0 = time();

if (@ARGV && $ARGV[0] eq 'build') {
    my $n = 0;
    my $per_report = 100_000;
    srand;
    for my $id(1..100_000_000) {
        my @t = ("everywhere");

        for my $term(@terms) {
            push @t,$term if (int(rand(100)) == 1);
        }

        $i->index(\@t,$id) if @t;
        if ($n != 0 && ($n % $per_report) == 0) {

            my $diff = time() - $t0;
            $t0 = time();
            print sprintf "$n: took <%.2f> for %d or %.2f per second\n",$diff,$per_report,$per_report / $diff;

        }
        $n++;
    }
}

$t0 = time();
my $result = $i->search({
    must => [
        { term => "jack" },
        { term => "doe" },
    ]
});
print Dumper([time() - $t0,$result]);


$t0 = time();
my $result = $i->search({
    must => [
        { term => "jack" },
        { term => "doe" },
    ]
});
print Dumper([time() - $t0,$result]);
