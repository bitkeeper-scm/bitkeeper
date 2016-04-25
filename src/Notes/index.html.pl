#!/usr/bin/perl -w

print <<EOF;
<!DOCTYPE html>
<html>
<head>
<script type="text/javascript" src="http://code.jquery.com/jquery-1.11.2.min.js"></script>
<script type="text/javascript" src="http://cdn.ucb.org.br/Scripts/tablesorter/jquery.tablesorter.min.js"></script>
</head>
<body>
<table id="myTable" class="tablesorter">
<thead>
<tr>
    <th>Name</th>
    <th>Description</th>
    <th>Last Modified</th>
</tr>
</thead>
<tbody>

EOF

foreach my $f (@ARGV) {
    ($_ = $f) =~ s/.adoc//;
    print "<tr>\n";
    print "\t<td><A href=\"$_.html\">$_</A></td>\n";

    open(F, $f);
    $_ = <F>;
    close(F);
    print "\t<td>$_</td>\n";
    printf "\t<td>%s</td>\n", `bk prs -hnd':D: :T:' $f`;
    print "</tr>\n";
}

print <<'EOF'
</tbody>
</table>
<script>
$(document).ready(function()
    {
        $("#myTable").tablesorter();
    }
);
</script>
</body>
</html>
EOF
