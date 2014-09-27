package Inverted::StoredList;

use 5.012004;
use strict;
use warnings;

our @ISA = qw();

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Inverted::StoredList', $VERSION);

sub new {
    my ($klass,$params) = @_;
    $params ||= {};
    $params->{root} ||= '/tmp/';
    $params->{stored_payload_size} ||= 4;
    $params->{directory_split} ||= 32;
    my $self = bless {
        params => $params,
        terms  => {},
    }, $klass;
    return $self;
}

sub term {
    my ($self,$term) = @_;
    $term = $self->{terms}{$term} ||= Inverted::StoredList::MMAP->new(
        "$term.idx",
        $self->{params}{root},
        $self->{params}{stored_payload_size},
        $self->{params}{directory_split}
    );
    return $term;
}

sub index {
    my ($self,$terms,$doc_id) = @_;
    $terms = [ $terms ] unless ref($terms) eq 'ARRAY';
    for my $t(@{ $terms }) {
        $self->term($t)->insert($doc_id);
    }
}
=begin
search({
    must => [
        { term => "jack" },
        { should => [ { term => "doe" }, { term => "moe" } ] },
    ],
    term => "foo"
}

=cut
use Data::Dumper;
sub _generate {
    my ($self,$top,$query,$ref_cnt_hack) = @_;
    for my $k(keys(%{ $query })) {
        my $internal = undef;
        if ($k eq 'term') {
            $internal = Inverted::StoredList::TermQuery->new($self->term($query->{$k}));
        } elsif ($k eq 'should' || $k eq 'must') {
            $internal = $k eq 'should' ? 
                Inverted::StoredList::BoolShouldQuery->new() :
                Inverted::StoredList::BoolMustQuery->new();

            for my $s(@{ $query->{$k} }) {
                $self->_generate($internal,$s,$ref_cnt_hack);
            }
        } else {
            die "dont know what to do with <$k>";
        }
        $ref_cnt_hack->{$internal} = $internal;
        $top->add($internal);
    }
}

sub search {
    my ($self,$query,$n) = @_;
    $n ||= 20;
    my $ref_cnt_hack = {}; # ->add doesnt increment the refcnt, so just store everything in a hash
    my $top = Inverted::StoredList::BoolMustQuery->new();
    $self->_generate($top,$query,$ref_cnt_hack);
    my $results = Inverted::StoredList::Search::topN($top,$n);
    %$ref_cnt_hack = ();
    return $results;
}

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Inverted::StoredList - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Inverted::StoredList;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Inverted::StoredList, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.


=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

jack doe, E<lt>jack@apple.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2014 by jack doe

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.12.4 or,
at your option, any later version of Perl 5 you may have available.


=cut
