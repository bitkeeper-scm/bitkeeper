
// Filter failed bk tests: ./build t | bk tclsh t/failed.l 2>&1 | tee BUGS
//
// This will filter all the failed tests out of the regressions, keeping
// the title and subtitle of test the failure happened.  This is
// to be able to quickly triage and go fix bugs.
//
// alg by rick, code by damon

string line, title, subtitle;

while (defined(line = fgetline(stdin))) {
        if (line =~ /^===/) {
                title = line;
                subtitle = undef;
        } else if (line =~ /^---/) {
                subtitle = line;
        } else if (line =~ /\.\.failed \(bug/) {
                if (defined(title)) puts("\n" . title);
                if (defined(subtitle)) puts(subtitle);
                puts(line);
                title = undef;
                subtitle = undef;
        }
}
