BEGIN { 
	filter = 0
	matches = 0
	skip = 0
}

$0 ~ /=head1 Name/ {
	filter = 1
}
$0 ~ /Little/ {
	if (filter) {
		matches += 1
		if (matches == 2) {
			gsub(/Little/, "bk little, or Little,")
		}
	}
}
$0 ~ /L .options. progname.l/ {
	gsub(/L/, "bk little")
}
$0 ~ /=head2 pod2html.l/ { skip = 1 }
{ if (skip == 0) print }
