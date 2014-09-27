use 5.012004;
use ExtUtils::MakeMaker;
my $CC = $ENV{"CXX"} || 'g++';
WriteMakefile(
    NAME              => 'Inverted::StoredList',
    VERSION_FROM      => 'lib/Inverted/StoredList.pm',
    PREREQ_PM         => {},
    ($] >= 5.005 ? 
      (ABSTRACT_FROM  => 'lib/Inverted/StoredList.pm',
       AUTHOR         => 'borislav nikolov <jack@sofialondonmoskva.co>') : ()),
    CC            => $CC,
    LD            => '$(CC)',
    LIBS              => [''], # e.g., '-lm'
    DEFINE            => '-g0 -O3', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
    XSOPT             => '-C++',
    TYPEMAPS          => ['perlobject.map','typemap'],
);
