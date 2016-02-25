#!/usr/bin/perl -w
# Copyright 2001,2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Program to create graphs for dispalying on bkweb
#
# This program spits out a file that is then read by the 'dot' program
# found in the graphviz suite (http://www.research.att.com/sw/tools/graphviz/)
#
# Call with a filename, else defaults to the cset file
#
# Example: 
#  ./viz_gen.pl | dot -Tgif > /tmp/z2.gif
#
# TODO: Don't even think of doing this on the full bk cset file yet. The
# resulting gif is mongo big.
#

if ($ARGV[0]) {
	$file = $ARGV[0]; 
	open(L,"bk _lines -u -t $file |") or die "Can't open _lines";
} else {
	$file = "ChangeSet";
	open(L,"bk -R _lines -R-1M -n50 -u -t $file |") or die "Can't open _lines";
}

#printf STDERR "file=($file)\n";

# Create header for the dot program
sub header 
{
	print "digraph \"$file\" {\n";
	#print "\trotate=90\n";
	print "\trankdir=LR\n";
	print "\tranksep=.25\n";
	print "\tnode [height=.3,width=.3,shape=box,style=filled,regular=1,color=\".7 .3 1.0\"];\n";
}
sub footer
{
	print "\n\t// Do labels\n\n";
	foreach $lbl (sort keys %label) {
		printf STDOUT "\t%s [fontsize=10,label=\"%s\"];\n",$lbl,$label{$lbl};
	}
	print "}\n";
}

sub genGraph 
{
	&header;
	$line = 1;
	while (<L>) {
		print "\n\t//=================\n";
		@line = split /\s+/, $_;
		print "\t";
		$len = $#line;
		$i = 0;
		$mark = 'no';
		foreach $node (@line) {
			$i++;
			if ($node =~ /\|/) {
				($node, $merge) = split /\|/, $node;
				#print "n=($node) m=($merge) \n";
				@g = split /-/, $node;
				@m = split /-/, $merge;
				$mn{$m[2]} = $g[2];
			} else {
				@g = split /-/, $node;
			}
			$n = $g[2];
			$label{$n} = "$g[0]\\n$g[1]";
			if ($line == 1) {
				printf "$n %s ", $i <= $len ? "->" : "";
			} else {
				if ($mark eq 'no') { 
					printf "$n  ";
					$mark = 'yes';
				} elsif ($mark eq 'yes') {
					print "-> $n ";
					$mark = 'yes';
				}
			}
		}
		$line++;
	}
	print "\n\t//===== Merges =========\n";
	$mark = 'no';
	foreach $n (sort keys %mn) {
		printf "\t%s -> %s\n",$n,$mn{$n};
	}
	&footer;
}

&genGraph;
