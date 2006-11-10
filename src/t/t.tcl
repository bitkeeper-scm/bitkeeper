echo $N Test Tcl encrpytion/decryption ..............................$NL
cd "$HERE"
TCLWRAP="`bk bin`"/tclwrap
test -x "$TCLWRAP" || {
	echo failed
	echo "$TCLWRAP not found or not an executable"
	exit 1
}
echo 'IT WORKS' > expected
for i in 2 3 5 7 11 13 17 19 23 29 31 37 41
do	echo 'fconfigure stdout -translation lf' > clear.tcl
	j=0
	while [ $j -lt $i ]
	do	echo "	# comment" >> clear.tcl
		j=`expr $j + 1`
	done
	cat >> clear.tcl <<'EOF'
		puts "IT WORKS"
EOF
	$TCLWRAP < clear.tcl > crypted.tcl 2>${DEV_NULL}
	bk _tclsh crypted.tcl 2>ERR >result
	checkfiles expected result
done
echo OK

# This next test exposes a bug that occurs when Tcl is compiled with 
# GCC 2.7.2. If this regression fails, you need to switch compilers
# to GCC-2.95.3 or later.
echo $N Test Tcl/GCC compiler interaction \(AKA format bug\) ..........$NL
cd "$HERE"
echo '1 1 1 1' > expected
bk _tclsh 2>ERR >result <<'EOF'
    fconfigure stdout -translation lf

    set a 0xaaaaaaaa
    # Ensure $a and $b are separate objects
    set b 0xaaaa
    append b aaaa

    set result [expr {$a == $b}]
    format %08lx $b
    lappend result [expr {$a == $b}]

    set b 0xaaaa
    append b aaaa

    lappend result [expr {$a == $b}]
    format %08x $b
    lappend result [expr {$a == $b}]
    puts $result

EOF
checkfiles expected result
echo OK
