use List::Util qw(shuffle);
use Data::Dumper;
use Time::HiRes qw(time);
use Inverted::StoredList;
my $i = Inverted::StoredList->new();
my @terms = ("jack","doe","foo","bar",'a'..'z');

my $t0 = time();

if (@ARGV && $ARGV[0] eq 'build') {
    my $n = 0;
    my $per_report = 100_000;
    srand;
    for my $id(1..10_000_000) {
        my @t = ("everywhere");
        push @t, $terms[rand(@terms)] for 1..5;
        $i->index(\@t,$id);
        if ($n != 0 && ($n % $per_report) == 0) {

            my $diff = time() - $t0;
            $t0 = time();
            print sprintf "$n: took <%.2f> for %d or %.2f per second\n",$diff,$per_report,$per_report / $diff;

        }
        $n++;
    }
}

while(1) {
    my $result = $i->search({
        must => [
            { term => "jack" },
            { term => "doe" },
            { term => "a" },
            { term => "b" },
            { term => "d" },
            { term => "everywhere" },
            ]
    });
    print Dumper([$result->{took},$result->{total}]);
}
