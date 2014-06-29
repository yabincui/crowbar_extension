crowbar_extension
=================

implement crowbar script language and extend it in some areas.

crowbar_extension is a script language interpreter used for crowbar language.



INSTALL

crowbar_extension uses flex for lexical analyzer generation and bison for syntax 
parser generation. In the meantime, it uses oniguruma regular expressions library.

To install it, you should make sure you have flex, bison and oniguruma regular library
installed. Their versions on my environment is:

flex 2.5.35
bison 2.5
oniguruma 5.9.5

As it is simple, you don't need to configure or make install it, you can just use
'make' and 'make clean'.

